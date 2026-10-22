-- Additive upgrade after 20261019_guidance_v1.sql. Review before production use.
-- Keeps task history, books, shelves, checkpoints and recorded encoder routes.
BEGIN;
ALTER TABLE robots ADD COLUMN IF NOT EXISTS home_checkpoint_id UUID REFERENCES checkpoints(id) ON DELETE RESTRICT;
ALTER TABLE robots ADD COLUMN IF NOT EXISTS guidance_state TEXT NOT NULL DEFAULT 'unknown';
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS return_route_id UUID REFERENCES routes(id) ON DELETE RESTRICT;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS return_route_fingerprint TEXT;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS atlas_otp_hash TEXT;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS otp_expires_at TIMESTAMPTZ;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS otp_attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS verified_at TIMESTAMPTZ;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS session_id TEXT;
ALTER TABLE guidance_tasks DROP CONSTRAINT IF EXISTS ck_guidance_status;
ALTER TABLE guidance_tasks ADD CONSTRAINT ck_guidance_status CHECK (status IN ('waiting_for_atlas','awaiting_otp','queued','awaiting_robot','running','returning_home','expired','completed','failed','cancelled'));
ALTER TABLE guidance_tasks DROP CONSTRAINT IF EXISTS ck_guidance_otp_attempts;
ALTER TABLE guidance_tasks ADD CONSTRAINT ck_guidance_otp_attempts CHECK (otp_attempts BETWEEN 0 AND 5);
-- Pre-upgrade tasks have no consumed authorization and must not resume.
UPDATE guidance_tasks SET status='failed', failure_reason='Authorization upgrade; create a new request.', updated_at=CURRENT_TIMESTAMP
WHERE status IN ('queued','awaiting_robot','running') AND verified_at IS NULL;
ALTER TABLE guidance_tasks ALTER COLUMN status SET DEFAULT 'waiting_for_atlas';
DROP INDEX IF EXISTS uq_guidance_active_robot;
DROP INDEX IF EXISTS uq_guidance_active_user;
CREATE UNIQUE INDEX uq_guidance_active_robot ON guidance_tasks(robot_id) WHERE status IN ('awaiting_otp','queued','awaiting_robot','running','returning_home');
CREATE UNIQUE INDEX uq_guidance_active_user ON guidance_tasks(user_id) WHERE status IN ('waiting_for_atlas','awaiting_otp','queued','awaiting_robot','running','returning_home');
CREATE UNIQUE INDEX IF NOT EXISTS uq_guidance_session ON guidance_tasks(session_id);
ALTER TABLE guidance_tasks ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON guidance_tasks FROM PUBLIC, anon, authenticated;
INSERT INTO schema_migrations(version) VALUES ('20261019_atlas_otp_v2') ON CONFLICT DO NOTHING;
COMMIT;
