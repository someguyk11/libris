"""Opt-in operational security boundary. No client-supplied roles or device identity."""
import hmac
import json
import os
from uuid import UUID
from fastapi import Depends, HTTPException, Request
from auth import authenticate
from auth_config import get_config, allowed_origins
from database import get_db

def enabled():
    return os.getenv('GUIDANCE_ENABLED', 'false').lower() == 'true'

def device_keys():
    try:
        keys = json.loads(os.getenv('ATLAS_DEVICE_KEYS', '{}'))
        if not isinstance(keys, dict) or any(not isinstance(k, str) or not isinstance(v, str) or len(v) < 32 for k, v in keys.items()):
            raise ValueError()
        return keys
    except (ValueError, TypeError):
        raise RuntimeError('ATLAS_DEVICE_KEYS must be a JSON object of robot IDs to random secrets of at least 32 characters.') from None

def staff_ids():
    return {str(UUID(item.strip())) for item in os.getenv('AUTH_STAFF_USER_IDS', '').split(',') if item.strip()}

def validate_config():
    # These settings protect operations even when reader guidance is disabled.
    keys, staff = device_keys(), staff_ids()
    if enabled() and (not get_config().enabled or not keys or not staff):
        raise RuntimeError('Guidance requires enabled authentication, trusted staff IDs and per-robot device keys.')

def available():
    if not enabled():
        raise HTTPException(503, 'Atlas guidance is not enabled. Firmware setup and physical testing are required.')

def browser(request, db):
    if not get_config().enabled:
        raise HTTPException(503, 'Authentication is not configured.')
    if request.method not in ('GET', 'HEAD') and request.headers.get('origin') not in allowed_origins():
        raise HTTPException(403, 'Untrusted request origin.')
    return authenticate(db, request, csrf=request.method not in ('GET', 'HEAD'))

def reader(request: Request, db=Depends(get_db)):
    available()
    return browser(request, db)

def staff(request: Request, db=Depends(get_db)):
    context = browser(request, db)
    if context.user_id not in staff_ids():
        raise HTTPException(403, 'Staff access required.')
    return context

def device(request: Request):
    robot_id = request.path_params.get('robot_id')
    expected = device_keys().get(robot_id)
    provided = request.headers.get('x-atlas-key', '')
    if not expected or not hmac.compare_digest(expected.encode(), provided.encode()):
        raise HTTPException(401, 'Device authentication required.')

def operational_guard(request: Request, db=Depends(get_db)):
    # Operational security is independent of the guidance feature toggle.
    path = request.url.path
    device_call = path.startswith('/robot/') and (
        (request.method == 'GET' and path.endswith('/command')) or
        (request.method == 'POST' and path.endswith(('/command/result', '/sensors', '/telemetry', '/recovery/ack'))))
    if device_call:
        return device(request)
    public_read = request.method == 'GET' and (path.startswith('/book/') or path.startswith('/shelves'))
    if not public_read:
        return staff(request, db)
