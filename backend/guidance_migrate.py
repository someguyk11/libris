"""Additive guidance migration; never drops or rewrites existing Atlas data."""
from sqlalchemy import inspect, text
from guidance_models import GuidanceBase

def upgrade_guidance(engine):
    with engine.begin() as connection:
        columns = {c['name'] for c in inspect(connection).get_columns('shelves')}
        if 'destination_checkpoint_id' not in columns:
            kind = 'UUID' if engine.dialect.name == 'postgresql' else 'CHAR(32)'
            connection.execute(text(f'ALTER TABLE shelves ADD COLUMN destination_checkpoint_id {kind} REFERENCES checkpoints(id) ON DELETE SET NULL'))
        kind = 'UUID' if engine.dialect.name == 'postgresql' else 'CHAR(32)'
        robot_columns = {c['name'] for c in inspect(connection).get_columns('robots')}
        if 'home_checkpoint_id' not in robot_columns:
            connection.execute(text(f'ALTER TABLE robots ADD COLUMN home_checkpoint_id {kind} REFERENCES checkpoints(id) ON DELETE RESTRICT'))
        if 'guidance_state' not in robot_columns:
            connection.execute(text("ALTER TABLE robots ADD COLUMN guidance_state TEXT NOT NULL DEFAULT 'unknown'"))
        if inspect(connection).has_table('guidance_tasks') and 'atlas_otp_hash' not in {c['name'] for c in inspect(connection).get_columns('guidance_tasks')}:
            if engine.dialect.name == 'sqlite':
                # SQLite cannot alter CHECK/partial-index predicates. Preserve every
                # historical row, but never resume a pre-authorization task.
                old_columns = [c['name'] for c in inspect(connection).get_columns('guidance_tasks')]
                for index in inspect(connection).get_indexes('guidance_tasks'):
                    connection.execute(text(f'DROP INDEX "{index["name"]}"'))
                connection.execute(text('ALTER TABLE guidance_tasks RENAME TO guidance_tasks_previous'))
                GuidanceBase.metadata.create_all(connection)
                names = ','.join(old_columns)
                connection.execute(text(f'INSERT INTO guidance_tasks ({names}) SELECT {names} FROM guidance_tasks_previous'))
                connection.execute(text("UPDATE guidance_tasks SET status='failed', failure_reason='Authorization upgrade; create a new request.' WHERE status IN ('queued','awaiting_robot','running')"))
                connection.execute(text('DROP TABLE guidance_tasks_previous'))
            else:
                from pathlib import Path
                sql = (Path(__file__).with_name('migrations') / '20261019_atlas_otp_v2.sql').read_text(encoding='utf-8')
                connection.exec_driver_sql(sql.replace('BEGIN;', '', 1).strip().removesuffix('COMMIT;'))
        GuidanceBase.metadata.create_all(connection)
        if engine.dialect.name == 'postgresql':
            connection.execute(text('ALTER TABLE guidance_tasks ENABLE ROW LEVEL SECURITY'))
            connection.execute(text('REVOKE ALL ON guidance_tasks FROM PUBLIC, anon, authenticated'))
        connection.execute(text("INSERT INTO schema_migrations(version) VALUES ('20261019_guidance_v1') ON CONFLICT DO NOTHING"))
        connection.execute(text("INSERT INTO schema_migrations(version) VALUES ('20261019_atlas_otp_v2') ON CONFLICT DO NOTHING"))
