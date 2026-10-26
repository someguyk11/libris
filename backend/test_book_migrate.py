"""Upgrade tests use temporary databases only; never start the app lifespan."""
import contextlib
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

# Existing test harness selects an isolated engine before importing app models.
import test_api  # noqa: F401
from sqlalchemy import create_engine, event, inspect, select, text
from sqlalchemy.engine import URL
from sqlalchemy.exc import IntegrityError, OperationalError
from database import BookModel
from database_config import database_url
from book_migrate import VERSION, main, require_compatible_books, schema_status, upgrade_books
from migrate import upgrade


class DatabaseConfigurationTests(unittest.TestCase):
    def test_missing_blank_or_invalid_url_fails_without_secret(self):
        for env in ({}, {'DATABASE_URL': ''}, {'DATABASE_URL': '  '},
                    {'DATABASE_URL': 'do-not-print-this-secret'},
                    {'DATABASE_URL': 'mysql://user:do-not-print-this-secret@host/db'}):
            with self.subTest(env_present=bool(env)):
                with self.assertRaises(ValueError) as caught:
                    database_url(env)
                self.assertNotIn('do-not-print-this-secret', str(caught.exception))

    def test_explicit_local_and_postgresql_selection(self):
        for url in ('sqlite://', 'sqlite:///explicit-local.db', 'postgresql+psycopg2://user:password@example.invalid/db'):
            self.assertEqual(database_url({'DATABASE_URL': url}), url)


class BookSchemaUpgradeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='libris-schema-')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'previous schema.db'
        self.url = URL.create('sqlite', database=str(self.path))
        self.engine = create_engine(self.url)
        self.addCleanup(self.engine.dispose)

        @event.listens_for(self.engine, 'connect')
        def enable_foreign_keys(connection, _):
            connection.execute('PRAGMA foreign_keys=ON')

        with self.engine.begin() as connection:
            connection.execute(text('CREATE TABLE shelves (shelf_id INTEGER PRIMARY KEY, shelf_name VARCHAR NOT NULL UNIQUE)'))
            connection.execute(text('CREATE TABLE books (id INTEGER PRIMARY KEY, isbn VARCHAR NOT NULL UNIQUE, title VARCHAR NOT NULL, author VARCHAR NOT NULL, publish_date VARCHAR, available BOOLEAN NOT NULL, created_at DATETIME, updated_at DATETIME, shelf_id INTEGER NOT NULL REFERENCES shelves(shelf_id))'))
            connection.execute(text('CREATE INDEX ix_books_title ON books(title)'))
            connection.execute(text("INSERT INTO shelves VALUES (7, 'Original shelf')"))
            connection.execute(text("INSERT INTO books VALUES (23, '9780306406157', 'Original title', 'Original author', '1980', 1, '2026-01-02 03:04:05', '2026-01-03 03:04:05', 7)"))
            connection.execute(text('CREATE TABLE unrelated (id INTEGER PRIMARY KEY, value TEXT)'))
            connection.execute(text("INSERT INTO unrelated VALUES (1, 'preserve me')"))

    def status(self):
        with self.engine.connect() as connection:
            return schema_status(connection)

    def rows(self):
        with self.engine.connect() as connection:
            return connection.execute(text('SELECT id,isbn,title,author,publish_date,available,created_at,updated_at,shelf_id FROM books')).all()

    def run_cli(self, args, url=None):
        out = io.StringIO()
        with patch.dict(os.environ, {'DATABASE_URL': url or str(self.url)}), patch('dotenv.load_dotenv'), contextlib.redirect_stdout(out):
            result = main(args)
        return result, out.getvalue()

    def test_previous_schema_reproduces_orm_failure_then_upgrade_preserves_data(self):
        before = self.rows()
        with self.engine.connect() as connection:
            with self.assertRaises(OperationalError):
                connection.execute(select(BookModel))
        self.assertEqual(self.status()['schema'], 'upgrade_required')
        upgrade_books(self.engine)
        self.assertEqual(self.status(), {'schema': 'compatible', 'recorded': True})
        self.assertEqual(self.rows(), before)
        with self.engine.connect() as connection:
            row = connection.execute(select(BookModel)).mappings().one()
            self.assertIsNone(row['bibliography'])
            self.assertEqual(connection.execute(text('SELECT value FROM unrelated')).scalar_one(), 'preserve me')
            self.assertEqual(connection.execute(text('SELECT shelf_name FROM shelves')).scalar_one(), 'Original shelf')
            self.assertEqual(connection.execute(text('PRAGMA foreign_key_check')).all(), [])
            self.assertEqual(inspect(connection).get_indexes('books')[0]['name'], 'ix_books_title')
        with self.engine.begin() as connection:
            connection.execute(BookModel.__table__.update().values(bibliography={'publisher': 'Corrected publisher', 'description': 'Preserved metadata'}))
        upgrade_books(self.engine)
        with self.engine.connect() as connection:
            self.assertEqual(connection.execute(select(BookModel.bibliography)).scalar_one()['publisher'], 'Corrected publisher')
            self.assertEqual(connection.execute(text('SELECT count(*) FROM schema_migrations WHERE version=:v'), {'v': VERSION}).scalar_one(), 1)
        with self.engine.begin() as connection:
            with self.assertRaises(IntegrityError):
                connection.execute(text("INSERT INTO books (isbn,title,author,available,shelf_id) VALUES ('9780306406157','Duplicate','Author',1,7)"))
        with self.engine.begin() as connection:
            with self.assertRaises(IntegrityError):
                connection.execute(text('UPDATE books SET shelf_id=999'))

    def test_startup_refuses_old_schema_before_any_other_ddl(self):
        tables = inspect(self.engine).get_table_names()
        with self.assertRaisesRegex(RuntimeError, 'Database upgrade required'):
            upgrade(self.engine)
        self.assertEqual(inspect(self.engine).get_table_names(), tables)
        self.assertEqual(self.status()['schema'], 'upgrade_required')

    def test_readonly_check_does_not_change_database_and_apply_is_explicit(self):
        before = self.path.read_bytes()
        result, output = self.run_cli(['--check'])
        self.assertEqual(result, 1, output)
        self.assertIn('upgrade_required', output)
        self.assertEqual(self.path.read_bytes(), before)
        result, output = self.run_cli(['--apply'])
        self.assertEqual(result, 0, output)
        before = self.path.read_bytes()
        self.assertEqual(self.run_cli([])[0], 0)
        self.assertEqual(self.path.read_bytes(), before)

    def test_missing_path_is_not_created_and_errors_hide_url(self):
        missing = Path(self.temp.name) / 'must-not-create.db'
        for mode in ('--check', '--apply'):
            result, output = self.run_cli([mode], str(URL.create('sqlite', database=str(missing))))
            self.assertEqual(result, 1)
            self.assertNotIn(str(missing), output)
            self.assertFalse(missing.exists())

    def test_false_marker_does_not_silently_skip(self):
        with self.engine.begin() as connection:
            connection.execute(text('CREATE TABLE schema_migrations (version VARCHAR(100) PRIMARY KEY)'))
            connection.execute(text('INSERT INTO schema_migrations VALUES (:v)'), {'v': VERSION})
        with self.assertRaisesRegex(RuntimeError, 'recorded but'):
            upgrade_books(self.engine)
        self.assertNotIn('bibliography', [column['name'] for column in inspect(self.engine).get_columns('books')])

    def test_wrong_type_or_not_nullable_is_rejected_without_conversion(self):
        for definition in ('TEXT', "JSON NOT NULL DEFAULT '{}'" ):
            other = create_engine('sqlite://')
            try:
                with other.begin() as connection:
                    connection.execute(text(f'CREATE TABLE books (id INTEGER PRIMARY KEY, bibliography {definition})'))
                with self.assertRaisesRegex(RuntimeError, 'nullable JSON'):
                    upgrade_books(other)
                self.assertNotIn('schema_migrations', inspect(other).get_table_names())
            finally:
                other.dispose()

    def test_compatible_untracked_column_is_validated_then_recorded(self):
        with self.engine.begin() as connection:
            connection.execute(text('ALTER TABLE books ADD COLUMN bibliography JSON'))
        require_compatible_books(self.engine)
        self.assertEqual(self.status(), {'schema': 'compatible', 'recorded': False})
        upgrade_books(self.engine)
        self.assertTrue(self.status()['recorded'])

    def test_failure_recording_marker_rolls_back_column_and_keeps_rows(self):
        before = self.rows()

        def fail_marker(connection, cursor, statement, parameters, context, executemany):
            if statement.startswith('INSERT INTO schema_migrations'):
                raise RuntimeError('simulated marker failure')

        event.listen(self.engine, 'before_cursor_execute', fail_marker)
        try:
            with self.assertRaisesRegex(RuntimeError, 'simulated marker failure'):
                upgrade_books(self.engine)
        finally:
            event.remove(self.engine, 'before_cursor_execute', fail_marker)
        self.assertEqual(self.status()['schema'], 'upgrade_required')
        self.assertNotIn('schema_migrations', inspect(self.engine).get_table_names())
        self.assertEqual(self.rows(), before)
        upgrade_books(self.engine)
        self.assertTrue(self.status()['recorded'])

    def test_new_database_preflight_is_allowed_but_targeted_upgrade_does_not_initialize(self):
        other = create_engine('sqlite://')
        try:
            require_compatible_books(other)
            with self.assertRaisesRegex(RuntimeError, 'does not initialize'):
                upgrade_books(other)
            self.assertEqual(inspect(other).get_table_names(), [])
        finally:
            other.dispose()


if __name__ == '__main__':
    unittest.main()
