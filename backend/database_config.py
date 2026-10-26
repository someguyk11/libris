"""Explicit database selection; never log a database URL or its credentials."""
import os
from sqlalchemy.engine import make_url


def database_url(environ=None):
    environ = os.environ if environ is None else environ
    value = (environ.get('DATABASE_URL') or '').strip()
    if not value:
        raise ValueError('DATABASE_URL is required. Explicitly select a local SQLite database or the reviewed PostgreSQL database; no SQLite fallback is used.')
    try:
        kind = make_url(value).get_backend_name()
    except Exception:
        raise ValueError('DATABASE_URL is invalid (value withheld).') from None
    if kind not in ('sqlite', 'postgresql'):
        raise ValueError('DATABASE_URL must select SQLite or PostgreSQL (value withheld).')
    return value
