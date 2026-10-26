"""Server-only configuration. Provider configuration is never sent to Vue."""
import os
from dataclasses import dataclass
from functools import lru_cache
from urllib.parse import urlsplit
from cryptography.fernet import Fernet


def positive(name, default):
    value = int(os.getenv(name, str(default)))
    if value < 1:
        raise ValueError(f'{name} must be positive')
    return value


def allowed_origins():
    origins = tuple(value.strip() for value in os.getenv('CORS_ORIGINS', 'http://localhost:5173,http://127.0.0.1:5173').split(',') if value.strip())
    if not origins or any('*' in origin or urlsplit(origin).scheme not in ('http', 'https') or not urlsplit(origin).hostname or urlsplit(origin).username or urlsplit(origin).password or urlsplit(origin).path or urlsplit(origin).query or urlsplit(origin).fragment for origin in origins):
        raise ValueError('CORS_ORIGINS must contain exact origins without paths or wildcards')
    return origins


@dataclass(frozen=True)
class AuthConfig:
    enabled: bool
    url: str
    public_key: str
    encryption_key: str
    security_secret: str
    secure_cookie: bool
    cookie_samesite: str
    session_seconds: int
    idle_seconds: int
    recovery_seconds: int
    min_password: int
    ip_limit: int
    email_limit: int
    rate_window: int
    send_limit: int
    send_window: int

    @property
    def cookie_name(self):
        return '__Host-libris_session' if self.secure_cookie else 'libris_session_dev'


@lru_cache
def get_config():
    config = AuthConfig(
        enabled=os.getenv('AUTH_ENABLED', 'false').lower() == 'true',
        url=os.getenv('SUPABASE_URL', '').rstrip('/'),
        public_key=os.getenv('SUPABASE_PUBLISHABLE_KEY', os.getenv('SUPABASE_ANON_KEY', '')),
        encryption_key=os.getenv('AUTH_SESSION_ENCRYPTION_KEY', ''),
        security_secret=os.getenv('AUTH_SECURITY_SECRET', ''),
        secure_cookie=os.getenv('AUTH_COOKIE_SECURE', 'true').lower() == 'true',
        cookie_samesite=os.getenv('AUTH_COOKIE_SAMESITE', 'lax').lower(),
        session_seconds=positive('AUTH_SESSION_SECONDS', 604800),
        idle_seconds=positive('AUTH_IDLE_SECONDS', 1800),
        recovery_seconds=positive('AUTH_RECOVERY_SECONDS', 600),
        min_password=positive('AUTH_MIN_PASSWORD_LENGTH', 12),
        ip_limit=positive('AUTH_IP_LIMIT', 30),
        email_limit=positive('AUTH_EMAIL_LIMIT', 10),
        rate_window=positive('AUTH_RATE_WINDOW_SECONDS', 300),
        send_limit=positive('AUTH_EMAIL_SEND_LIMIT', 3),
        send_window=positive('AUTH_EMAIL_SEND_WINDOW_SECONDS', 900),
    )
    if config.cookie_samesite not in ('lax', 'strict', 'none'):
        raise ValueError('AUTH_COOKIE_SAMESITE must be lax, strict, or none')
    if config.cookie_samesite == 'none' and not config.secure_cookie:
        raise ValueError('SameSite=None requires AUTH_COOKIE_SECURE=true')
    if not config.enabled:
        return config
    parsed = urlsplit(config.url)
    development = os.getenv('AUTH_ENV', 'production') == 'development'
    local_url = parsed.hostname in ('localhost', '127.0.0.1') and development
    if (parsed.scheme != 'https' and not (local_url and parsed.scheme == 'http')) or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment or parsed.path not in ('', '/'):
        raise ValueError('SUPABASE_URL must be an HTTPS project origin')
    if not config.public_key or config.public_key.startswith('sb_secret_'):
        raise ValueError('Supply a Supabase publishable/anon key, not an administrative secret')
    # Legacy anon keys are JWTs. Reject accidentally supplied service-role keys;
    # this is configuration inspection, NOT JWT authentication/verification.
    if config.public_key.count('.') == 2:
        import base64
        import json
        try:
            payload = config.public_key.split('.')[1]
            if json.loads(base64.urlsafe_b64decode(payload + '=' * (-len(payload) % 4))).get('role') != 'anon':
                raise ValueError('Only a publishable or legacy anon key is permitted')
        except (ValueError, KeyError) as error:
            raise ValueError('Invalid Supabase public key configuration') from error
    Fernet(config.encryption_key.encode())
    if len(config.security_secret) < 32:
        raise ValueError('AUTH_SECURITY_SECRET must contain at least 32 random characters')
    if not config.secure_cookie and not development:
        raise ValueError('Insecure cookies are allowed only with AUTH_ENV=development')
    origins = allowed_origins()
    if not development and any(not origin.startswith('https://') for origin in origins):
        raise ValueError('Production authentication requires HTTPS frontend origins')
    if config.min_password > 128:
        raise ValueError('AUTH_MIN_PASSWORD_LENGTH cannot exceed 128')
    return config
