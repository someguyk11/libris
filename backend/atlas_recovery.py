"""Small, volatile recovery mailbox. Never resumes interrupted navigation."""
import time
from typing import Literal
from uuid import UUID
from fastapi import HTTPException
from pydantic import BaseModel, ConfigDict, Field, StrictBool
import atlas_runtime as runtime

TTL = 60

class RecoveryRequest(BaseModel):
    model_config = ConfigDict(extra='forbid')
    request_id: UUID
    action: Literal['reconnect', 'restart', 'auto_reconnect']
    enabled: StrictBool | None = None

class RecoveryAck(BaseModel):
    model_config = ConfigDict(extra='forbid')
    request_id: UUID
    boot_id: str = Field(pattern=r'^[a-f0-9]{32}$')

def interrupt(db, robot_id, reason):
    from guidance import active, fail
    task = active(db, robot_id)
    if task:
        fail(db, task, reason)
    robot = runtime.state(robot_id)
    command = runtime.pending_commands.get(robot['active_command_id'])
    if command and command['status'] not in ('completed', 'failed'):
        command['interrupted'] = True
    recording = runtime.recording_for(robot_id)
    if recording:
        recording['status'] = 'failed'
    robot.update(active_command_id=None, command='stop', checkpoint_location_verified=False)
    # Do not attach the recovery stop to a failed recording buffer.

def expire(robot):
    request = robot['recovery']
    if request and request['status'] in ('queued', 'accepted') and time.time() - request['created_at'] > TTL:
        request['status'] = 'expired'
    return request

def observe_boot(db, robot_id, boot_id, completed_id=None):
    robot = runtime.state(robot_id)
    request = expire(robot)
    if robot['boot_id'] != boot_id:
        interrupt(db, robot_id, 'Firmware boot changed; interrupted work must not resume.')
        if request and request['status'] == 'accepted' and request['action'] == 'restart':
            request['status'] = 'recovered'
        elif request and request['status'] in ('queued', 'accepted'):
            request['status'] = 'cancelled'
        robot['boot_id'] = boot_id
    elif request and request['status'] == 'accepted' and request['request_id'] == completed_id:
        if request['action'] == 'reconnect':
            request['status'] = 'recovered'
        elif request['action'] == 'auto_reconnect':
            request['status'] = 'applied'
    robot['recovery_capable'] = True

def queue(db, robot_id, body):
    robot = runtime.state(robot_id)
    current = expire(robot)
    identifier = str(body.request_id)
    receipts = robot.setdefault('recovery_receipts', {})
    previous = receipts.get(identifier)
    if previous:
        if previous['action'] != body.action or previous['enabled'] != body.enabled:
            raise HTTPException(409, 'request_id was already used for a different recovery request.')
        return previous
    if current and current['status'] in ('queued', 'accepted'):
        raise HTTPException(409, 'A recovery request is already pending.')
    if not robot['recovery_capable'] or not robot['last_command_poll'] or time.time() - robot['last_command_poll'] > 3:
        raise HTTPException(409, 'Compatible firmware must be polling before recovery can be queued.')
    if (body.action == 'auto_reconnect') != (body.enabled is not None):
        raise HTTPException(422, 'Only auto_reconnect requires an enabled boolean.')
    interrupt(db, robot_id, 'Operator requested connection recovery; no automatic navigation resume.')
    robot['recovery'] = dict(request_id=identifier, action=body.action, enabled=body.enabled,
        status='queued', created_at=time.time(), boot_id=robot['boot_id'])
    receipts[identifier] = robot['recovery']
    if len(receipts) > 64:
        del receipts[next(iter(receipts))]
    return robot['recovery']

def acknowledge(robot_id, body):
    robot = runtime.state(robot_id)
    request = expire(robot)
    if not request or request['request_id'] != str(body.request_id) or request['boot_id'] != body.boot_id or robot['boot_id'] != body.boot_id:
        raise HTTPException(409, 'Recovery request or firmware boot no longer matches.')
    if request['status'] not in ('queued', 'accepted', 'applied'):
        raise HTTPException(409, 'Recovery request is no longer executable.')
    if request['action'] == 'auto_reconnect':
        robot['auto_reconnect'] = request['enabled']
    if request['status'] != 'applied':
        request['status'] = 'accepted'
    return {'status': 200, 'data': {'request_id': request['request_id'], 'acknowledged': True}}

def pending(robot):
    request = expire(robot)
    if request and request['status'] == 'queued':
        return {key: request[key] for key in ('request_id', 'action', 'enabled')}
    return None
