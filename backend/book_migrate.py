"""Targeted, additive bibliography upgrade. Importing this module never connects."""
import argparse
from pathlib import Path
from sqlalchemy import JSON, create_engine, inspect, text
from sqlalchemy.exc import SQLAlchemyError

VERSION = '20261024_book_bibliography'


def schema_status(connection):
    """Read-only inspection. Version markers never substitute for schema checks."""
    if connection.dialect.name not in ('sqlite', 'postgresql'):
        raise RuntimeError('Bibliography migration supports only SQLite and PostgreSQL.')
    inspector = inspect(connection)
    recorded = False
    if inspector.has_table('schema_migrations'):
        recorded = connection.execute(text('SELECT version FROM schema_migrations WHERE version = :version'), {'version': VERSION}).first() is not None
    if not inspector.has_table('books'):
        if recorded:
            raise RuntimeError('Bibliography migration is recorded but books is missing. Explicit schema review is required.')
        return {'schema': 'books_missing', 'recorded': False}
    columns = {column['name']: column for column in inspector.get_columns('books')}
    column = columns.get('bibliography')
    if column is None:
        if recorded:
            raise RuntimeError('Bibliography migration is recorded but books.bibliography is missing. Explicit schema review is required.')
        return {'schema': 'upgrade_required', 'recorded': False}
    if not isinstance(column['type'], JSON) or not column['nullable']:
        raise RuntimeError('books.bibliography must be nullable JSON (or PostgreSQL JSONB). No automatic type conversion is permitted.')
    return {'schema': 'compatible', 'recorded': recorded}


def require_compatible_books(engine):
    """Startup preflight: do not mutate an existing incompatible database."""
    with engine.connect() as connection:
        status = schema_status(connection)
    if status['schema'] == 'upgrade_required':
        raise RuntimeError('Database upgrade required: books.bibliography is missing. Run python book_migrate.py --check, then review the Phase 1 database guide. Startup did not apply this migration.')
    # A new database has no books table; the existing create_all path creates the
    # current model. An existing table must already pass the checks above.


def upgrade_books(engine):
    """Apply ONLY the nullable column and its marker in a single transaction."""
    with engine.connect() as connection:
        if connection.dialect.name == 'sqlite':
            # Python sqlite's legacy transaction mode does not BEGIN for DDL.
            # Explicit BEGIN keeps column + marker atomic and serializes writers.
            connection.exec_driver_sql('BEGIN IMMEDIATE')
        elif connection.dialect.name == 'postgresql':
            connection.execute(text("SET LOCAL lock_timeout = '5s'"))
            connection.execute(text('SELECT pg_advisory_xact_lock(hashtext(:version))'), {'version': VERSION})
        try:
            status = schema_status(connection)
            if status['schema'] == 'books_missing':
                raise RuntimeError('books is missing. This targeted upgrade requires an existing catalogue; it does not initialize databases.')
            if status['schema'] == 'upgrade_required':
                connection.execute(text('ALTER TABLE books ADD COLUMN bibliography JSON'))
            if schema_status(connection)['schema'] != 'compatible':
                raise RuntimeError('Bibliography migration postcondition failed.')
            connection.execute(text('CREATE TABLE IF NOT EXISTS schema_migrations (version VARCHAR(100) PRIMARY KEY)'))
            connection.execute(text('INSERT INTO schema_migrations(version) VALUES (:version) ON CONFLICT DO NOTHING'), {'version': VERSION})
            connection.commit()
        except Exception:
            connection.rollback()
            raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Read-only schema check (default).')
    mode.add_argument('--apply', action='store_true', help='Explicitly apply to the reviewed DATABASE_URL. Back up and test first.')
    args = parser.parse_args(argv)
    engine = None
    try:
        from dotenv import load_dotenv
        from database_config import database_url
        from sqlalchemy.engine import make_url
        load_dotenv(Path(__file__).with_name('.env'))
        url = database_url()
        parsed = make_url(url)
        if parsed.get_backend_name() == 'sqlite' and parsed.database not in (None, '', ':memory:'):
            # Even --check must not accidentally create an empty SQLite file.
            if parsed.query.get('uri') or not Path(parsed.database).is_file():
                raise ValueError('Select an existing SQLite file with a plain DATABASE_URL; no file was created.')
            if not args.apply:
                from sqlalchemy.engine import URL
                url = URL.create('sqlite+pysqlite', database=Path(parsed.database).resolve().as_uri(), query={'mode': 'ro', 'uri': 'true'})
        engine = create_engine(url)
        if args.apply:
            upgrade_books(engine)
        with engine.connect() as connection:
            if not args.apply and connection.dialect.name == 'postgresql':
                connection.execute(text('SET TRANSACTION READ ONLY'))
            status = schema_status(connection)
        print(f"{engine.dialect.name}: {status['schema']}; {VERSION} recorded={status['recorded']}")
        return 0 if status['schema'] == 'compatible' else 1
    except (ValueError, RuntimeError) as error:
        print(str(error))
        return 1
    except SQLAlchemyError:
        print('Database operation failed; connection details and SQL parameters withheld. Verify configuration, permissions, and schema using trusted server-side tooling.')
        return 1
    finally:
        if engine is not None:
            engine.dispose()


if __name__ == '__main__':
    raise SystemExit(main())
