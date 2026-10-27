"""Additive private auth storage; deliberately separate from Atlas metadata."""
from datetime import datetime, timezone
from uuid import uuid4
from sqlalchemy import Column, String, Text, DateTime, Boolean, Integer, ForeignKey, CheckConstraint
from sqlalchemy.orm import declarative_base
from database import engine

SCHEMA = 'libris_auth' if engine.dialect.name == 'postgresql' else None
AuthBase = declarative_base()


def now():
    return datetime.now(timezone.utc)


def account_fk():
    return (SCHEMA + '.' if SCHEMA else '') + 'auth_accounts.user_id'


class AuthAccount(AuthBase):
    __tablename__ = 'auth_accounts'
    __table_args__ = (CheckConstraint("status IN ('active','suspended')", name='ck_auth_account_status'), {'schema': SCHEMA})
    user_id = Column(String(36), primary_key=True)
    email = Column(String(320), nullable=False, unique=True)
    otp_verified_at = Column(DateTime(timezone=True), nullable=False)
    status = Column(String(20), nullable=False, default='active')
    created_at = Column(DateTime(timezone=True), nullable=False, default=now)


class AuthSession(AuthBase):
    __tablename__ = 'auth_sessions'
    __table_args__ = (CheckConstraint("purpose IN ('user','recovery')", name='ck_auth_session_purpose'), {'schema': SCHEMA})
    id = Column(String(36), primary_key=True, default=lambda: str(uuid4()))
    token_hash = Column(String(64), nullable=False, unique=True)
    user_id = Column(String(36), ForeignKey(account_fk(), ondelete='CASCADE'), nullable=False, index=True)
    purpose = Column(String(20), nullable=False)
    access_encrypted = Column(Text, nullable=False)
    refresh_encrypted = Column(Text, nullable=False)
    access_expires_at = Column(DateTime(timezone=True), nullable=False)
    created_at = Column(DateTime(timezone=True), nullable=False, default=now)
    last_seen_at = Column(DateTime(timezone=True), nullable=False, default=now)
    expires_at = Column(DateTime(timezone=True), nullable=False, index=True)
    revoked_at = Column(DateTime(timezone=True), nullable=True)


class AuthProfile(AuthBase):
    __tablename__ = 'auth_profiles'
    __table_args__ = {'schema': SCHEMA}
    user_id = Column(String(36), ForeignKey(account_fk(), ondelete='CASCADE'), primary_key=True)
    username = Column(String(30), nullable=True, unique=True)
    display_name = Column(String(80), nullable=False, default='')
    updated_at = Column(DateTime(timezone=True), nullable=False, default=now, onupdate=now)


class AuthRateLimit(AuthBase):
    __tablename__ = 'auth_rate_limits'
    __table_args__ = {'schema': SCHEMA}
    key = Column(String(64), primary_key=True)
    count = Column(Integer, nullable=False)
    resets_at = Column(DateTime(timezone=True), nullable=False, index=True)


class AuthEvent(AuthBase):
    __tablename__ = 'auth_events'
    __table_args__ = {'schema': SCHEMA}
    id = Column(String(36), primary_key=True, default=lambda: str(uuid4()))
    kind = Column(String(50), nullable=False)
    success = Column(Boolean, nullable=False)
    user_id = Column(String(36), nullable=True, index=True)
    subject_hash = Column(String(64), nullable=True)
    ip_hash = Column(String(64), nullable=False)
    created_at = Column(DateTime(timezone=True), nullable=False, default=now, index=True)
