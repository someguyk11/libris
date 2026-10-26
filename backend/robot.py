"""Atlas V1 persistent identity + temporary command/telemetry state."""
import re
import time
from uuid import UUID
from typing import Optional
from fastapi import APIRouter, Depends, HTTPException, Query
from pydantic import BaseModel, Field
from sqlalchemy.orm import Session
from sqlalchemy.exc import IntegrityError
from database import get_db, RobotModel
from atlas_schemas import RobotCreate, RobotUpdate, RobotResponse, CommandSubmission, ExecutionResult, SensorSample
from recordings import apply_result, add_sample
import atlas_runtime as runtime
import atlas_recovery as recovery

router = APIRouter(tags=['Atlas'])

class RobotTelemetry(BaseModel):
    wifi_ssid: str
    signal_strength: int
    uptime_seconds: int = Field(ge=0)
    firmware_version: Optional[str] = Field(default=None, max_length=100)

class NetworkProvisionRequest(BaseModel):
    ssid: str = Field(min_length=1, max_length=32)
    password: str = Field(max_length=63)

def get_or_create_robot(db, robot_id):
    if not re.fullmatch(r'[A-Za-z0-9-]{1,80}', robot_id):
        raise HTTPException(422, 'Invalid robot identifier.')
    value = db.get(RobotModel, robot_id)
    if not value:
        value = RobotModel(id=robot_id, name=robot_id)
        db.add(value); db.commit(); db.refresh(value)
    return value

def drive_ready(state):
    return bool(state['last_command_poll'] and 0 <= time.time() - state['last_command_poll'] <= 3)

def reset_runtime(db=None):
    # Historical confirmed checkpoints remain in SQL, but are never current-location proof.
    runtime.clear()

@router.get('/robots/', response_model=list[RobotResponse])
def list_robots(db: Session = Depends(get_db)):
    return db.query(RobotModel).order_by(RobotModel.created_at, RobotModel.id).all()

@router.post('/robots/', response_model=RobotResponse, status_code=201)
def register_robot(body: RobotCreate, db: Session = Depends(get_db)):
    with runtime.lock:
        value = RobotModel(**body.model_dump())
        db.add(value)
        try:
            db.commit()
        except IntegrityError:
            db.rollback()
            raise HTTPException(409, 'This robot ID is already registered.')
        db.refresh(value)
        return value

@router.patch('/robots/{registration_id}', response_model=RobotResponse)
def update_robot(registration_id: str, body: RobotUpdate, db: Session = Depends(get_db)):
    with runtime.lock:
        value = db.get(RobotModel, registration_id)
        if not value:
            raise HTTPException(404, 'Atlas not found.')
        value.name = body.name
        db.commit(); db.refresh(value)
        return value

@router.post('/robot/{robot_id}/command')
def send_command(robot_id: str, body: CommandSubmission, db: Session = Depends(get_db)):
    with runtime.lock:
        get_or_create_robot(db, robot_id)
        state = runtime.state(robot_id)
        pending_recovery = recovery.expire(state)
        if body.command.value != 'stop' and pending_recovery and pending_recovery['status'] in ('queued', 'accepted'):
            raise HTTPException(409, 'Wait for recovery to finish before issuing movement.')
        if body.command.value == 'stop' and state.get('guidance_task_id'):
            from guidance import active, fail
            task = active(db, robot_id)
            if task:
                fail(db, task, 'Stop requested from manual driving controls.')
        if body.command.value != 'stop' and not drive_ready(state):
            raise HTTPException(409, 'Robot is not polling commands. Movement was not queued.')
        command = runtime.submit(robot_id, body.command.value, body.recording_id, body.request_id)
        return {'status': 200, 'data': runtime.wire(command)}

@router.get('/robot/{robot_id}/command')
def get_command(robot_id: str, db: Session = Depends(get_db), boot_id: str | None = Query(default=None, pattern=r'^[a-f0-9]{32}$'), recovery_completed: UUID | None = None):
    with runtime.lock:
        get_or_create_robot(db, robot_id)
        state = runtime.state(robot_id)
        if boot_id is not None:
            recovery.observe_boot(db, robot_id, boot_id, str(recovery_completed) if recovery_completed else None)
        command = runtime.pending_commands.get(state['active_command_id'])
        if command and command['command'] != 'stop' and command['status'] not in ('completed', 'failed') and time.time() - command['refreshed_at'] > runtime.COMMAND_TTL:
            command = runtime.submit(robot_id, 'stop', safety=True)
        state['last_seen'] = state['last_command_poll'] = time.time()
        data = dict(command='stop', sequence=state['sequence'], command_id=None, recording_id=state['recording_id'], recording_sequence=None, execute=False)
        if command:
            if command['status'] == 'queued':
                command['status'] = 'delivered'
            data.update(runtime.wire(command))
            data['execute'] = command['status'] == 'delivered'
            if command['status'] in ('completed', 'failed'):
                data['command'] = 'stop'
        data['factory_reset'], data['new_network'] = state['factory_reset_triggered'], state['pending_reconfig']
        data['recovery'] = recovery.pending(state)
        data['auto_reconnect'] = state['auto_reconnect']
        state['factory_reset_triggered'] = False; state['pending_reconfig'] = None
        return {'status': 200, 'data': data}

@router.post('/robot/{robot_id}/command/result')
def command_result(robot_id: str, body: ExecutionResult):
    with runtime.lock:
        return apply_result(robot_id, body)

@router.post('/robot/{robot_id}/sensors')
def sensor_sample(robot_id: str, body: SensorSample):
    with runtime.lock:
        return add_sample(robot_id, body)

@router.post('/robot/{robot_id}/telemetry')
def update_telemetry(robot_id: str, body: RobotTelemetry, db: Session = Depends(get_db)):
    with runtime.lock:
        value = get_or_create_robot(db, robot_id)
        state = runtime.state(robot_id)
        for key in ('wifi_ssid', 'signal_strength', 'uptime_seconds'):
            state[key] = getattr(body, key)
        state['last_seen'] = state['telemetry_at'] = time.time()
        if body.firmware_version is not None and value.firmware_version != body.firmware_version:
            value.firmware_version = body.firmware_version; db.commit()
        return {'status': 200, 'status_state': 'Online'}

@router.get('/robot/{robot_id}/status')
def get_robot_status(robot_id: str, db: Session = Depends(get_db)):
    with runtime.lock:
        value = db.get(RobotModel, robot_id)
        if not value:
            raise HTTPException(404, 'Atlas not found.')
        state = runtime.state(robot_id)
        online = bool(state['last_seen'] and 0 <= time.time() - state['last_seen'] <= 15)
        command = runtime.pending_commands.get(state['active_command_id'])
        reported_command = command['command'] if command and command['status'] not in ('completed', 'failed') and 0 <= time.time() - command['refreshed_at'] <= runtime.COMMAND_TTL else 'stop'
        data = {key: state[key] for key in ('sequence', 'last_seen', 'telemetry_at', 'wifi_ssid', 'signal_strength', 'uptime_seconds', 'factory_reset_triggered')}
        data.update(robot_id=value.id, command=reported_command, status='Online' if online else 'Offline' if state['last_seen'] else 'Needs drive setup',
            drive_ready=drive_ready(state), network_change_pending=state['pending_reconfig'] is not None,
            residing_checkpoint_id=value.residing_checkpoint_id, last_sequence_number=value.last_sequence_number,
            firmware_version=value.firmware_version, recording_id=state['recording_id'], checkpoint_location_verified=online and state['checkpoint_location_verified'], guidance_task_id=state.get('guidance_task_id'))
        data.update(boot_id=state['boot_id'], recovery=recovery.expire(state), auto_reconnect=state['auto_reconnect'],
            recovery_capable=state['recovery_capable'], replay_connected=False, imu_status='not_implemented')
        return {'status': 200, 'data': data}

@router.post('/robot/{robot_id}/recovery', status_code=202)
def request_recovery(robot_id: str, body: recovery.RecoveryRequest, db: Session = Depends(get_db)):
    with runtime.lock:
        get_or_create_robot(db, robot_id)
        return {'status': 202, 'data': recovery.queue(db, robot_id, body)}

@router.post('/robot/{robot_id}/recovery/ack')
def acknowledge_recovery(robot_id: str, body: recovery.RecoveryAck):
    with runtime.lock:
        return recovery.acknowledge(robot_id, body)

@router.post('/robot/{robot_id}/reconfigure')
def register_network_provision(robot_id: str, body: NetworkProvisionRequest, db: Session = Depends(get_db)):
    with runtime.lock:
        get_or_create_robot(db, robot_id)
        runtime.state(robot_id)['pending_reconfig'] = body.model_dump()
        return {'status': 200, 'message': 'Network change queued for firmware pickup; not confirmed applied.'}

@router.post('/robot/{robot_id}/reset')
def reset_robot(robot_id: str, db: Session = Depends(get_db)):
    with runtime.lock:
        get_or_create_robot(db, robot_id)
        runtime.submit(robot_id, 'stop', safety=True)
        state = runtime.state(robot_id)
        state.update(pending_reconfig=None, factory_reset_triggered=True, checkpoint_location_verified=False)
        recording = runtime.recording_for(robot_id)
        if recording:
            recording['status'] = 'failed'
        return {'status': 200, 'message': 'Factory reset queued for firmware pickup; not confirmed completed.'}
