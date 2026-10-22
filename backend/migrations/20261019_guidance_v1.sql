-- Additive migration after Atlas V1 and auth V1. Review before production use.
-- No legacy Atlas tables, routes, books or shelves are dropped or rewritten.
BEGIN;
ALTER TABLE shelves ADD COLUMN IF NOT EXISTS destination_checkpoint_id UUID REFERENCES checkpoints(id) ON DELETE SET NULL;
CREATE TABLE IF NOT EXISTS guidance_tasks (
    id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    book_id INTEGER NOT NULL REFERENCES books(id) ON DELETE RESTRICT,
    shelf_id INTEGER NOT NULL REFERENCES shelves(shelf_id) ON DELETE RESTRICT,
    robot_id TEXT NOT NULL REFERENCES robots(id) ON DELETE RESTRICT,
    route_id UUID NOT NULL REFERENCES routes(id) ON DELETE RESTRICT,
    route_fingerprint TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'queued',
    progress_index INTEGER NOT NULL DEFAULT 0,
    failure_reason TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT ck_guidance_status CHECK (status IN ('queued','awaiting_robot','running','completed','failed','cancelled')),
    CONSTRAINT ck_guidance_progress CHECK (progress_index >= 0)
);
CREATE INDEX IF NOT EXISTS ix_guidance_tasks_user_id ON guidance_tasks(user_id);
CREATE INDEX IF NOT EXISTS ix_guidance_tasks_robot_id ON guidance_tasks(robot_id);
CREATE UNIQUE INDEX IF NOT EXISTS uq_guidance_active_robot ON guidance_tasks(robot_id) WHERE status IN ('queued','awaiting_robot','running');
CREATE UNIQUE INDEX IF NOT EXISTS uq_guidance_active_user ON guidance_tasks(user_id) WHERE status IN ('queued','awaiting_robot','running');
ALTER TABLE guidance_tasks ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON guidance_tasks FROM PUBLIC, anon, authenticated;
CREATE TABLE IF NOT EXISTS schema_migrations(version TEXT PRIMARY KEY);
INSERT INTO schema_migrations(version) VALUES ('20261019_guidance_v1') ON CONFLICT DO NOTHING;
COMMIT;
