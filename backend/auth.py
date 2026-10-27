"""Supabase identity + opaque server sessions. No membership or staff grants."""
import secrets
import re
import threading
from contextlib import nullcontext
from dataclasses import dataclass, field
from datetime import timedelta
from uuid import UUID
from cryptography.fernet import Fernet, InvalidToken
from fastapi import APIRouter, Depends, HTTPException, Request, Response
from pydantic import BaseModel, ConfigDict, EmailStr, Field, SecretStr, TypeAdapter, field_validator
from sqlalchemy import select, update
from sqlalchemy.dialects.postgresql import insert as pg_insert
from sqlalchemy.dialects.sqlite import insert as sqlite_insert
from sqlalchemy.exc import IntegrityError
from database import get_db
from auth_config import get_config
from auth_models import AuthAccount, AuthSession, AuthProfile, now
from auth_provider import provider, ProviderError
from auth_security import aware, digest, csrf_token, check_csrf, cookie_value, set_cookie, clear_cookie, audit, throttle

_sqlite_lock = threading.RLock()
_email_adapter = TypeAdapter(EmailStr)


def enabled():
    if not get_config().enabled:
        raise HTTPException(503, 'Authentication is not configured.')


router = APIRouter(prefix='/auth', tags=['Authentication'], dependencies=[Depends(enabled)])


class EmailBody(BaseModel):
    model_config = ConfigDict(extra='forbid')
    email: EmailStr = Field(max_length=320)

    @field_validator('email', mode='before')
    @classmethod
    def normalize(cls, value):
        return value.strip().lower() if isinstance(value, str) else value


class PasswordBody(EmailBody):
    password: SecretStr = Field(min_length=1, max_length=128)


class ProfileBody(BaseModel):
    model_config = ConfigDict(extra='forbid')
    display_name: str = Field(default='', max_length=80)
    username: str = Field(default='', max_length=30)

    @field_validator('display_name', 'username', mode='before')
    @classmethod
    def trim(cls, value):
        return value.strip() if isinstance(value, str) else value

    @field_validator('username')
    @classmethod
    def valid_username(cls, value):
        value = value.lower().removeprefix('@')
        if value and not re.fullmatch(r'[a-z0-9_]{3,30}', value):
            raise ValueError('Username must contain 3–30 letters, numbers, or underscores.')
        return value


class SignupBody(PasswordBody, ProfileBody):
    pass


class CallbackBody(BaseModel):
    model_config = ConfigDict(extra='forbid')
    type: str = Field(pattern='^(signup|email|recovery)$')
    token_hash: SecretStr | None = Field(default=None, min_length=1, max_length=4096)
    access_token: SecretStr | None = Field(default=None, min_length=1, max_length=8192)
    refresh_token: SecretStr | None = Field(default=None, min_length=1, max_length=4096)


class OTPBody(EmailBody):
    token: SecretStr = Field(min_length=6, max_length=10)

    @field_validator('token')
    @classmethod
    def numeric(cls, value):
        if not value.get_secret_value().isascii() or not value.get_secret_value().isdigit():
            raise ValueError('Enter a numeric email verification code.')
        return value


class NewPasswordBody(BaseModel):
    model_config = ConfigDict(extra='forbid')
    password: SecretStr = Field(min_length=1, max_length=128)


def password_policy(password):
    if len(password) < get_config().min_password:
        raise HTTPException(400, 'Password does not meet the configured minimum length.')


def serialized(db):
    # PostgreSQL uses row locks. SQLite is a single-worker development fallback.
    return _sqlite_lock if db.bind.dialect.name == 'sqlite' else nullcontext()


def provider_failure(error, invalid='Invalid credentials or verification code.'):
    if error.status == 429:
        message = ('The email service has reached its sending limit. Please wait before requesting another email.'
                   if error.code == 'over_email_send_rate_limit' else 'Too many attempts. Please try again shortly.')
        raise HTTPException(429, message, headers={'Retry-After': str(error.retry_after or get_config().rate_window)})
    if error.status >= 500 or error.status < 400:
        raise HTTPException(503, 'Authentication service temporarily unavailable.')
    raise HTTPException(401, invalid)


def confirmed_user(access, expected_email=None):
    user = provider.user(access)  # Supabase verifies the JWT; never trust decoded claims/client identity.
    try:
        user_id = str(UUID(user['id']))
        email = str(_email_adapter.validate_python(user['email'])).lower()
        if not user.get('email_confirmed_at') or user.get('is_anonymous') or (expected_email and email != expected_email):
            raise ValueError('Unverified email')
    except (KeyError, ValueError, TypeError):
        raise ProviderError(401) from None
    return user_id, email, user.get('user_metadata') or {}


def encrypted_tokens(tokens):
    try:
        access, refresh = tokens['access_token'], tokens['refresh_token']
        expires = int(tokens['expires_in'])
        if not isinstance(access, str) or not isinstance(refresh, str) or not 1 <= len(access) <= 16384 or not 1 <= len(refresh) <= 4096 or expires <= 0:
            raise ValueError('Malformed token response')
    except (KeyError, TypeError, ValueError, OverflowError):
        raise ProviderError(503) from None
    cipher = Fernet(get_config().encryption_key.encode())
    return access, cipher.encrypt(access.encode()).decode(), cipher.encrypt(refresh.encode()).decode(), now() + timedelta(seconds=expires)


def revoke(session):
    session.revoked_at = now()
    session.access_encrypted = session.refresh_encrypted = ''


def revoke_all(db, user_id):
    db.execute(update(AuthSession).where(AuthSession.user_id == user_id, AuthSession.revoked_at.is_(None)).values(
        revoked_at=now(), access_encrypted='', refresh_encrypted=''))


def issue(db, request, response, tokens, email, purpose='user', verified_otp=False):
    access, access_encrypted, refresh_encrypted, access_expiry = encrypted_tokens(tokens)
    user_id, canonical_email, metadata = confirmed_user(access, email)
    with serialized(db):
        account = db.scalar(select(AuthAccount).where(AuthAccount.user_id == user_id).with_for_update())
        if account is None:
            insert = pg_insert if db.bind.dialect.name == 'postgresql' else sqlite_insert
            # Simultaneous successful OTP/recovery requests must not create two
            # accounts or fail a legitimate sign-in with a uniqueness error.
            db.execute(insert(AuthAccount).values(user_id=user_id, email=canonical_email,
                       otp_verified_at=now()).on_conflict_do_nothing(index_elements=[AuthAccount.user_id]))
            account = db.scalar(select(AuthAccount).where(AuthAccount.user_id == user_id).with_for_update())
        if account.status != 'active':
            raise HTTPException(403, 'Account unavailable.')
        if verified_otp:
            account.otp_verified_at = now()
        account.email = canonical_email
        # Metadata supplies profile defaults only, after identity confirmation.
        # It never grants privileges or reserves usernames before confirmation.
        profile = db.get(AuthProfile, user_id)
        if profile is None:
            insert = pg_insert if db.bind.dialect.name == 'postgresql' else sqlite_insert
            display = metadata.get('display_name', '') if isinstance(metadata, dict) else ''
            username = str(metadata.get('username', '')).lower() if isinstance(metadata, dict) else ''
            username = username if re.fullmatch(r'[a-z0-9_]{3,30}', username) else None
            db.execute(insert(AuthProfile).values(user_id=user_id, display_name=str(display)[:80], username=username).on_conflict_do_nothing())
            # A metadata username can be taken before confirmation; signing in
            # must still work and the user can choose another in their profile.
            db.execute(insert(AuthProfile).values(user_id=user_id, display_name=str(display)[:80]).on_conflict_do_nothing(index_elements=[AuthProfile.user_id]))
        old_cookie = cookie_value(request)
        old = db.scalar(select(AuthSession).where(AuthSession.token_hash == digest(old_cookie)).with_for_update()) if old_cookie else None
        if old:
            revoke(old)
        config = get_config()
        duration = config.recovery_seconds if purpose == 'recovery' else config.session_seconds
        raw = secrets.token_urlsafe(32)
        session = AuthSession(token_hash=digest(raw), user_id=user_id, purpose=purpose,
                              access_encrypted=access_encrypted, refresh_encrypted=refresh_encrypted,
                              access_expires_at=access_expiry, expires_at=now() + timedelta(seconds=duration))
        db.add(session)
        audit(db, request, 'recovery_verified' if purpose == 'recovery' else 'signed_in', True, user_id, canonical_email)
        db.commit()
    set_cookie(response, raw, duration)
    return {'authenticated': purpose == 'user', 'purpose': purpose, 'csrf_token': csrf_token(raw),
            'user': public_user(user_id, canonical_email, db)}


@dataclass
class SessionContext:
    id: str
    user_id: str
    email: str
    purpose: str
    expires_at: object
    access: str = field(repr=False)
    csrf: str = field(repr=False)


def authenticate(db, request, *, csrf=False, purpose='user', refresh=False):
    enabled()
    raw = cookie_value(request)
    if not raw:
        raise HTTPException(401, 'Authentication required.')
    if csrf:
        check_csrf(request, raw)
    with serialized(db):
        # Lock account before session, consistently with issue/logout-all.
        user_id = db.scalar(select(AuthSession.user_id).where(AuthSession.token_hash == digest(raw)))
        account = db.scalar(select(AuthAccount).where(AuthAccount.user_id == user_id).with_for_update()) if user_id else None
        session = db.scalar(select(AuthSession).where(AuthSession.token_hash == digest(raw)).with_for_update().execution_options(populate_existing=True))
        current = now()
        if not session or session.revoked_at or not account or account.status != 'active':
            raise HTTPException(401, 'Session unavailable. Please sign in again.')
        if aware(session.expires_at) <= current or aware(session.last_seen_at) + timedelta(seconds=get_config().idle_seconds) <= current:
            revoke(session)
            db.commit()
            raise HTTPException(401, 'Session expired. Please sign in again.')
        if purpose and session.purpose != purpose:
            raise HTTPException(403, 'This session cannot perform that operation.')
        cipher = Fernet(get_config().encryption_key.encode())
        try:
            access = cipher.decrypt(session.access_encrypted.encode()).decode()
            if refresh or aware(session.access_expires_at) <= current + timedelta(seconds=30):
                tokens = provider.refresh(cipher.decrypt(session.refresh_encrypted.encode()).decode())
                access, encrypted_access, encrypted_refresh, expiry = encrypted_tokens(tokens)
                # Store rotated refresh token before further provider calls, so a
                # transient /user failure does not discard the new credential.
                session.access_encrypted, session.refresh_encrypted = encrypted_access, encrypted_refresh
                session.access_expires_at = expiry
            confirmed_id, email, _ = confirmed_user(access)
            if confirmed_id != session.user_id or email != account.email:
                raise ProviderError(401)
        except InvalidToken:
            revoke(session)
            db.commit()
            raise HTTPException(401, 'Session unavailable. Please sign in again.') from None
        except ProviderError as error:
            if error.status in (400, 401, 403, 422):
                revoke(session)
            db.commit()
            provider_failure(error)
        session.last_seen_at = current
        context = SessionContext(session.id, session.user_id, email, session.purpose, aware(session.expires_at), access, csrf_token(raw))
        db.commit()
        return context


def require_user(request: Request, db=Depends(get_db)):
    """Reusable verified-reader dependency; NOT a librarian/tenant permission check."""
    return authenticate(db, request)


def public_user(user_id, email, db):
    from guidance_access import staff_ids
    profile = db.get(AuthProfile, user_id)
    return {'id': user_id, 'email': email, 'email_verified': True, 'is_staff': user_id in staff_ids(),
            'display_name': profile.display_name if profile else '', 'username': profile.username if profile else None}


def public_session(context, db):
    return {'authenticated': context.purpose == 'user', 'purpose': context.purpose,
            'csrf_token': context.csrf, 'expires_at': context.expires_at.isoformat(),
            'user': public_user(context.user_id, context.email, db)}


def send_email(db, request, email, action, operation):
    throttle(db, request, action, email, sending=True)
    try:
        operation()
    except ProviderError as error:
        audit(db, request, action, False, email=email)
        db.commit()
        if error.status == 429 or error.status >= 500:
            provider_failure(error)
        if error.code in ('weak_password', 'password_too_short'):
            raise HTTPException(400, 'Choose a stronger password that meets the password policy.')
        # Do not expose existing/nonexistent/unconfirmed account distinctions.
        return {'message': 'If the request is eligible, check your email for a verification code.'}
    audit(db, request, action, True, email=email)
    db.commit()
    return {'message': 'If the request is eligible, check your email for a verification code.'}


@router.post('/signup', status_code=202)
def signup(body: SignupBody, request: Request, db=Depends(get_db)):
    password = body.password.get_secret_value()
    password_policy(password)
    return send_email(db, request, str(body.email), 'signup', lambda: provider.signup(str(body.email), password,
                      {'display_name': body.display_name, 'username': body.username}))


@router.post('/callback')
def callback(body: CallbackBody, request: Request, response: Response, db=Depends(get_db)):
    throttle(db, request, 'callback')
    try:
        if body.token_hash and not body.access_token and not body.refresh_token:
            tokens = provider.verify_link(body.token_hash.get_secret_value(), body.type)
        elif body.access_token and body.refresh_token and not body.token_hash:
            # Default Supabase email links already exchanged the one-time code.
            # Refresh server-side; never trust expiry or identity from the URL.
            tokens = provider.refresh(body.refresh_token.get_secret_value())
            old_id, _, _ = confirmed_user(body.access_token.get_secret_value())
            new_id, _, _ = confirmed_user(tokens['access_token'])
            if old_id != new_id:
                raise ProviderError(401)
        else:
            raise HTTPException(400, 'Invalid email link. Request a new one.')
        return issue(db, request, response, tokens, None,
                     purpose='recovery' if body.type == 'recovery' else 'user', verified_otp=True)
    except ProviderError as error:
        provider_failure(error, 'This email link is invalid or expired. Request a new one.')


@router.post('/otp/resend', status_code=202)
def resend(body: EmailBody, request: Request, db=Depends(get_db)):
    return send_email(db, request, str(body.email), 'resend', lambda: provider.resend(str(body.email)))


@router.post('/otp/verify')
def verify(body: OTPBody, request: Request, response: Response, db=Depends(get_db)):
    email = str(body.email)
    throttle(db, request, 'verify', email)
    try:
        tokens = provider.verify(email, body.token.get_secret_value())
        return issue(db, request, response, tokens, email, verified_otp=True)
    except ProviderError as error:
        audit(db, request, 'verification_failed', False, email=email)
        db.commit()
        provider_failure(error)


@router.post('/login')
def login(body: PasswordBody, request: Request, response: Response, db=Depends(get_db)):
    email = str(body.email)
    throttle(db, request, 'login', email)
    try:
        tokens = provider.login(email, body.password.get_secret_value())
        return issue(db, request, response, tokens, email)
    except ProviderError as error:
        audit(db, request, 'login_failed', False, email=email)
        db.commit()
        provider_failure(error, 'Invalid credentials or email verification is required.')


@router.get('/session')
def session(request: Request, db=Depends(get_db)):
    return public_session(authenticate(db, request, purpose=None), db)


@router.get('/me')
def me(context=Depends(require_user), db=Depends(get_db)):
    return public_session(context, db)


@router.patch('/profile')
def update_profile(body: ProfileBody, request: Request, db=Depends(get_db)):
    context = authenticate(db, request, csrf=True)
    throttle(db, request, 'profile')
    with serialized(db):
        account = db.scalar(select(AuthAccount).where(AuthAccount.user_id == context.user_id).with_for_update())
        stored = db.get(AuthSession, context.id, populate_existing=True)
        if not account or account.status != 'active' or not stored or stored.revoked_at:
            raise HTTPException(401, 'Session unavailable. Please sign in again.')
        profile = db.get(AuthProfile, context.user_id)
        if profile is None:
            profile = AuthProfile(user_id=context.user_id)
            db.add(profile)
        profile.display_name, profile.username = body.display_name, body.username or None
        try:
            db.commit()
        except IntegrityError:
            db.rollback()
            raise HTTPException(409, 'That username is already taken. Choose another.') from None
    return public_session(context, db)


@router.post('/refresh')
def refresh(request: Request, response: Response, db=Depends(get_db)):
    throttle(db, request, 'refresh')
    context = authenticate(db, request, csrf=True, purpose=None, refresh=True)
    set_cookie(response, cookie_value(request), (context.expires_at - now()).total_seconds())
    return public_session(context, db)


def logout_impl(request, response, db, all_sessions=False):
    throttle(db, request, 'logout')
    raw = cookie_value(request)
    access = None
    with serialized(db):
        user_id = db.scalar(select(AuthSession.user_id).where(AuthSession.token_hash == digest(raw))) if raw else None
        if user_id:
            check_csrf(request, raw)
            db.scalar(select(AuthAccount).where(AuthAccount.user_id == user_id).with_for_update())
            stored = db.scalar(select(AuthSession).where(AuthSession.token_hash == digest(raw)).with_for_update().execution_options(populate_existing=True))
            if all_sessions and stored.revoked_at:
                raise HTTPException(401, 'Authentication required.')
            if not stored.revoked_at:
                try:
                    access = Fernet(get_config().encryption_key.encode()).decrypt(stored.access_encrypted.encode()).decode()
                except InvalidToken:
                    pass
                if all_sessions:
                    # Expired/revoked cookies must not authorize account-wide actions.
                    if aware(stored.expires_at) <= now() or aware(stored.last_seen_at) + timedelta(seconds=get_config().idle_seconds) <= now():
                        raise HTTPException(401, 'Sign in again before signing out all devices.')
                    revoke_all(db, user_id)
                else:
                    revoke(stored)
                audit(db, request, 'signed_out_all' if all_sessions else 'signed_out', True, user_id)
                db.commit()
        elif all_sessions:
            raise HTTPException(401, 'Authentication required.')
    clear_cookie(response)
    provider_revoked = False
    if access:
        try:
            provider.logout(access, all_sessions)
            provider_revoked = True
        except ProviderError:
            pass  # Local revocation succeeds even if Supabase is unavailable.
    return {'signed_out': True, 'provider_revoked': provider_revoked}


@router.post('/logout')
def logout(request: Request, response: Response, db=Depends(get_db)):
    return logout_impl(request, response, db)


@router.post('/logout-all')
def logout_all(request: Request, response: Response, db=Depends(get_db)):
    return logout_impl(request, response, db, True)


@router.post('/password/forgot', status_code=202)
def forgot(body: EmailBody, request: Request, db=Depends(get_db)):
    return send_email(db, request, str(body.email), 'recover', lambda: provider.recover(str(body.email)))


@router.post('/password/verify')
def verify_recovery(body: OTPBody, request: Request, response: Response, db=Depends(get_db)):
    email = str(body.email)
    throttle(db, request, 'verify', email)  # Shared attempt budget with signup OTPs.
    try:
        tokens = provider.verify(email, body.token.get_secret_value(), recovery=True)
        return issue(db, request, response, tokens, email, purpose='recovery', verified_otp=True)
    except ProviderError as error:
        audit(db, request, 'recovery_failed', False, email=email)
        db.commit()
        provider_failure(error)


@router.post('/password/reset')
def reset_password(body: NewPasswordBody, request: Request, response: Response, db=Depends(get_db)):
    password = body.password.get_secret_value()
    password_policy(password)
    throttle(db, request, 'password')
    context = authenticate(db, request, csrf=True, purpose='recovery')
    with serialized(db):
        account = db.scalar(select(AuthAccount).where(AuthAccount.user_id == context.user_id).with_for_update())
        stored = db.scalar(select(AuthSession).where(AuthSession.id == context.id).with_for_update().execution_options(populate_existing=True))
        # authenticate() committed before this transaction. Recheck under the
        # account/session locks so logout or another reset cannot race a replay.
        current = now()
        if (not account or account.status != 'active' or not stored or stored.revoked_at
                or stored.purpose != 'recovery' or aware(stored.expires_at) <= current
                or aware(stored.last_seen_at) + timedelta(seconds=get_config().idle_seconds) <= current):
            raise HTTPException(401, 'Session unavailable. Please sign in again.')
        try:
            provider.password(context.access, password)
        except ProviderError as error:
            provider_failure(error, 'Password could not be changed. Check the configured password policy.')
        revoke_all(db, context.user_id)
        audit(db, request, 'password_reset', True, context.user_id)
        db.commit()
    clear_cookie(response)
    provider_revoked = False
    try:
        provider.logout(context.access, True)
        provider_revoked = True
    except ProviderError:
        pass
    return {'password_reset': True, 'sign_in_required': True, 'provider_revoked': provider_revoked}
