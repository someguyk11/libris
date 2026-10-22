"""Temporary encoder recordings. Only an explicit successful Save writes a route."""
from copy import deepcopy
from datetime import datetime, timezone
import hashlib
import json
import time
from uuid import UUID, uuid4
from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session
from sqlalchemy.exc import IntegrityError, SQLAlchemyError
from database import get_db, RobotModel, RouteModel
from atlas_schemas import RecordingStart, RecordingSave, CheckpointConfirmation, ExecutionResult, SensorSample, RouteCreate, RouteResponse
from atlas_validation import checkpoint, validate_route
import atlas_runtime as runtime

router = APIRouter(tags=['Atlas recording'])

def get_recording(identifier):
    recording = runtime.active_recordings.get(str(identifier))
    if not recording:
        raise HTTPException(404, 'Temporary recording not found. Buffers do not survive server restart.')
    return recording

def get_command(robot_id, body):
    command = runtime.pending_commands.get(str(body.command_id))
    if not command or command['robot_id'] != robot_id:
        raise HTTPException(404, 'Command not found for this robot.')
    if command['recording_id'] != (str(body.recording_id) if body.recording_id else None) or command['sequence'] != body.sequence:
        raise HTTPException(409, 'Recording/sequence does not match this command.')
    return command

def sample_key(value):
    return value.astimezone(timezone.utc).isoformat()

def digest(body):
    return hashlib.sha256(json.dumps(body.model_dump(mode='json'), sort_keys=True).encode()).hexdigest()

def apply_result(robot_id, body: ExecutionResult):
    receipt = runtime.receipts.get(str(body.command_id))
    if receipt:
        if receipt['robot_id'] != robot_id or receipt['recording_id'] != (str(body.recording_id) if body.recording_id else None) or receipt['sequence'] != body.sequence:
            raise HTTPException(409, 'Result identity does not match the acknowledged command.')
        if body.status != 'started' and receipt['status'] != 'superseded' and receipt['result_digest'] != digest(body):
            raise HTTPException(409, 'Conflicting result for an acknowledged command.')
        return {'status': 200, 'data': {'command_id': str(body.command_id), 'execution_status': receipt['status'], 'acknowledged': True}}
    command = get_command(robot_id, body)
    if command['status'] == 'queued':
        raise HTTPException(409, 'Cannot acknowledge a command before firmware delivery.')
    if command['status'] in ('completed', 'failed'):
        if body.status != 'started' and command['result_digest'] != digest(body):
            raise HTTPException(409, 'Conflicting duplicate execution result.')
    elif body.status == 'started':
        command['status'] = 'started'
    else:
        if body.sensor_readings is not None:
            key = sample_key(body.captured_at)
            readings = body.sensor_readings.model_dump(mode='json', exclude_none=True)
            existing = command['samples'].get(key)
            if existing is not None and existing != readings:
                raise HTTPException(409, 'Conflicting sensor readings at the same capture timestamp.')
            command['samples'][key] = readings
        command['status'] = body.status
        command['result'] = body.model_dump(mode='json')
        command['result_digest'] = digest(body)
        if body.status == 'failed' and command['recording_id']:
            runtime.active_recordings[command['recording_id']]['status'] = 'failed'
        if not command['recording_id']:
            runtime.remember(command)
            runtime.pending_commands.pop(command['id'])
    robot = runtime.state(robot_id)
    robot['last_seen'] = time.time()
    return {'status': 200, 'data': {'command_id': command['id'], 'execution_status': command['status'], 'acknowledged': True}}

def add_sample(robot_id, body: SensorSample):
    receipt = runtime.receipts.get(str(body.command_id))
    if receipt:
        if receipt['robot_id'] != robot_id or receipt['recording_id'] != str(body.recording_id) or receipt['sequence'] != body.sequence:
            raise HTTPException(409, 'Late sensor sample does not match the acknowledged command.')
        return {'status': 200, 'acknowledged': True, 'retained': False}
    command = get_command(robot_id, body)
    if command['status'] == 'queued':
        raise HTTPException(409, 'Sensor sample refers to an undelivered command.')
    key = sample_key(body.captured_at)
    sample = body.sensor_readings.model_dump(mode='json', exclude_none=True)
    if key in command['samples'] and command['samples'][key] != sample:
        raise HTTPException(409, 'Conflicting sample at this command/capture timestamp.')
    if len(command['samples']) >= 1000 and key not in command['samples']:
        raise HTTPException(409, 'Temporary sensor buffer limit reached for this command.')
    command['samples'][key] = sample
    return {'status': 200, 'acknowledged': True}

def snapshot(recording):
    return {key: deepcopy(recording[key]) for key in ('id', 'robot_id', 'status', 'started_at', 'start_checkpoint_id')} | {
        'commands': [{key: deepcopy(runtime.pending_commands[identifier][key]) for key in ('id', 'sequence', 'command', 'status', 'checkpoint_id', 'result')} for identifier in recording['command_ids']]}

def finalize(recording):
    if recording['status'] == 'failed' or any(runtime.pending_commands[key]['status'] == 'failed' for key in recording['command_ids']):
        raise HTTPException(409, 'Recording contains a failed execution. Inspect or discard it; missing encoder results cannot be invented.')
    recording['status'] = 'finalizing'
    commands = [runtime.pending_commands[value] for value in recording['command_ids']]
    if not commands or commands[-1]['command'] != 'stop':
        runtime.submit(recording['robot_id'], 'stop', recording['id'], safety=True)

@router.post('/robot/{robot_id}/recordings', status_code=201)
def start_recording(robot_id: str, body: RecordingStart, db: Session = Depends(get_db)):
    with runtime.lock:
        if not db.get(RobotModel, robot_id):
            raise HTTPException(404, 'Register this robot first.')
        checkpoint(db, body.start_checkpoint_id, active=True)
        robot = runtime.state(robot_id)
        if robot.get('guidance_task_id'):
            raise HTTPException(409, 'Atlas is reserved for guidance; recording cannot start.')
        if robot['recording_id']:
            raise HTTPException(409, 'This robot already has an active recording.')
        current = runtime.pending_commands.get(robot['active_command_id'])
        if current and current['status'] not in ('completed', 'failed'):
            raise HTTPException(409, 'Acknowledge or stop existing movement before starting a recording.')
        identifier = str(uuid4())
        recording = dict(id=identifier, robot_id=robot_id, status='recording', started_at=datetime.now(timezone.utc).isoformat(),
            start_checkpoint_id=str(body.start_checkpoint_id), command_ids=[], save_metadata=None, last_confirmed_sequence=0)
        runtime.active_recordings[identifier] = recording
        robot['recording_id'] = identifier
        return snapshot(recording)

@router.get('/recordings/{recording_id}')
def get_recording_status(recording_id: UUID):
    with runtime.lock:
        return snapshot(get_recording(recording_id))

@router.post('/recordings/{recording_id}/checkpoints')
def confirm_checkpoint(recording_id: UUID, body: CheckpointConfirmation, db: Session = Depends(get_db)):
    with runtime.lock:
        recording = get_recording(recording_id)
        value = checkpoint(db, body.checkpoint_id, active=True)
        command = next((runtime.pending_commands[key] for key in recording['command_ids'] if runtime.pending_commands[key]['sequence'] == body.sequence), None)
        if not command or command['status'] != 'completed':
            raise HTTPException(409, 'Checkpoint confirmation requires a completed, measured execution result.')
        identifier = str(body.checkpoint_id)
        timestamp = sample_key(body.sensor_captured_at) if body.sensor_captured_at else None
        if command['checkpoint_id'] and (command['checkpoint_id'] != identifier or command['sensor_captured_at'] != timestamp):
            raise HTTPException(409, 'Conflicting checkpoint confirmation for this sequence.')
        command['checkpoint_id'], command['sensor_captured_at'] = identifier, timestamp
        if body.sequence >= recording['last_confirmed_sequence']:
            robot = db.get(RobotModel, recording['robot_id'])
            robot.residing_checkpoint_id = value.id; robot.last_sequence_number = body.sequence
            db.commit()
            recording['last_confirmed_sequence'] = body.sequence
            runtime.state(robot.id)['checkpoint_location_verified'] = body.sequence == len(recording['command_ids'])
        return {'status': 200, 'sequence': body.sequence, 'checkpoint_id': identifier, 'basis': 'operator_confirmed'}

@router.post('/recordings/{recording_id}/finalize')
def finalize_recording(recording_id: UUID):
    with runtime.lock:
        recording = get_recording(recording_id); finalize(recording)
        return snapshot(recording)

@router.post('/recordings/{recording_id}/save', response_model=RouteResponse)
def save_recording(recording_id: UUID, body: RecordingSave, db: Session = Depends(get_db)):
    with runtime.lock:
        # The recording UUID is also the saved route UUID: retries cannot create duplicates.
        existing = db.get(RouteModel, str(recording_id))
        if existing:
            if existing.name != body.name or existing.destination_checkpoint_id != str(body.destination_checkpoint_id) or existing.status != body.status:
                raise HTTPException(409, 'This recording was already saved with different metadata.')
            buffered = runtime.active_recordings.get(str(recording_id))
            if buffered:
                if existing.robot_id != buffered['robot_id'] or existing.start_checkpoint_id != buffered['start_checkpoint_id']:
                    raise HTTPException(409, 'Saved route identity does not match the buffered recording.')
                # Commit may have succeeded even if refresh/HTTP delivery failed.
                runtime.release_recording(buffered)
            return existing
        recording = get_recording(recording_id)
        checkpoint(db, body.destination_checkpoint_id, active=True)
        metadata = body.model_dump(mode='json')
        if recording['save_metadata'] and recording['save_metadata'] != metadata:
            raise HTTPException(409, 'Save metadata is fixed while finalizing; retry the original request.')
        recording['save_metadata'] = metadata
        finalize(recording)
        commands = [runtime.pending_commands[key] for key in recording['command_ids']]
        if any(command['status'] != 'completed' for command in commands):
            return JSONResponse(status_code=202, content={'status': 'finalizing', 'recording_id': recording['id'], 'message': 'Stop requested. Waiting for all ESP32 execution results; no route has been persisted.'})
        segments = []
        for command in commands:
            segment = dict(sequence=command['sequence'], checkpoint_id=command['checkpoint_id'], movement=command['result']['movement'])
            if command['checkpoint_id'] and command['sensor_captured_at']:
                readings = command['samples'].get(command['sensor_captured_at'])
                if readings is None:
                    raise HTTPException(409, 'A selected checkpoint sensor snapshot has not arrived; buffer retained.')
                segment['checkpoint_sensor_readings'] = readings
            segments.append(segment)
        data = RouteCreate(name=body.name, start_checkpoint_id=recording['start_checkpoint_id'], destination_checkpoint_id=body.destination_checkpoint_id,
            status=body.status, route_segments=segments)
        validate_route(db, data)
        route = RouteModel(id=recording['id'], robot_id=recording['robot_id'], **data.model_dump(mode='json'))
        db.add(route)
        try:
            db.commit(); db.refresh(route)
        except IntegrityError:
            db.rollback()
            raise HTTPException(409, 'Database rejected the route. Recording retained; resolve duplicate names or changed references and retry.')
        except SQLAlchemyError:
            db.rollback()
            raise HTTPException(503, 'Database save failed. Temporary recording retained for retry.')
        runtime.release_recording(recording)
        return route

@router.delete('/recordings/{recording_id}')
def discard_recording(recording_id: UUID):
    with runtime.lock:
        recording = get_recording(recording_id)
        recording['status'] = 'discarding'
        commands = [runtime.pending_commands[key] for key in recording['command_ids']]
        if not commands or commands[-1]['command'] != 'stop':
            runtime.submit(recording['robot_id'], 'stop', recording['id'], safety=True)
            commands = [runtime.pending_commands[key] for key in recording['command_ids']]
        if any(command['status'] not in ('completed', 'failed') for command in commands) or commands[-1]['status'] != 'completed':
            return JSONResponse(status_code=202, content={'status': 'discarding', 'message': 'Stop requested; awaiting execution results before discarding.'})
        runtime.release_recording(recording)
        return {'status': 'discarded'}
