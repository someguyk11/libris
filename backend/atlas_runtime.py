"""Single-worker temporary state. Nothing in this module is a database table."""
from collections import OrderedDict
from copy import deepcopy
from threading import RLock
from uuid import uuid4
import time
from fastapi import HTTPException

lock = RLock()
robots = {}
active_recordings = {}
pending_commands = {}
receipts = OrderedDict()
COMMAND_TTL = 1.0
MAX_PENDING = 10000

def state(robot_id):
    return robots.setdefault(robot_id, dict(command='stop', sequence=0, active_command_id=None,
        last_command_poll=0.0, last_seen=0.0, telemetry_at=0.0, wifi_ssid='Unknown Network',
        signal_strength=None, uptime_seconds=None, pending_reconfig=None, factory_reset_triggered=False,
        recording_id=None, checkpoint_location_verified=False))

def clear():
    robots.clear(); active_recordings.clear(); pending_commands.clear(); receipts.clear()

def recording_for(robot_id):
    return active_recordings.get(state(robot_id)['recording_id'])

def submit(robot_id, movement, recording_id=None, request_id=None, safety=False):
    robot = state(robot_id)
    if robot.get('guidance_task_id') and not safety:
        raise HTTPException(409, 'Atlas is reserved for guidance. Use the guidance emergency stop.')
    recording = recording_for(robot_id)
    if recording_id is not None and (not recording or recording['id'] != str(recording_id)):
        raise HTTPException(409, 'This recording does not belong to the robot or is no longer active.')
    if request_id:
        for command in pending_commands.values():
            if command['robot_id'] == robot_id and command['request_id'] == str(request_id):
                if command['command'] != movement or command['recording_id'] != (recording['id'] if recording else None):
                    raise HTTPException(409, 'request_id was already used for a different command.')
                return command
        receipt = receipts.get(('request', robot_id, str(request_id)))
        if receipt:
            if receipt['command'] != movement or receipt['recording_id'] != (recording['id'] if recording else None):
                raise HTTPException(409, 'request_id was already used for a different command.')
            return receipt
    current = pending_commands.get(robot['active_command_id'])
    if current and current['status'] not in ('completed', 'failed'):
        if current['command'] == movement and not request_id:
            current['refreshed_at'] = time.time()
            return current
        if movement != 'stop' and recording:
            raise HTTPException(409, 'Wait for the current execution result, or send Stop.')
        if not recording:
            # Preserve the legacy website's non-recording hold/release mailbox.
            # Superseded is explicitly not an execution acknowledgement.
            current['status'] = 'superseded'
            remember(current)
            pending_commands.pop(current['id'])
    if recording and recording['status'] != 'recording' and movement != 'stop':
        raise HTTPException(409, 'Recording is finalizing; new movement commands are not accepted.')
    if len(pending_commands) >= MAX_PENDING and not (safety and movement == 'stop'):
        raise HTTPException(409, 'Temporary command limit reached; save or discard recordings first.')
    if recording and len(recording['command_ids']) >= (5000 if movement == 'stop' else 4999):
        raise HTTPException(409, 'Recording command limit reached.')
    robot['sequence'] += 1
    command = dict(id=str(uuid4()), robot_id=robot_id, recording_id=recording['id'] if recording else None,
        sequence=len(recording['command_ids']) + 1 if recording else robot['sequence'],
        transport_sequence=robot['sequence'], command=movement, request_id=str(request_id) if request_id else None,
        status='queued', refreshed_at=time.time(), result=None, checkpoint_id=None,
        sensor_captured_at=None, samples={})
    pending_commands[command['id']] = command
    robot['active_command_id'] = command['id']; robot['command'] = movement
    if movement != 'stop':
        robot['checkpoint_location_verified'] = False
    if recording:
        recording['command_ids'].append(command['id'])
    return command

def wire(command):
    return dict(command=command['command'], sequence=command['transport_sequence'], command_id=command['id'],
        recording_id=command['recording_id'], recording_sequence=command['sequence'] if command['recording_id'] else None,
        execution_status=command['status'])

def remember(command):
    receipt = {key: deepcopy(command[key]) for key in ('id', 'robot_id', 'recording_id', 'sequence', 'transport_sequence', 'command', 'request_id', 'status', 'result')}
    receipt.pop('result', None)
    receipt['result_digest'] = command.get('result_digest')
    receipts[command['id']] = receipt
    if command['request_id']:
        receipts[('request', command['robot_id'], command['request_id'])] = receipt
    while len(receipts) > 4000:
        receipts.popitem(last=False)

def release_recording(recording):
    for command_id in recording['command_ids']:
        command = pending_commands.pop(command_id)
        remember(command)
    state(recording['robot_id'])['recording_id'] = None
    state(recording['robot_id'])['active_command_id'] = None
    state(recording['robot_id'])['command'] = 'stop'
    active_recordings.pop(recording['id'], None)
