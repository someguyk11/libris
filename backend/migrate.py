"""Finalize only Atlas tables. Never reset the Libris/Supabase database."""
from pathlib import Path
from sqlalchemy import inspect, text
from database import Base, engine, RobotModel, CheckpointModel, RouteModel
from book_migrate import require_compatible_books

VERSION = '20261018_atlas_v1_encoder_v2'
SQL_FILE = Path(__file__).with_name('migrations') / '20261018_atlas_v1_encoder.sql'

def upgrade(target_engine=engine):
    # Fail before any startup DDL on a previous catalogue schema. Bibliography
    # upgrades are an explicit operator action, never an automatic production edit.
    require_compatible_books(target_engine)
    inspector = inspect(target_engine)
    columns = lambda table: {column['name'] for column in inspector.get_columns(table)} if inspector.has_table(table) else set()
    legacy = 'robot_id' in columns('robots') or 'shelf_id' in columns('routes') or inspector.has_table('route_steps')
    with target_engine.connect() as connection:
        if legacy:
            for table in ('route_steps', 'routes', 'robots'):
                if inspector.has_table(table) and connection.execute(text(f'SELECT COUNT(*) FROM {table}')).scalar():
                    raise RuntimeError('Legacy Atlas tables contain records. Encoder values cannot be invented from timed routes. Back up and explicitly clear obsolete Atlas records before restructuring.')
        else:
            for model in (RobotModel, CheckpointModel, RouteModel):
                existing = columns(model.__tablename__)
                expected = set(model.__table__.columns.keys())
                if model is RobotModel:
                    expected -= {'home_checkpoint_id', 'guidance_state'}
                    existing -= {'home_checkpoint_id', 'guidance_state'}
                if existing and expected != existing:
                    raise RuntimeError(f'Unknown {model.__tablename__} schema; explicit schema mapping is required.')
    if target_engine.dialect.name == 'postgresql':
        # The standalone SQL has BEGIN/COMMIT for SQL Editor use. Let SQLAlchemy
        # own that outer transaction here, so errors reliably roll back the upgrade.
        sql = SQL_FILE.read_text(encoding='utf-8').replace('\nBEGIN;\n', '\n', 1).strip().removesuffix('COMMIT;')
        with target_engine.begin() as connection:
            connection.execute(text(sql))
            # Only create missing unrelated tables, never alter existing ones.
            Base.metadata.create_all(bind=connection)
    else:
        # Existing SQLite is retained solely for local development/tests. Production
        # PostgreSQL uses actual UUID[], JSONB, TEXT IDs and TIMESTAMPTZ from the SQL.
        with target_engine.begin() as connection:
            if legacy:
                for table in ('route_steps', 'routes', 'robots'):
                    if inspector.has_table(table):
                        connection.execute(text(f'DROP TABLE {table}'))
            Base.metadata.create_all(bind=connection)
            connection.execute(text('CREATE TABLE IF NOT EXISTS schema_migrations (version VARCHAR(100) PRIMARY KEY)'))
            connection.execute(text('INSERT INTO schema_migrations(version) VALUES (:version) ON CONFLICT DO NOTHING'), {'version': VERSION})

if __name__ == '__main__':
    upgrade()
    print(f'Applied {VERSION} to configured {engine.dialect.name} database; unrelated Libris tables untouched.')
