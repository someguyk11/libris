"""Create only auth tables. No existing table is replaced or altered."""
from sqlalchemy import text
from auth_models import AuthBase

VERSION = '20261019_auth_v1'
PROFILE_VERSION = '20261027_auth_profiles_v2'


def upgrade_auth(engine):
    with engine.begin() as connection:
        if engine.dialect.name == 'postgresql':
            connection.execute(text('CREATE SCHEMA IF NOT EXISTS libris_auth'))
            connection.execute(text('REVOKE ALL ON SCHEMA libris_auth FROM PUBLIC, anon, authenticated'))
        AuthBase.metadata.create_all(connection)
        if engine.dialect.name == 'postgresql':
            for table in AuthBase.metadata.sorted_tables:
                name = 'libris_auth.' + table.name
                connection.execute(text(f'ALTER TABLE {name} ENABLE ROW LEVEL SECURITY'))
                connection.execute(text(f'REVOKE ALL ON TABLE {name} FROM PUBLIC, anon, authenticated'))
        connection.execute(text('CREATE TABLE IF NOT EXISTS schema_migrations (version VARCHAR(100) PRIMARY KEY)'))
        connection.execute(text('INSERT INTO schema_migrations(version) VALUES (:version) ON CONFLICT DO NOTHING'), {'version': VERSION})
        connection.execute(text('INSERT INTO schema_migrations(version) VALUES (:version) ON CONFLICT DO NOTHING'), {'version': PROFILE_VERSION})
