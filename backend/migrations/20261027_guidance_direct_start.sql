-- Retire Atlas codes after guidance v1 or v2. Account email codes are unaffected.
-- Keep historical tasks, live sessions, return routes and measured route data.
BEGIN;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS return_route_id UUID REFERENCES routes(id) ON DELETE RESTRICT;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS return_route_fingerprint TEXT;
ALTER TABLE guidance_tasks ADD COLUMN IF NOT EXISTS session_id TEXT;
UPDATE robots SET guidance_state='unknown' WHERE guidance_state='awaiting_otp' OR id IN (SELECT robot_id FROM guidance_tasks WHERE status='awaiting_otp');
UPDATE guidance_tasks SET status='cancelled', failure_reason='Atlas code step retired; create a new request.', updated_at=CURRENT_TIMESTAMP WHERE status='awaiting_otp';
UPDATE robots SET guidance_state='unknown' WHERE id IN (SELECT robot_id FROM guidance_tasks WHERE status IN ('queued','awaiting_robot','running','returning_home') AND session_id IS NULL);
UPDATE guidance_tasks SET status='failed', failure_reason='Session upgrade; create a new request.', updated_at=CURRENT_TIMESTAMP WHERE status IN ('queued','awaiting_robot','running','returning_home') AND session_id IS NULL;
ALTER TABLE guidance_tasks DROP CONSTRAINT IF EXISTS ck_guidance_status;
ALTER TABLE guidance_tasks DROP CONSTRAINT IF EXISTS ck_guidance_otp_attempts;
ALTER TABLE guidance_tasks DROP COLUMN IF EXISTS atlas_otp_hash;
ALTER TABLE guidance_tasks DROP COLUMN IF EXISTS otp_expires_at;
ALTER TABLE guidance_tasks DROP COLUMN IF EXISTS otp_attempts;
ALTER TABLE guidance_tasks DROP COLUMN IF EXISTS verified_at;
ALTER TABLE guidance_tasks ADD CONSTRAINT ck_guidance_status CHECK (status IN ('waiting_for_atlas','queued','awaiting_robot','running','returning_home','expired','completed','failed','cancelled'));
ALTER TABLE guidance_tasks ALTER COLUMN status SET DEFAULT 'waiting_for_atlas';
DROP INDEX IF EXISTS uq_guidance_active_robot;
DROP INDEX IF EXISTS uq_guidance_active_user;
CREATE UNIQUE INDEX uq_guidance_active_robot ON guidance_tasks(robot_id) WHERE status IN ('queued','awaiting_robot','running','returning_home');
CREATE UNIQUE INDEX uq_guidance_active_user ON guidance_tasks(user_id) WHERE status IN ('waiting_for_atlas','queued','awaiting_robot','running','returning_home');
CREATE UNIQUE INDEX IF NOT EXISTS uq_guidance_session ON guidance_tasks(session_id);
ALTER TABLE guidance_tasks ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON guidance_tasks FROM PUBLIC, anon, authenticated;
INSERT INTO schema_migrations(version) VALUES ('20261027_guidance_direct_start') ON CONFLICT DO NOTHING;
COMMIT;
