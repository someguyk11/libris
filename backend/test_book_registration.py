"""ISBN API tests against a private in-memory SQLite database, never app lifespan."""
import os
os.environ.setdefault('DATABASE_URL', 'sqlite://')
import unittest
from types import SimpleNamespace
from unittest.mock import AsyncMock, patch, MagicMock
from fastapi import FastAPI, Depends, HTTPException
from fastapi.testclient import TestClient
from sqlalchemy import create_engine, event
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool
from database import Base, get_db, BookModel, ShelfModel
from books import router, metadata_service, lock_editions
from book_metadata import MetadataError
from guidance_access import operational_guard, staff

ISBN = '9780306406157'
OTHER = '9780804429573'


class RegistrationTests(unittest.TestCase):
    def setUp(self):
        self.engine = create_engine('sqlite://', connect_args={'check_same_thread': False}, poolclass=StaticPool)
        @event.listens_for(self.engine, 'connect')
        def enable_foreign_keys(connection, _):
            connection.execute('PRAGMA foreign_keys=ON')
        Base.metadata.create_all(self.engine)
        self.Session = sessionmaker(bind=self.engine)
        def session():
            with self.Session() as db:
                yield db
        self.app = FastAPI()
        self.app.include_router(router, dependencies=[Depends(operational_guard)])
        self.app.dependency_overrides.update({get_db: session, operational_guard: lambda: None, staff: lambda: None})
        self.client = TestClient(self.app)
        with self.Session() as db:
            db.add_all([ShelfModel(shelf_id=1, shelf_name='First'), ShelfModel(shelf_id=2, shelf_name='Second')])
            db.commit()
        self.addCleanup(self.client.close)
        self.addCleanup(self.engine.dispose)

    def details(self, isbn=ISBN, **changes):
        return {'isbn': isbn, 'title': 'Reviewed title', 'author': 'Reviewed author', **changes}

    def register(self, isbn=ISBN, **changes):
        return self.client.post('/book/register', json={**self.details(isbn), 'shelf_id': 1, **changes})

    def count(self):
        with self.Session() as db:
            return db.query(BookModel).count()

    def legacy(self, isbn='0-306-40615-2'):
        with self.Session() as db:
            book = BookModel(isbn=isbn, title='Preserved title', author='Preserved author', shelf_id=1)
            db.add(book); db.commit(); db.refresh(book)
            return book.id

    def test_single_corrected_metadata_persists_original_isbn_and_optional_fields(self):
        response = self.register('0-306-40615-2', bibliography={'publisher': 'Corrected publisher', 'description': 'Reviewed description', 'edition': '2nd', 'cover_url': 'https://covers.openlibrary.org/b/id/1-L.jpg'}, require_new=True)
        self.assertEqual(response.status_code, 200, response.text)
        data = response.json()
        self.assertEqual(data['isbn'], ISBN)
        self.assertEqual(data['bibliography']['isbn10'], '0306406152')
        self.assertEqual(data['bibliography']['publisher'], 'Corrected publisher')
        self.assertIsNone(data['publish_date'])
        self.assertEqual(self.client.get('/book/search/%25').json()[0]['bibliography'], data['bibliography'])

    def test_catalogue_is_bounded_stably_ordered_and_detail_is_authoritative(self):
        with self.Session() as db:
            db.add_all([BookModel(isbn=f'fixture-{i}', title=f'Title {i:03}', author='Fixture', shelf_id=1) for i in range(105)])
            db.commit()
        first = self.client.get('/book/search/%25').json()
        second = self.client.get('/book/search/%25?limit=50&offset=50').json()
        last = self.client.get('/book/search/%25?limit=50&offset=100').json()
        self.assertEqual([len(first), len(second), len(last)], [50, 50, 5])
        self.assertEqual(len({book['id'] for book in first + second + last}), 105)
        self.assertEqual(first[0]['title'], 'Title 000')
        self.assertEqual(self.client.get(f"/book/{first[0]['id']}").json(), first[0])
        self.assertEqual(self.client.get('/book/999999').status_code, 404)
        for query in ('limit=101', 'limit=0', 'offset=-1', 'offset=100001'):
            self.assertEqual(self.client.get('/book/search/%25?' + query).status_code, 422)

    def test_catalogue_search_treats_sql_wildcards_as_literal_text(self):
        with self.Session() as db:
            db.add_all([BookModel(isbn='fixture-1', title='A_B 100%', author='Fixture', shelf_id=1), BookModel(isbn='fixture-2', title='AXB 100 books', author='Fixture', shelf_id=1)])
            db.commit()
        self.assertEqual(len(self.client.get('/book/search/A_B').json()), 1)
        self.assertEqual(len(self.client.get('/book/search/100%25').json()), 1)

    def test_server_validation_rejects_checksums_and_unreviewed_or_unsafe_metadata(self):
        for changes in ({'isbn': '9780306406158'}, {'isbn': '4006381333931'}, {'title': '  '}, {'author': ''},
                        {'author': 'a' * 101}, {'bibliography': {'isbn10': '080442957X'}},
                        {'bibliography': {'isbn10': ISBN}}, {'bibliography': {'cover_url': 'javascript:alert(1)'}},
                        {'bibliography': {'cover_url': 'https://user:pass@example.com/image'}}):
            with self.subTest(changes=changes):
                self.assertEqual(self.register(**changes).status_code, 422)
        self.assertEqual(self.count(), 0)

    def test_single_duplicate_alias_conflicts_and_explicit_move_preserves_metadata(self):
        identifier = self.legacy()
        self.assertEqual(self.register(require_new=True).status_code, 409)
        moved = self.register(shelf_id=2, expected_existing_id=identifier, title='Must not overwrite')
        self.assertEqual(moved.status_code, 200, moved.text)
        self.assertEqual(moved.json()['title'], 'Preserved title')
        self.assertEqual(moved.json()['shelf_id'], 2)
        self.assertEqual(self.count(), 1)
        self.assertEqual(self.register(expected_existing_id=identifier + 1).status_code, 409)

    def test_ambiguous_legacy_aliases_require_manual_resolution(self):
        self.legacy(); self.legacy(ISBN)
        self.assertEqual(self.register().status_code, 409)
        self.assertEqual(self.client.get('/book/metadata/' + ISBN).status_code, 409)

    def test_missing_existing_record_or_shelf_does_not_create(self):
        self.assertEqual(self.register(expected_existing_id=10).status_code, 409)
        self.assertEqual(self.register(shelf_id=999).status_code, 404)
        self.assertEqual(self.count(), 0)

    def test_metadata_existing_local_record_never_calls_provider_or_writes(self):
        identifier = self.legacy()
        with patch.object(metadata_service, 'lookup', new_callable=AsyncMock) as lookup:
            response = self.client.get('/book/metadata/' + ISBN)
            lookup.assert_not_awaited()
        self.assertEqual(response.json()['existing']['id'], identifier)
        self.assertEqual(self.count(), 1)

    def test_metadata_lookup_and_provider_errors_never_save(self):
        result = {'isbn': ISBN, 'found': True, 'metadata': {'title': 'Provider title'}}
        with patch.object(metadata_service, 'lookup', new_callable=AsyncMock, return_value=result) as lookup:
            response = self.client.get('/book/metadata/0-306-40615-2')
            self.assertEqual(response.json()['metadata']['title'], 'Provider title')
            lookup.assert_awaited_once_with(ISBN)
        with patch.object(metadata_service, 'lookup', new_callable=AsyncMock, side_effect=MetadataError(429, 'Provider busy')):
            response = self.client.get('/book/metadata/' + ISBN)
            self.assertEqual(response.status_code, 429)
            self.assertEqual(response.headers['retry-after'], '60')
        with patch.object(metadata_service, 'lookup', new_callable=AsyncMock) as lookup:
            self.assertEqual(self.client.get('/book/metadata/9780306406158').status_code, 422)
            lookup.assert_not_awaited()
        self.assertEqual(self.count(), 0)

    def test_bulk_creates_distinct_editions_and_retry_does_not_move_or_duplicate(self):
        payload = {'shelf_id': 2, 'books': [self.details(), self.details(OTHER)]}
        response = self.client.post('/book/register-bulk', json=payload)
        self.assertEqual(response.status_code, 200, response.text)
        self.assertEqual([r['status'] for r in response.json()['results']], ['created', 'created'])
        payload['shelf_id'] = 1
        retry = self.client.post('/book/register-bulk', json=payload)
        self.assertEqual([r['status'] for r in retry.json()['results']], ['existing', 'existing'])
        self.assertTrue(all(r['book']['shelf_id'] == 2 for r in retry.json()['results']))
        self.assertEqual(self.count(), 2)

    def test_bulk_skips_existing_legacy_metadata_and_duplicate_aliases(self):
        identifier = self.legacy()
        response = self.client.post('/book/register-bulk', json={'shelf_id': 2, 'books': [{'isbn': ISBN, 'existing_id': identifier}, self.details('0306406152'), self.details(OTHER)]})
        self.assertEqual(response.status_code, 200, response.text)
        results = response.json()['results']
        self.assertEqual([r['status'] for r in results], ['existing', 'duplicate', 'created'])
        self.assertEqual(results[0]['book']['shelf_id'], 1)
        self.assertEqual(results[0]['book']['title'], 'Preserved title')
        self.assertEqual(self.count(), 2)

    def test_bulk_invalid_item_missing_shelf_and_batch_limits_are_atomic(self):
        for payload, status in [({'shelf_id': 1, 'books': [self.details(), self.details(OTHER, title='')]}, 422),
                                ({'shelf_id': 999, 'books': [self.details()]}, 404),
                                ({'shelf_id': 1, 'books': []}, 422),
                                ({'shelf_id': 1, 'books': [self.details()] * 51}, 422)]:
            with self.subTest(payload=payload):
                self.assertEqual(self.client.post('/book/register-bulk', json=payload).status_code, status)
                self.assertEqual(self.count(), 0)

    def test_bulk_stale_existing_reference_rolls_back_earlier_insert(self):
        response = self.client.post('/book/register-bulk', json={'shelf_id': 1, 'books': [self.details(), {'isbn': OTHER, 'existing_id': 123}]})
        self.assertEqual(response.status_code, 409, response.text)
        self.assertEqual(self.count(), 0)

    def test_bulk_commit_conflict_rolls_back_all_and_can_be_retried(self):
        payload = {'shelf_id': 1, 'books': [self.details(), self.details(OTHER)]}
        with patch.object(self.Session.class_, 'commit', side_effect=IntegrityError('private SQL', {}, Exception('private constraint'))):
            response = self.client.post('/book/register-bulk', json=payload)
        self.assertEqual(response.status_code, 409, response.text)
        self.assertNotIn('private', response.text)
        self.assertEqual(self.count(), 0)
        self.assertEqual(self.client.post('/book/register-bulk', json=payload).status_code, 200)
        self.assertEqual(self.count(), 2)

    def test_metadata_and_bulk_require_trusted_staff_and_mutation_origin_csrf(self):
        self.app.dependency_overrides.pop(operational_guard)
        self.app.dependency_overrides.pop(staff)
        staff_id = '00000000-0000-0000-0000-000000000001'
        with patch('guidance_access.get_config', return_value=SimpleNamespace(enabled=True)), \
             patch('guidance_access.allowed_origins', return_value=['http://localhost:5173']), \
             patch.dict(os.environ, {'AUTH_STAFF_USER_IDS': staff_id}), \
             patch('guidance_access.authenticate') as authenticate:
            authenticate.side_effect = HTTPException(401, 'Sign in required')
            self.assertEqual(self.client.get('/book/metadata/' + ISBN).status_code, 401)
            authenticate.side_effect = None
            authenticate.return_value = SimpleNamespace(user_id='not-trusted-staff')
            self.assertEqual(self.client.get('/book/metadata/' + ISBN).status_code, 403)
            authenticate.return_value = SimpleNamespace(user_id=staff_id)
            payload = {'shelf_id': 1, 'books': [self.details()]}
            self.assertEqual(self.client.post('/book/register-bulk', json=payload).status_code, 403)
            authenticate.side_effect = HTTPException(403, 'Invalid CSRF token')
            self.assertEqual(self.client.post('/book/register-bulk', json=payload, headers={'origin': 'http://localhost:5173'}).status_code, 403)
            self.assertTrue(authenticate.call_args.kwargs['csrf'])
            authenticate.side_effect = None
            self.assertEqual(self.client.post('/book/register-bulk', json=payload, headers={'origin': 'http://localhost:5173'}).status_code, 200)

    def test_postgresql_lock_order_is_canonical_stable_and_deduplicated(self):
        db = MagicMock()
        db.get_bind.return_value.dialect.name = 'postgresql'
        lock_editions(db, [OTHER, ISBN, OTHER])
        self.assertEqual([call.args[1]['isbn'] for call in db.execute.call_args_list], sorted([ISBN, OTHER]))
