"""Lean, single-worker exact-route replay. ESP32 owns motors/watchdog/obstacle stop."""
import asyncio
import hashlib
import hmac
import json
import secrets
import time
from datetime import datetime, timezone, timedelta
from uuid import UUID, uuid4, uuid5
from fastapi import APIRouter, Depends, HTTPException, Request
from pydantic import Field
from sqlalchemy.exc import IntegrityError
from database import get_db, SessionLocal, BookModel, ShelfModel, RobotModel, RouteModel
from atlas_schemas import StrictModel, Movement
from atlas_validation import checkpoint, route_data, validate_route
from guidance_models import GuidanceTask, ACTIVE, CLAIMED
from guidance_access import reader, staff, device, enabled, available
from auth_security import throttle
from auth_config import get_config
import atlas_runtime as runtime

router = APIRouter(tags=['Atlas guidance'])
POLL_TIMEOUT = 5
LEASE_SECONDS = 1.5
SEGMENT_TIMEOUT = 60
CONFIRM_TIMEOUT = 120
# Temporary delivery/ack state only. No permanent command/sample tables.
deliveries = {}
# Display plaintext and physical attestations are short-lived, never stored in DB.
otp_displays = {}
arrivals = {}

def now():
    return datetime.now(timezone.utc)

def stamp(value):
    return value.replace(tzinfo=timezone.utc) if value.tzinfo is None else value

def fingerprint(route):
    data = route_data(route).model_dump(mode='json')
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()

def active(db, robot_id=None):
    query = db.query(GuidanceTask).filter(GuidanceTask.status.in_(ACTIVE))
    return query.filter(GuidanceTask.robot_id == robot_id, GuidanceTask.status.in_(CLAIMED)).first() if robot_id else query.all()

def current_route(db, task):
    return db.get(RouteModel, task.return_route_id if task.status in ('returning_home', 'completed') else task.route_id)

def command_id(task, route, sequence):
    return str(uuid5(UUID(task.session_id), f'{route.id}:{sequence}'))

def otp_digest(task, code):
    binding = f'atlas-otp:{task.id}:{task.robot_id}:{task.route_id}:{task.route_fingerprint}:{stamp(task.otp_expires_at).isoformat()}:{code}'
    return hmac.new(get_config().security_secret.encode(), binding.encode(), hashlib.sha256).hexdigest()

def clear_otp(task):
    task.atlas_otp_hash = None
    otp_displays.pop(task.id, None)

def public(task, db):
    route = current_route(db, task)
    outbound = db.get(RouteModel, task.route_id)
    robot = db.get(RobotModel, task.robot_id)
    shelf = db.get(ShelfModel, task.shelf_id)
    return dict(id=task.id, book_id=task.book_id, robot_id=task.robot_id, route_id=task.route_id,
        shelf_name=shelf.shelf_name if shelf else None, status=task.status, progress_index=task.progress_index,
        segment_count=len(route.route_segments) if route else 0, failure_reason=task.failure_reason,
        awaiting_confirmation=bool(route and task.status in ('running', 'returning_home') and task.progress_index == len(route.route_segments)),
        destination_checkpoint_id=outbound.destination_checkpoint_id if outbound else None,
        robot_state=robot.guidance_state if robot else 'unknown', session_id=task.session_id,
        current_checkpoint_id=route.destination_checkpoint_id if route else None,
        home_checkpoint_id=robot.home_checkpoint_id if robot else None,
        phase='return' if task.status == 'returning_home' else 'guidance',
        otp_expires_at=stamp(task.otp_expires_at).isoformat() if task.status == 'awaiting_otp' and task.otp_expires_at else None,
        created_at=stamp(task.created_at).isoformat(), updated_at=stamp(task.updated_at).isoformat())

def fail(db, task, reason, status='failed', preserve_home=False):
    claimed = task.status in CLAIMED
    task.status, task.failure_reason, task.updated_at = status, reason, now()
    clear_otp(task)
    state = runtime.state(task.robot_id)
    robot = db.get(RobotModel, task.robot_id)
    if claimed:
        if not preserve_home:
            state.update(checkpoint_location_verified=False, guidance_confirmed_at=0)
        state['guidance_task_id'] = None
        robot.guidance_state = 'at_home' if preserve_home and state['checkpoint_location_verified'] else 'fault'
        runtime.submit(task.robot_id, 'stop', safety=True)
    deliveries.pop(task.id, None)
    arrivals.pop(task.id, None)
    db.commit()

def check_task(db, task):
    route = current_route(db, task)
    outbound = db.get(RouteModel, task.route_id)
    returning = db.get(RouteModel, task.return_route_id) if task.return_route_id else None
    try:
        if not outbound or outbound.status != 'active' or fingerprint(outbound) != task.route_fingerprint or not returning or returning.status != 'active' or fingerprint(returning) != task.return_route_fingerprint:
            raise HTTPException(409, 'Selected route changed or is no longer active.')
        robot = db.get(RobotModel, task.robot_id)
        if robot.home_checkpoint_id != outbound.start_checkpoint_id or returning.start_checkpoint_id != outbound.destination_checkpoint_id or returning.destination_checkpoint_id != robot.home_checkpoint_id:
            raise HTTPException(409, 'Home or return route configuration changed.')
        validate_route(db, route_data(outbound))
        validate_route(db, route_data(returning))
        shelf = db.get(ShelfModel, task.shelf_id)
        book = db.get(BookModel, task.book_id)
        if not shelf or not book or book.shelf_id != shelf.shelf_id or shelf.destination_checkpoint_id != outbound.destination_checkpoint_id:
            raise HTTPException(409, 'Book or shelf destination changed.')
    except HTTPException as error:
        fail(db, task, str(error.detail))
        return None
    return route

def sweep(db):
    for task in active(db):
        state = runtime.state(task.robot_id)
        if task.status == 'waiting_for_atlas':
            if (now() - stamp(task.created_at)).total_seconds() > 900:
                fail(db, task, 'Request expired while waiting for Atlas.', 'expired')
            else:
                check_task(db, task)
            continue
        state['guidance_task_id'] = task.id
        delivery = deliveries.get(task.id)
        reason = None
        if task.status == 'awaiting_otp' and (not task.otp_expires_at or now() >= stamp(task.otp_expires_at)):
            fail(db, task, 'Atlas code expired. Request guidance again.', 'expired', preserve_home=True)
            continue
        if task.status == 'awaiting_otp' and (not state['checkpoint_location_verified'] or not state.get('guidance_stopped')):
            reason = 'Atlas left its confirmed home station before authorization.'
        elif (now() - stamp(task.created_at)).total_seconds() > 900:
            reason = 'Guidance time limit reached; staff must check Atlas.'
        elif time.time() - state.get('guidance_poll_at', 0) > (LEASE_SECONDS if delivery else POLL_TIMEOUT):
            reason = 'Robot connection lost; physical position must be reconfirmed.'
        elif delivery and time.time() - delivery['issued_at'] > SEGMENT_TIMEOUT:
            reason = 'Segment acknowledgement timed out.'
        elif not delivery and (now() - stamp(task.updated_at)).total_seconds() > CONFIRM_TIMEOUT:
            reason = 'Robot preparation or destination confirmation timed out.'
        if reason:
            fail(db, task, reason)
        else:
            check_task(db, task)

def recover(db):
    deliveries.clear()
    otp_displays.clear()
    arrivals.clear()
    for task in active(db):
        fail(db, task, 'Server restarted; replay will not resume automatically.')
    for robot in db.query(RobotModel):
        robot.guidance_state = 'unknown'
    db.commit()

def select_next(db, robot):
    state = runtime.state(robot.id)
    if active(db, robot.id) or robot.guidance_state != 'at_home' or not state['checkpoint_location_verified'] or not state.get('guidance_stopped') or state['recording_id']:
        return
    if robot.residing_checkpoint_id != robot.home_checkpoint_id:
        return
    command = runtime.pending_commands.get(state['active_command_id'])
    if command and command['command'] != 'stop' and command['status'] not in ('completed', 'failed'):
        return
    task = db.query(GuidanceTask).filter_by(robot_id=robot.id, status='waiting_for_atlas').order_by(GuidanceTask.created_at, GuidanceTask.id).with_for_update().first()
    if not task or not check_task(db, task):
        return
    code = f'{secrets.randbelow(1000000):06d}'
    task.otp_expires_at, task.otp_attempts = now() + timedelta(seconds=60), 0
    task.atlas_otp_hash = otp_digest(task, code)
    task.status, task.updated_at = 'awaiting_otp', now()
    robot.guidance_state = 'awaiting_otp'
    state['guidance_task_id'] = task.id
    db.commit()
    otp_displays[task.id] = code

async def watchdog():
    while True:
        await asyncio.sleep(.5)
        if enabled():
            # SQL/provider failures must not keep the HTTP service dispatching.
            # Firmware MUST independently stop on loss of successful heartbeat.
            try:
                with runtime.lock, SessionLocal() as db:
                    sweep(db)
            except Exception:
                import logging
                logging.getLogger(__name__).exception('Guidance watchdog failed; device watchdog must stop motors.')
                with runtime.lock:
                    for state in runtime.robots.values():
                        state['checkpoint_location_verified'] = False
                        state['guidance_poll_at'] = 0

class GuidanceRequest(StrictModel):
    book_id: int = Field(strict=True, gt=0)

class ShelfDestination(StrictModel):
    destination_checkpoint_id: UUID | None

class PositionConfirmation(StrictModel):
    checkpoint_id: UUID
    operator_confirmed: bool
    task_id: UUID | None = None

class ReplayResult(StrictModel):
    task_id: UUID
    session_id: UUID
    route_id: UUID
    command_id: UUID
    sequence: int = Field(strict=True, gt=0)
    status: str = Field(pattern='^(accepted|started|completed|failed|paused|resumed)$')
    control_request_id: UUID | None = None
    movement: Movement | None = None
    error: str | None = Field(default=None, max_length=300)

class AtlasVerification(StrictModel):
    atlas_otp: str = Field(pattern=r'^\d{6}$')

class HomeConfiguration(StrictModel):
    home_checkpoint_id: UUID

class ArrivalReport(StrictModel):
    task_id: UUID
    session_id: UUID
    route_id: UUID
    checkpoint_id: UUID
    stopped: bool
    confirmation_method: str = Field(pattern='^operator_attestation$')

class ReplayControl(StrictModel):
    task_id: UUID
    session_id: UUID
    route_id: UUID
    command_id: UUID
    sequence: int = Field(strict=True, gt=0)
    request_id: UUID
    action: str = Field(pattern='^(pause|resume)$')

@router.post('/robots/{robot_id}/guidance-control')
def replay_control(robot_id: str, body: ReplayControl, context=Depends(staff), db=Depends(get_db)):
    with runtime.lock:
        sweep(db)
        task = active(db, robot_id)
        route = current_route(db, task) if task else None
        delivery = deliveries.get(task.id) if task else None
        if (not task or not route or not delivery or delivery.get('protocol') != 3 or
            str(body.task_id) != task.id or str(body.session_id) != task.session_id or
            str(body.route_id) != route.id or task.progress_index >= len(route.route_segments) or
            body.sequence != route.route_segments[task.progress_index]['sequence'] or
            str(body.command_id) != command_id(task, route, body.sequence)):
            raise HTTPException(409, 'No matching live protocol 3 segment.')
        identifier = str(body.request_id)
        controls = delivery.setdefault('controls', {})
        if identifier in controls:
            if controls[identifier]['action'] != body.action:
                raise HTTPException(409, 'Conflicting control UUID.')
            return controls[identifier]
        if delivery.get('control') or len(controls) >= 32:
            raise HTTPException(409, 'Control pending or control limit reached.')
        expected = 'paused' if body.action == 'resume' else 'started'
        if delivery['status'] != expected:
            raise HTTPException(409, 'Pause requires running; resume requires acknowledged pause.')
        control = dict(request_id=identifier, action=body.action, acknowledged=False)
        controls[identifier] = control
        delivery['control'] = control
        return control

@router.patch('/shelves/{shelf_id}/destination')
def map_shelf(shelf_id: int, body: ShelfDestination, context=Depends(staff), db=Depends(get_db)):
    with runtime.lock:
        shelf = db.get(ShelfModel, shelf_id)
        if not shelf:
            raise HTTPException(404, 'Shelf not found.')
        if body.destination_checkpoint_id:
            checkpoint(db, body.destination_checkpoint_id, active=True)
        shelf.destination_checkpoint_id = str(body.destination_checkpoint_id) if body.destination_checkpoint_id else None
        db.commit()
        sweep(db)
        return {'destination_checkpoint_id': shelf.destination_checkpoint_id}

@router.post('/guidance-requests', status_code=201)
def request_guidance(body: GuidanceRequest, request: Request, context=Depends(reader), db=Depends(get_db)):
    throttle(db, request, 'guidance', context.email)
    with runtime.lock:
        sweep(db)
        existing = db.query(GuidanceTask).filter(GuidanceTask.user_id == context.user_id, GuidanceTask.status.in_(ACTIVE)).first()
        if existing:
            if existing.book_id == body.book_id:
                return public(existing, db)
            raise HTTPException(409, 'You already have an active guidance request.')
        book = db.get(BookModel, body.book_id)
        shelf = db.get(ShelfModel, book.shelf_id) if book else None
        if not shelf or not shelf.destination_checkpoint_id:
            raise HTTPException(409, 'Guidance is unavailable: this book has no mapped shelf destination.')
        checkpoint(db, shelf.destination_checkpoint_id, active=True)
        robots = db.query(RobotModel).order_by(RobotModel.id).with_for_update().all()
        for robot in robots:
            state = runtime.state(robot.id)
            if not robot.home_checkpoint_id or state['recording_id']:
                continue
            candidates = db.query(RouteModel).filter_by(robot_id=robot.id, status='active', start_checkpoint_id=robot.home_checkpoint_id, destination_checkpoint_id=shelf.destination_checkpoint_id).order_by(RouteModel.id).all()
            returns = db.query(RouteModel).filter_by(robot_id=robot.id, status='active', start_checkpoint_id=shelf.destination_checkpoint_id, destination_checkpoint_id=robot.home_checkpoint_id).order_by(RouteModel.id).all()
            for route in candidates:
                try:
                    validate_route(db, route_data(route))
                except HTTPException:
                    continue
                return_route = None
                for candidate in returns:
                    try:
                        validate_route(db, route_data(candidate))
                        return_route = candidate
                        break
                    except HTTPException:
                        continue
                if not return_route:
                    continue
                task = GuidanceTask(user_id=context.user_id, book_id=book.id, shelf_id=shelf.shelf_id,
                    robot_id=robot.id, route_id=route.id, route_fingerprint=fingerprint(route),
                    return_route_id=return_route.id, return_route_fingerprint=fingerprint(return_route), status='waiting_for_atlas')
                db.add(task)
                try:
                    db.flush()
                    db.commit()
                except IntegrityError:
                    db.rollback()
                    raise HTTPException(409, 'Robot or account already has an active guidance task.') from None
                return public(task, db)
        raise HTTPException(409, 'Guidance requires a configured home and active recorded routes both to this shelf and back home.')

@router.post('/guidance-requests/{task_id}/verify')
def verify_atlas(task_id: UUID, body: AtlasVerification, request: Request, context=Depends(reader), db=Depends(get_db)):
    throttle(db, request, 'atlas-otp', context.email)
    with runtime.lock:
        sweep(db)
        task = db.query(GuidanceTask).filter_by(id=str(task_id), user_id=context.user_id).with_for_update().first()
        if not task:
            raise HTTPException(404, 'Guidance request not found.')
        robot = db.query(RobotModel).filter_by(id=task.robot_id).with_for_update().one()
        state = runtime.state(robot.id)
        if (task.status != 'awaiting_otp' or not task.atlas_otp_hash or task.verified_at or
            not task.otp_expires_at or now() >= stamp(task.otp_expires_at) or not active(db, robot.id) or active(db, robot.id).id != task.id or
            robot.guidance_state != 'awaiting_otp' or robot.residing_checkpoint_id != robot.home_checkpoint_id or
            not state['checkpoint_location_verified'] or not state.get('guidance_stopped') or
            time.time() - state.get('guidance_poll_at', 0) > POLL_TIMEOUT):
            raise HTTPException(409, 'Atlas authorization is no longer available. Request guidance again.')
        if not hmac.compare_digest(task.atlas_otp_hash, otp_digest(task, body.atlas_otp)):
            task.otp_attempts += 1
            if task.otp_attempts >= 5:
                fail(db, task, 'Atlas code attempt limit reached.', 'expired', preserve_home=True)
            else:
                db.commit()
            raise HTTPException(400, 'Invalid Atlas code.')
        task.verified_at, task.session_id, task.status, task.updated_at = now(), str(uuid4()), 'queued', now()
        clear_otp(task)
        robot.guidance_state = 'guiding'
        db.commit()
        return public(task, db)

@router.get('/guidance-requests')
def my_tasks(context=Depends(reader), db=Depends(get_db)):
    with runtime.lock:
        sweep(db)
        return [public(task, db) for task in db.query(GuidanceTask).filter_by(user_id=context.user_id).order_by(GuidanceTask.created_at.desc()).limit(20)]

@router.get('/guidance-requests/{task_id}')
def my_task(task_id: UUID, context=Depends(reader), db=Depends(get_db)):
    with runtime.lock:
        sweep(db)
        task = db.get(GuidanceTask, str(task_id))
        if not task or task.user_id != context.user_id:
            raise HTTPException(404, 'Guidance request not found.')
        return public(task, db)

@router.post('/guidance-requests/{task_id}/cancel')
def cancel(task_id: UUID, context=Depends(reader), db=Depends(get_db)):
    with runtime.lock:
        task = db.get(GuidanceTask, str(task_id))
        if not task or task.user_id != context.user_id:
            raise HTTPException(404, 'Guidance request not found.')
        # Once offered to firmware, it may have started even if the ack was lost.
        if task.status not in ('waiting_for_atlas', 'awaiting_otp', 'queued'):
            raise HTTPException(409, 'Robot may already be moving. Ask staff to stop it.')
        fail(db, task, 'Cancelled by reader before delivery.', 'cancelled', preserve_home=True)
        return public(task, db)

@router.get('/robots/{robot_id}/guidance-tasks')
def robot_tasks(robot_id: str, context=Depends(staff), db=Depends(get_db)):
    with runtime.lock:
        sweep(db)
        return [public(task, db) for task in db.query(GuidanceTask).filter_by(robot_id=robot_id).order_by(GuidanceTask.created_at.desc()).limit(20)]

@router.post('/robots/{robot_id}/guidance-stop')
def stop(robot_id: str, context=Depends(staff), db=Depends(get_db)):
    with runtime.lock:
        task = active(db, robot_id)
        if task:
            fail(db, task, 'Emergency stop requested by staff.')
        else:
            runtime.state(robot_id)['checkpoint_location_verified'] = False
            runtime.submit(robot_id, 'stop', safety=True)
        return {'message': 'Stop requested. Physical stop is not confirmed; check Atlas.'}

@router.post('/robots/{robot_id}/checkpoint-confirmation')
def confirm_position(robot_id: str, body: PositionConfirmation, context=Depends(staff), db=Depends(get_db)):
    with runtime.lock:
        sweep(db)
        robot = db.get(RobotModel, robot_id)
        if not robot:
            raise HTTPException(404, 'Atlas not found.')
        checkpoint(db, body.checkpoint_id, active=True)
        state = runtime.state(robot_id)
        if not body.operator_confirmed or time.time() - state.get('guidance_poll_at', 0) > POLL_TIMEOUT or not state.get('guidance_stopped') or state['recording_id']:
            raise HTTPException(409, 'Confirm actual placement only while compatible firmware reports stopped and no recording is active.')
        task = active(db, robot_id)
        if task:
            if task.status in ('awaiting_otp', 'queued'):
                raise HTTPException(409, 'Cancel the station authorization before changing its checkpoint.')
            route = check_task(db, task)
            if not route or str(body.task_id) != task.id or task.progress_index != len(route.route_segments) or str(body.checkpoint_id) != route.destination_checkpoint_id:
                raise HTTPException(409, 'Only the completed replay destination can be confirmed for this task.')
            # Evidence only: firmware must also report contextual stopped arrival.
            arrivals[task.id] = dict(route_id=route.id, checkpoint_id=route.destination_checkpoint_id, at=time.time())
            return {'message': 'Physical arrival attested. Waiting for the ESP32 arrival report.'}
        elif body.task_id:
            raise HTTPException(409, 'No active task to confirm.')
        if not robot.home_checkpoint_id or str(body.checkpoint_id) != robot.home_checkpoint_id:
            raise HTTPException(409, 'Configure a home station, then confirm actual placement at that station.')
        robot.residing_checkpoint_id = str(body.checkpoint_id)
        robot.guidance_state = 'at_home'
        robot.last_sequence_number = None
        state.update(checkpoint_location_verified=True, guidance_confirmed_at=time.time())
        db.commit()
        return {'message': 'Operator-confirmed checkpoint saved.'}

@router.patch('/robots/{robot_id}/home')
def configure_home(robot_id: str, body: HomeConfiguration, context=Depends(staff), db=Depends(get_db)):
    with runtime.lock:
        robot = db.get(RobotModel, robot_id)
        if not robot:
            raise HTTPException(404, 'Atlas not found.')
        if db.query(GuidanceTask).filter(GuidanceTask.robot_id == robot_id, GuidanceTask.status.in_(ACTIVE)).first():
            raise HTTPException(409, 'Cancel or finish this robot\'s requests before changing its home.')
        checkpoint(db, body.home_checkpoint_id, active=True)
        robot.home_checkpoint_id, robot.guidance_state = str(body.home_checkpoint_id), 'unknown'
        runtime.state(robot_id)['checkpoint_location_verified'] = False
        db.commit()
        return {'home_checkpoint_id': robot.home_checkpoint_id, 'robot_state': robot.guidance_state}

@router.get('/robot/{robot_id}/guidance/command', dependencies=[Depends(device)])
def replay_poll(robot_id: str, stopped: bool, protocol: int, db=Depends(get_db)):
    available()
    if protocol not in (2, 3):
        raise HTTPException(409, 'Atlas replay protocol 2 or 3 required.')
    with runtime.lock:
        # Check old heartbeat BEFORE accepting this one: no auto-resume after loss.
        sweep(db)
        state = runtime.state(robot_id)
        previous_poll = state.get('guidance_poll_at', 0)
        if time.time() - previous_poll > POLL_TIMEOUT:
            state['checkpoint_location_verified'] = False
        state.update(guidance_poll_at=time.time(), guidance_stopped=stopped, last_seen=time.time())
        robot = db.query(RobotModel).filter_by(id=robot_id).with_for_update().first()
        if not robot:
            raise HTTPException(404, 'Atlas not found.')
        if not stopped and robot.guidance_state in ('at_home', 'awaiting_otp'):
            state['checkpoint_location_verified'] = False
            robot.guidance_state = 'unknown'
            pending = active(db, robot_id)
            if pending:
                fail(db, pending, 'Atlas left its station before authorization.')
            db.commit()
        select_next(db, robot)
        task = active(db, robot_id)
        stop_message = {'command': 'stop' if protocol == 2 else 'idle', 'execute': protocol == 2,
            'protocol': protocol, 'clear_otp': True, 'robot_state': robot.guidance_state}
        if not task:
            if not stopped:
                state['checkpoint_location_verified'] = False
            return stop_message
        if task.status == 'awaiting_otp':
            display = otp_displays.get(task.id)
            if not display:
                fail(db, task, 'OTP display state was lost.', 'expired', preserve_home=True)
                return stop_message
            return dict(protocol=protocol, command='display_otp', execute=False, request_id=task.id,
                command_id=str(uuid5(UUID(task.id), str(task.otp_expires_at))), atlas_otp=display,
                otp_expires_at=stamp(task.otp_expires_at).isoformat(), robot_state='awaiting_otp', stop=True)
        if not task.session_id or not task.verified_at:
            fail(db, task, 'Guidance session has no consumed Atlas authorization.')
            return stop_message
        route = check_task(db, task)
        if not route:
            return stop_message
        if protocol == 3 and any(s.get('motion_kind') is None for s in route.route_segments):
            fail(db, task, 'Protocol 3 requires typed measured routes; re-record legacy routes.')
            return stop_message
        if task.progress_index >= len(route.route_segments):
            evidence = arrivals.get(task.id)
            return stop_message | dict(task_id=task.id, session_id=task.session_id, route_id=route.id,
                phase='return' if task.status == 'returning_home' else 'guidance', awaiting_confirmation=True,
                checkpoint_id=route.destination_checkpoint_id,
                operator_attested=bool(evidence and evidence['route_id'] == route.id and time.time() - evidence['at'] <= CONFIRM_TIMEOUT))
        segment = route.route_segments[task.progress_index]
        delivery = deliveries.get(task.id)
        if delivery is None:
            if not stopped:
                fail(db, task, 'Robot was not stopped before segment dispatch.')
                return stop_message
            delivery = dict(issued_at=time.time(), status='offered', protocol=protocol)
            deliveries[task.id] = delivery
            if task.status != 'returning_home':
                task.status = 'awaiting_robot' if task.progress_index == 0 else 'running'
            task.updated_at = now()
            state['checkpoint_location_verified'] = False
            db.commit()
        if delivery.get('protocol', 2) != protocol:
            fail(db, task, 'Replay protocol changed during a segment.')
            return stop_message
        if delivery['status'] == 'paused' and not stopped:
            fail(db, task, 'Firmware reports motion after acknowledged pause.')
            return stop_message
        return dict(protocol=protocol, command='replay_segment', clear_otp=True, task_id=task.id, session_id=task.session_id,
            phase='return' if task.status == 'returning_home' else 'guidance', route_id=route.id,
            command_id=command_id(task, route, segment['sequence']), sequence=segment['sequence'],
            movement=segment['movement'], execute=delivery['status'] == 'offered',
            lease_ms=int(LEASE_SECONDS * 1000), segment_timeout_ms=SEGMENT_TIMEOUT * 1000) | (
                dict(motion_kind=segment['motion_kind'], turn_degrees=segment.get('turn_degrees'),
                     execution_status=delivery['status'], control=delivery.get('control')) if protocol == 3 else {})

@router.post('/robot/{robot_id}/guidance/result', dependencies=[Depends(device)])
def replay_result(robot_id: str, body: ReplayResult, db=Depends(get_db)):
    available()
    with runtime.lock:
        sweep(db)
        task = db.get(GuidanceTask, str(body.task_id))
        if not task or task.robot_id != robot_id or task.session_id != str(body.session_id):
            current = active(db, robot_id)
            if current:
                fail(db, current, 'Replay identity mismatch.')
            raise HTTPException(409, 'Replay identity mismatch.')
        route = current_route(db, task)
        if not route or route.id != str(body.route_id) or str(body.command_id) != command_id(task, route, body.sequence):
            if task.status in ACTIVE:
                fail(db, task, 'Replay command identity mismatch.')
            raise HTTPException(409, 'Replay command identity mismatch.')
        # A late fault must never be hidden by completion deduplication.
        if body.status == 'failed' and task.status in ACTIVE:
            fail(db, task, 'Robot reported an obstacle, fault or execution failure.')
            return {'acknowledged': True}
        previous = next((s for s in route.route_segments[:task.progress_index] if s['sequence'] == body.sequence), None)
        if previous:
            if body.status == 'completed' and (body.movement is None or body.movement.model_dump() != previous['movement']):
                if task.status in ACTIVE:
                    fail(db, task, 'Conflicting duplicate encoder result.')
                raise HTTPException(409, 'Conflicting duplicate encoder result. Stop requested.')
            return {'acknowledged': True, 'duplicate': True}
        if task.status not in ('awaiting_robot', 'running', 'returning_home') or not check_task(db, task):
            raise HTTPException(409, 'Task is no longer executing.')
        delivery = deliveries.get(task.id)
        segment = route.route_segments[task.progress_index] if task.progress_index < len(route.route_segments) else None
        if not delivery or not segment or body.sequence != segment['sequence']:
            fail(db, task, 'Out-of-order replay acknowledgement.')
            raise HTTPException(409, 'Unexpected segment acknowledgement. Stop requested.')
        if body.status in ('paused', 'resumed'):
            identifier = str(body.control_request_id)
            control = delivery.get('controls', {}).get(identifier)
            action = 'pause' if body.status == 'paused' else 'resume'
            if not control or control['action'] != action or delivery.get('protocol') != 3:
                fail(db, task, 'Unexpected pause/resume acknowledgement.')
                raise HTTPException(409, 'Control identity mismatch. Stop requested.')
            if not control['acknowledged']:
                if delivery.get('control') is not control or (action == 'pause' and not runtime.state(robot_id).get('guidance_stopped')):
                    fail(db, task, 'Pause/resume state contradiction.')
                    raise HTTPException(409, 'Control state contradiction. Stop requested.')
                control['acknowledged'] = True
                delivery['status'] = 'paused' if action == 'pause' else 'started'
                delivery['control'] = None
            return {'acknowledged': True, 'control_request_id': identifier, 'status': body.status}
        if body.control_request_id is not None:
            raise HTTPException(422, 'Control UUID is only valid for pause/resume results.')
        if body.status == 'failed':
            fail(db, task, 'Robot reported an obstacle, fault or execution failure.')
        elif body.status in ('accepted', 'started'):
            if delivery['status'] == 'paused' or delivery.get('control'):
                # Retransmitted start receipts cannot undo an acknowledged pause.
                return {'acknowledged': True}
            if delivery['status'] != 'started':
                delivery['status'] = body.status
            if task.status != 'returning_home':
                task.status = 'running'
            task.updated_at = now()
            db.commit()
        else:
            if delivery['status'] != 'started' or body.movement is None or body.movement.model_dump() != segment['movement']:
                fail(db, task, 'Missing start acknowledgement or encoder result does not match target.')
                raise HTTPException(409, 'Invalid measured completion. Stop requested.')
            task.progress_index += 1
            task.updated_at = now()
            deliveries.pop(task.id, None)
            db.commit()
        return {'acknowledged': True}

@router.post('/robot/{robot_id}/guidance/arrival', dependencies=[Depends(device)])
def report_arrival(robot_id: str, body: ArrivalReport, db=Depends(get_db)):
    available()
    with runtime.lock:
        sweep(db)
        task = db.query(GuidanceTask).filter_by(id=str(body.task_id), robot_id=robot_id).with_for_update().first()
        if not task or task.session_id != str(body.session_id):
            raise HTTPException(409, 'Arrival session mismatch.')
        if task.status == 'completed' and str(body.route_id) == task.return_route_id:
            returning = db.get(RouteModel, task.return_route_id)
            if str(body.checkpoint_id) != returning.destination_checkpoint_id or not body.stopped:
                raise HTTPException(409, 'Duplicate arrival context mismatch.')
            return {'acknowledged': True, 'duplicate': True}
        if task.status == 'returning_home' and str(body.route_id) == task.route_id:
            outbound = db.get(RouteModel, task.route_id)
            if str(body.checkpoint_id) == outbound.destination_checkpoint_id and body.stopped:
                return {'acknowledged': True, 'duplicate': True}
        route = check_task(db, task) if task.status in ('running', 'awaiting_robot', 'returning_home') else None
        state = runtime.state(robot_id)
        evidence = arrivals.get(task.id)
        if (not route or str(body.route_id) != route.id or task.progress_index != len(route.route_segments) or
            str(body.checkpoint_id) != route.destination_checkpoint_id or not body.stopped or
            not state.get('guidance_stopped') or time.time() - state.get('guidance_poll_at', 0) > POLL_TIMEOUT or
            not evidence or evidence['route_id'] != route.id or time.time() - evidence['at'] > CONFIRM_TIMEOUT):
            raise HTTPException(409, 'Arrival requires completed measured replay, stopped firmware and current physical checkpoint attestation.')
        robot = db.get(RobotModel, robot_id)
        robot.residing_checkpoint_id = route.destination_checkpoint_id
        state.update(checkpoint_location_verified=True, guidance_confirmed_at=time.time())
        deliveries.pop(task.id, None); arrivals.pop(task.id, None)
        if task.status == 'returning_home':
            task.status, robot.guidance_state = 'completed', 'at_home'
            state['guidance_task_id'] = None
            clear_otp(task)
        else:
            task.status, robot.guidance_state, task.progress_index = 'returning_home', 'returning_home', 0
            state['checkpoint_location_verified'] = False
        task.updated_at = now()
        db.commit()
        return {'acknowledged': True, 'robot_state': robot.guidance_state}
