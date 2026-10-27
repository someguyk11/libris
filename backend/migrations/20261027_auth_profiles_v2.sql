-- Apply after 20261019_auth_v1.sql, against the SAME database as FastAPI.
-- Private profile data; no changes to the public catalogue/Atlas schema.
BEGIN;
CREATE TABLE IF NOT EXISTS libris_auth.auth_profiles (
    user_id VARCHAR(36) PRIMARY KEY REFERENCES libris_auth.auth_accounts(user_id) ON DELETE CASCADE,
    username VARCHAR(30) UNIQUE,
    display_name VARCHAR(80) NOT NULL DEFAULT '',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
ALTER TABLE libris_auth.auth_profiles ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON TABLE libris_auth.auth_profiles FROM PUBLIC, anon, authenticated;
INSERT INTO public.schema_migrations(version) VALUES ('20261027_auth_profiles_v2') ON CONFLICT DO NOTHING;
COMMIT;
