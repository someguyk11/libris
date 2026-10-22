-- Additive Supabase/PostgreSQL migration. Never resets Atlas, books or shelves.
-- Run as the database owner, NOT via the browser or the public PostgREST API.
BEGIN;
CREATE SCHEMA IF NOT EXISTS libris_auth;
REVOKE ALL ON SCHEMA libris_auth FROM PUBLIC, anon, authenticated;

CREATE TABLE IF NOT EXISTS libris_auth.auth_accounts (
    user_id VARCHAR(36) PRIMARY KEY,
    email VARCHAR(320) NOT NULL UNIQUE,
    otp_verified_at TIMESTAMPTZ NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'active',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT ck_auth_account_status CHECK (status IN ('active', 'suspended'))
);
CREATE TABLE IF NOT EXISTS libris_auth.auth_sessions (
    id VARCHAR(36) PRIMARY KEY,
    token_hash VARCHAR(64) NOT NULL UNIQUE,
    user_id VARCHAR(36) NOT NULL REFERENCES libris_auth.auth_accounts(user_id) ON DELETE CASCADE,
    purpose VARCHAR(20) NOT NULL,
    access_encrypted TEXT NOT NULL,
    refresh_encrypted TEXT NOT NULL,
    access_expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    last_seen_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at TIMESTAMPTZ NOT NULL,
    revoked_at TIMESTAMPTZ,
    CONSTRAINT ck_auth_session_purpose CHECK (purpose IN ('user', 'recovery'))
);
CREATE INDEX IF NOT EXISTS ix_libris_auth_auth_sessions_user_id ON libris_auth.auth_sessions(user_id);
CREATE INDEX IF NOT EXISTS ix_libris_auth_auth_sessions_expires_at ON libris_auth.auth_sessions(expires_at);
CREATE TABLE IF NOT EXISTS libris_auth.auth_rate_limits (
    key VARCHAR(64) PRIMARY KEY,
    count INTEGER NOT NULL,
    resets_at TIMESTAMPTZ NOT NULL
);
CREATE INDEX IF NOT EXISTS ix_libris_auth_auth_rate_limits_resets_at ON libris_auth.auth_rate_limits(resets_at);
CREATE TABLE IF NOT EXISTS libris_auth.auth_events (
    id VARCHAR(36) PRIMARY KEY,
    kind VARCHAR(50) NOT NULL,
    success BOOLEAN NOT NULL,
    user_id VARCHAR(36),
    subject_hash VARCHAR(64),
    ip_hash VARCHAR(64) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ix_libris_auth_auth_events_user_id ON libris_auth.auth_events(user_id);
CREATE INDEX IF NOT EXISTS ix_libris_auth_auth_events_created_at ON libris_auth.auth_events(created_at);

ALTER TABLE libris_auth.auth_accounts ENABLE ROW LEVEL SECURITY;
ALTER TABLE libris_auth.auth_sessions ENABLE ROW LEVEL SECURITY;
ALTER TABLE libris_auth.auth_rate_limits ENABLE ROW LEVEL SECURITY;
ALTER TABLE libris_auth.auth_events ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON ALL TABLES IN SCHEMA libris_auth FROM PUBLIC, anon, authenticated;
-- No public policies. Keep this schema out of Supabase's exposed schemas.
-- FastAPI uses a server-only privileged DB connection; never expose its secrets.
CREATE TABLE IF NOT EXISTS public.schema_migrations (version VARCHAR(100) PRIMARY KEY);
INSERT INTO public.schema_migrations(version) VALUES ('20261019_auth_v1') ON CONFLICT DO NOTHING;
COMMIT;
