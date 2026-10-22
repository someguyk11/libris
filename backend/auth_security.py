"""Cookie/CSRF, atomic database throttling, and auth-only HTTP hardening."""
import hashlib
import hmac
from datetime import timedelta, timezone
from fastapi import HTTPException
from sqlalchemy import case
from sqlalchemy.dialects.postgresql import insert as pg_insert
from sqlalchemy.dialects.sqlite import insert as sqlite_insert
from starlette.responses import JSONResponse
from auth_config import get_config
from auth_models import AuthRateLimit, AuthEvent, now


def aware(value):
    return value.replace(tzinfo=timezone.utc) if value.tzinfo is None else value


def digest(value):
    return hashlib.sha256(value.encode()).hexdigest()


def keyed(value):
    return hmac.new(get_config().security_secret.encode(), value.encode(), hashlib.sha256).hexdigest()


def csrf_token(cookie):
    return keyed('csrf:' + cookie)


def check_csrf(request, cookie):
    token = request.headers.get('x-csrf-token', '')
    if not cookie or not hmac.compare_digest(token, csrf_token(cookie)):
        raise HTTPException(403, 'Invalid CSRF token.')


def cookie_value(request):
    value = request.cookies.get(get_config().cookie_name, '')
    return value if 30 <= len(value) <= 128 else ''


def set_cookie(response, cookie, seconds):
    config = get_config()
    response.set_cookie(config.cookie_name, cookie, max_age=max(1, int(seconds)), secure=config.secure_cookie,
                        httponly=True, samesite='lax', path='/')


def clear_cookie(response):
    config = get_config()
    response.delete_cookie(config.cookie_name, path='/', secure=config.secure_cookie, httponly=True, samesite='lax')


def audit(db, request, kind, success, user_id=None, email=None):
    ip = request.client.host if request.client else 'unknown'
    db.add(AuthEvent(kind=kind, success=success, user_id=user_id,
                     subject_hash=keyed('email:' + email) if email else None, ip_hash=keyed('ip:' + ip)))


def _hit(db, key, limit, seconds):
    current = now()
    insert = pg_insert if db.bind.dialect.name == 'postgresql' else sqlite_insert
    table = AuthRateLimit.__table__
    statement = insert(table).values(key=keyed(key), count=1, resets_at=current + timedelta(seconds=seconds))
    statement = statement.on_conflict_do_update(index_elements=[table.c.key], set_={
        'count': case((table.c.resets_at <= current, 1), else_=table.c.count + 1),
        'resets_at': case((table.c.resets_at <= current, current + timedelta(seconds=seconds)), else_=table.c.resets_at),
    }).returning(table.c.count, table.c.resets_at)
    count, resets_at = db.execute(statement).one()
    db.commit()  # Persist failed attempts too; no request transaction is open yet.
    if count > limit:
        raise HTTPException(429, 'Too many attempts. Please try again later.', headers={
            'Retry-After': str(max(1, int((aware(resets_at) - current).total_seconds())))})


def throttle(db, request, action, email=None, sending=False):
    config = get_config()
    ip = request.client.host if request.client else 'unknown'
    _hit(db, f'ip:{action}:{ip}', config.ip_limit, config.rate_window)
    if email:
        _hit(db, f'email:{action}:{email}', config.email_limit, config.rate_window)
    if sending:
        # Shared across signup/resend/recovery, not bypassable by switching endpoint.
        _hit(db, 'send:ip:' + ip, config.ip_limit, config.send_window)
        _hit(db, 'send:email:' + email, config.send_limit, config.send_window)


class AuthSafetyMiddleware:
    """Bound even chunked auth bodies; never echo validation inputs/secrets."""
    def __init__(self, app, origins):
        self.app, self.origins = app, origins

    async def __call__(self, scope, receive, send):
        path = scope.get('path', '')
        is_guidance = '/guidance' in path or path.endswith(('/destination', '/checkpoint-confirmation', '/home'))
        is_command_poll = path.startswith('/robot/') and path.endswith('/command')
        if scope['type'] != 'http' or not (path == '/auth' or path.startswith('/auth/') or is_guidance or is_command_poll):
            return await self.app(scope, receive, send)
        async def safe_send(message):
            if message['type'] == 'http.response.start':
                headers = list(message.get('headers', []))
                headers.extend([(b'cache-control', b'no-store'), (b'pragma', b'no-cache'),
                                (b'x-content-type-options', b'nosniff'), (b'referrer-policy', b'no-referrer')])
                message = {**message, 'headers': headers}
            await send(message)
        headers = dict(scope.get('headers', []))
        if scope['method'] not in ('GET', 'HEAD', 'OPTIONS'):
            is_device = path.startswith('/robot/') and '/guidance/' in path
            if not is_device and headers.get(b'origin', b'').decode('latin-1') not in self.origins:
                return await JSONResponse({'detail': 'Untrusted request origin.'}, status_code=403)(scope, receive, safe_send)
            body = bytearray()
            while True:
                message = await receive()
                if message['type'] == 'http.disconnect':
                    return
                body.extend(message.get('body', b''))
                if len(body) > 16384:
                    return await JSONResponse({'detail': 'Request body too large.'}, status_code=413)(scope, receive, safe_send)
                if not message.get('more_body', False):
                    break
            if body and headers.get(b'content-type', b'').split(b';')[0].strip() != b'application/json':
                return await JSONResponse({'detail': 'JSON is required.'}, status_code=415)(scope, receive, safe_send)
            original_receive = receive
            delivered = False
            async def replay():
                nonlocal delivered
                if not delivered:
                    delivered = True
                    return {'type': 'http.request', 'body': bytes(body), 'more_body': False}
                return await original_receive()
            receive = replay
        await self.app(scope, receive, safe_send)
