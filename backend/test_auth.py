"""Auth HTTP/security tests. Mock Supabase, isolated SQLite; never real mail/accounts."""
import os
import unittest
from types import SimpleNamespace
import httpx
from datetime import timedelta
from unittest.mock import patch
from uuid import uuid4
from cryptography.fernet import Fernet
from fastapi import Response
from fastapi.testclient import TestClient
from sqlalchemy import select, text

# Reuse the isolated engine fixture; this module never opens the on-disk database.
import test_api
import database
import auth
from main import app
from auth_config import get_config
from auth_migrate import upgrade_auth
from auth_models import AuthBase, AuthAccount, AuthSession, AuthRateLimit, AuthEvent, now
from auth_provider import SupabaseAuth, ProviderError
from auth_security import set_cookie, clear_cookie, digest

ORIGIN = {'Origin': 'http://localhost:5173'}
EMAIL = 'reader@example.com'
PASSWORD = 'long-test-password-42'


class FakeSupabase:
    def __init__(self):
        self.id = str(uuid4())
        self.verified = False
        self.used = set()
        self.serial = 0
        self.password_value = PASSWORD
        self.logout_fails = False

    def tokens(self):
        self.serial += 1
        return {'access_token': f'access-{self.serial}', 'refresh_token': f'refresh-{self.serial}', 'expires_in': 3600}

    def signup(self, email, password):
        pass

    def resend(self, email):
        pass

    def recover(self, email):
        pass

    def verify(self, email, token, recovery=False):
        expected = '654321' if recovery else '123456'
        if token != expected or token in self.used:
            raise ProviderError(403)
        self.used.add(token)
        self.verified = True
        return self.tokens()

    def login(self, email, password):
        if not self.verified or password != self.password_value:
            raise ProviderError(400)
        return self.tokens()

    def user(self, access):
        return {'id': self.id, 'email': EMAIL, 'email_confirmed_at': now().isoformat() if self.verified else None, 'is_anonymous': False}

    def refresh(self, refresh):
        return self.tokens()

    def logout(self, access, all_sessions=False):
        if self.logout_fails:
            raise ProviderError(503)

    def password(self, access, password):
        self.password_value = password


class AuthTests(unittest.TestCase):
    def setUp(self):
        self.env = patch.dict(os.environ, {
            'AUTH_ENABLED': 'true', 'AUTH_ENV': 'development', 'AUTH_COOKIE_SECURE': 'false', 'AUTH_COOKIE_SAMESITE': 'lax',
            'SUPABASE_URL': 'https://test-project.supabase.co', 'SUPABASE_PUBLISHABLE_KEY': 'sb_publishable_test',
            'AUTH_SESSION_ENCRYPTION_KEY': Fernet.generate_key().decode(), 'AUTH_SECURITY_SECRET': 'test-secret-' * 5,
            'CORS_ORIGINS': 'http://localhost:5173,http://127.0.0.1:5173',
            'AUTH_IP_LIMIT': '30', 'AUTH_EMAIL_LIMIT': '10', 'AUTH_EMAIL_SEND_LIMIT': '3',
        })
        self.env.start()
        get_config.cache_clear()
        upgrade_auth(database.engine)
        with database.engine.begin() as connection:
            for table in reversed(AuthBase.metadata.sorted_tables):
                connection.execute(table.delete())
        self.fake = FakeSupabase()
        self.mock = patch.object(auth, 'provider', self.fake)
        self.mock.start()
        self.client = TestClient(app)
        self.addCleanup(self.client.close)
        self.addCleanup(self.mock.stop)
        self.addCleanup(get_config.cache_clear)
        self.addCleanup(self.env.stop)

    def post(self, path, body=None, csrf=None, client=None):
        headers = {**ORIGIN, **({'X-CSRF-Token': csrf} if csrf else {})}
        return (client or self.client).post('/auth' + path, json=body, headers=headers)

    def verify(self):
        response = self.post('/otp/verify', {'email': EMAIL, 'token': '123456'})
        self.assertEqual(response.status_code, 200, response.text)
        return response

    def test_signup_does_not_create_local_account_or_session(self):
        response = self.post('/signup', {'email': ' Reader@Example.com ', 'password': PASSWORD})
        self.assertEqual(response.status_code, 202)
        self.assertNotIn('set-cookie', response.headers)
        with database.SessionLocal() as db:
            self.assertIsNone(db.scalar(select(AuthAccount)))
            self.assertIsNone(db.scalar(select(AuthSession)))
        self.assertEqual(self.post('/login', {'email': EMAIL, 'password': PASSWORD}).status_code, 401)

    def test_staff_indicator_comes_from_server_uuid_allowlist(self):
        with patch.dict(os.environ, {'AUTH_STAFF_USER_IDS': ''}):
            self.assertFalse(self.verify().json()['user']['is_staff'])
        with patch.dict(os.environ, {'AUTH_STAFF_USER_IDS': self.fake.id}):
            self.assertTrue(self.client.get('/auth/session').json()['user']['is_staff'])
        with patch.dict(os.environ, {'AUTH_STAFF_USER_IDS': ''}):
            self.assertFalse(self.client.get('/auth/session').json()['user']['is_staff'])

    def test_otp_creates_only_verified_reader_and_hides_credentials(self):
        response = self.verify()
        body = response.json()
        self.assertTrue(body['authenticated'])
        self.assertNotIn('role', body)
        self.assertNotIn('membership', body)
        self.assertNotIn('access-', response.text)
        cookie = self.client.cookies.get('libris_session_dev')
        self.assertIn('HttpOnly', response.headers['set-cookie'])
        self.assertIn('SameSite=lax', response.headers['set-cookie'])
        with database.SessionLocal() as db:
            stored = db.scalar(select(AuthSession))
            self.assertEqual(stored.token_hash, digest(cookie))
            self.assertNotEqual(stored.access_encrypted, 'access-1')
            self.assertEqual(Fernet(get_config().encryption_key.encode()).decrypt(stored.refresh_encrypted.encode()), b'refresh-1')
            self.assertIsNotNone(db.get(AuthAccount, self.fake.id).otp_verified_at)
        self.assertEqual(self.client.get('/auth/me').status_code, 200)

    def test_invalid_and_replayed_otp_never_create_sessions(self):
        self.assertEqual(self.post('/otp/verify', {'email': EMAIL, 'token': '000000'}).status_code, 401)
        self.verify()
        replay = self.post('/otp/verify', {'email': EMAIL, 'token': '123456'})
        self.assertEqual(replay.status_code, 401)
        self.assertNotIn('set-cookie', replay.headers)

    def test_provider_confirmation_alone_cannot_bypass_application_otp(self):
        self.fake.verified = True
        response = self.post('/login', {'email': EMAIL, 'password': PASSWORD})
        self.assertEqual(response.status_code, 401)
        self.assertNotIn('set-cookie', response.headers)

    def test_unconfirmed_anonymous_and_wrong_identity_are_rejected(self):
        for claims in (
            {'id': self.fake.id, 'email': EMAIL, 'email_confirmed_at': None},
            {'id': self.fake.id, 'email': EMAIL, 'email_confirmed_at': 'yes', 'is_anonymous': True},
            {'id': self.fake.id, 'email': 'other@example.com', 'email_confirmed_at': 'yes'},
            {'id': 'invalid', 'email': EMAIL, 'email_confirmed_at': 'yes'},
        ):
            with patch.object(self.fake, 'verify', return_value=self.fake.tokens()), patch.object(self.fake, 'user', return_value=claims):
                self.assertEqual(self.post('/otp/verify', {'email': EMAIL, 'token': '123456'}).status_code, 401)

    def test_origin_and_csrf_guards(self):
        self.assertEqual(self.client.post('/auth/signup', json={'email': EMAIL, 'password': PASSWORD}).status_code, 403)
        self.assertEqual(self.client.post('/auth/signup', headers={'Origin': 'https://attacker.example'}, json={}).status_code, 403)
        token = self.verify().json()['csrf_token']
        self.assertEqual(self.post('/refresh').status_code, 403)
        self.assertEqual(self.post('/logout', csrf='forged').status_code, 403)
        self.assertEqual(self.post('/refresh', csrf=token).status_code, 200)
        self.assertEqual(self.post('/logout', csrf=token).status_code, 200)

    def test_refresh_rotation_is_server_side_and_survives_transient_user_failure(self):
        token = self.verify().json()['csrf_token']
        with patch.object(self.fake, 'user', side_effect=ProviderError(503)):
            response = self.post('/refresh', csrf=token)
        self.assertEqual(response.status_code, 503)
        with database.SessionLocal() as db:
            stored = db.scalar(select(AuthSession))
            self.assertEqual(Fernet(get_config().encryption_key.encode()).decrypt(stored.refresh_encrypted.encode()), b'refresh-2')
            self.assertIsNone(stored.revoked_at)
        self.assertEqual(self.client.get('/auth/me').status_code, 200)

    def test_expired_idle_suspended_and_provider_rejected_sessions_fail(self):
        self.verify()
        with database.SessionLocal() as db:
            stored = db.scalar(select(AuthSession))
            stored.last_seen_at = now() - timedelta(days=1)
            db.commit()
        self.assertEqual(self.client.get('/auth/me').status_code, 401)
        self.post('/login', {'email': EMAIL, 'password': PASSWORD})
        with database.SessionLocal() as db:
            db.get(AuthAccount, self.fake.id).status = 'suspended'
            db.commit()
        self.assertEqual(self.client.get('/auth/me').status_code, 401)
        with database.SessionLocal() as db:
            db.get(AuthAccount, self.fake.id).status = 'active'
            db.commit()
        with patch.object(self.fake, 'user', side_effect=ProviderError(401)):
            self.assertEqual(self.client.get('/auth/me').status_code, 401)
        with database.SessionLocal() as db:
            self.assertTrue(all(s.revoked_at is not None for s in db.scalars(select(AuthSession))))

    def test_logout_locally_revokes_even_when_provider_is_unavailable(self):
        csrf = self.verify().json()['csrf_token']
        old_cookie = self.client.cookies.get('libris_session_dev')
        self.fake.logout_fails = True
        response = self.post('/logout', csrf=csrf)
        self.assertEqual(response.status_code, 200)
        self.assertFalse(response.json()['provider_revoked'])
        self.client.cookies.set('libris_session_dev', old_cookie)
        self.assertEqual(self.client.get('/auth/me').status_code, 401)

    def test_logout_all_revokes_second_browser_and_cannot_reuse_cookie(self):
        csrf = self.verify().json()['csrf_token']
        other = TestClient(app)
        self.addCleanup(other.close)
        response = self.post('/login', {'email': EMAIL, 'password': PASSWORD}, client=other)
        self.assertEqual(response.status_code, 200)
        old = self.client.cookies.get('libris_session_dev')
        self.assertEqual(self.post('/logout-all', csrf=csrf).status_code, 200)
        self.assertEqual(other.get('/auth/me').status_code, 401)
        self.client.cookies.set('libris_session_dev', old)
        self.assertEqual(self.post('/logout-all', csrf=csrf).status_code, 401)

    def test_password_recovery_is_limited_and_revokes_old_sessions(self):
        self.verify()
        other = TestClient(app)
        self.addCleanup(other.close)
        response = self.post('/password/verify', {'email': EMAIL, 'token': '654321'}, client=other)
        self.assertEqual(response.status_code, 200, response.text)
        self.assertFalse(response.json()['authenticated'])
        self.assertEqual(other.get('/auth/me').status_code, 403)
        self.assertEqual(other.get('/auth/session').json()['purpose'], 'recovery')
        csrf = response.json()['csrf_token']
        self.assertEqual(self.post('/password/reset', {'password': 'new-strong-password-42'}, client=other).status_code, 403)
        result = self.post('/password/reset', {'password': 'new-strong-password-42'}, csrf=csrf, client=other)
        self.assertEqual(result.status_code, 200, result.text)
        self.assertTrue(result.json()['sign_in_required'])
        self.assertEqual(self.client.get('/auth/me').status_code, 401)
        self.assertEqual(other.get('/auth/me').status_code, 401)
        self.assertEqual(self.post('/login', {'email': EMAIL, 'password': PASSWORD}).status_code, 401)
        self.assertEqual(self.post('/login', {'email': EMAIL, 'password': 'new-strong-password-42'}).status_code, 200)

    def test_user_session_cannot_reset_password(self):
        csrf = self.verify().json()['csrf_token']
        self.assertEqual(self.post('/password/reset', {'password': PASSWORD}, csrf=csrf).status_code, 403)

    def test_recovery_revoked_after_authentication_cannot_change_password(self):
        csrf = self.post('/password/verify', {'email': EMAIL, 'token': '654321'}).json()['csrf_token']
        original = auth.authenticate

        def revoke_between_transactions(db, request, **kwargs):
            context = original(db, request, **kwargs)
            auth.revoke_all(db, context.user_id)
            db.commit()
            return context

        with patch.object(auth, 'authenticate', side_effect=revoke_between_transactions), patch.object(self.fake, 'password') as password:
            result = self.post('/password/reset', {'password': 'new-strong-password-42'}, csrf=csrf)
            self.assertEqual(result.status_code, 401)
            password.assert_not_called()

    def test_shared_send_rate_budget_and_hashed_storage(self):
        os.environ['AUTH_EMAIL_SEND_LIMIT'] = '2'
        get_config.cache_clear()
        self.assertEqual(self.post('/signup', {'email': EMAIL, 'password': PASSWORD}).status_code, 202)
        self.assertEqual(self.post('/otp/resend', {'email': EMAIL}).status_code, 202)
        result = self.post('/password/forgot', {'email': EMAIL})
        self.assertEqual(result.status_code, 429)
        self.assertGreater(int(result.headers['retry-after']), 0)
        with database.SessionLocal() as db:
            for row in db.scalars(select(AuthRateLimit)):
                self.assertEqual(len(row.key), 64)
                self.assertNotIn(EMAIL, row.key)
            for event in db.scalars(select(AuthEvent)):
                self.assertNotIn(EMAIL, str(event.__dict__))
                self.assertNotIn(PASSWORD, str(event.__dict__))

    def test_attempt_budget_persists_failures_and_ignores_client_forwarded_ip(self):
        os.environ['AUTH_IP_LIMIT'] = '2'
        get_config.cache_clear()
        for i in range(2):
            self.assertEqual(self.client.post('/auth/login', json={'email': EMAIL, 'password': 'bad'}, headers={**ORIGIN, 'X-Forwarded-For': f'1.2.3.{i}'}).status_code, 401)
        self.assertEqual(self.post('/login', {'email': EMAIL, 'password': 'bad'}).status_code, 429)

    def test_generic_email_responses_validation_and_http_hardening(self):
        success = self.post('/password/forgot', {'email': EMAIL})
        with patch.object(self.fake, 'recover', side_effect=ProviderError(404)):
            unknown = self.post('/password/forgot', {'email': 'unknown@example.com'})
        self.assertEqual(success.json(), unknown.json())
        response = self.post('/signup', {'email': EMAIL, 'password': PASSWORD, 'role': 'owner'})
        self.assertEqual(response.status_code, 422)
        self.assertNotIn(PASSWORD, response.text)
        invalid = self.post('/otp/verify', {'email': EMAIL, 'token': 'secret-otp'})
        self.assertEqual(invalid.status_code, 422)
        self.assertNotIn('secret-otp', invalid.text)
        self.assertEqual(invalid.headers['cache-control'], 'no-store')
        self.assertEqual(invalid.headers['x-content-type-options'], 'nosniff')
        self.assertEqual(self.client.post('/auth/signup', content='x' * 17000, headers={**ORIGIN, 'Content-Type': 'application/json'}).status_code, 413)
        self.assertEqual(self.client.post('/auth/signup', content='email=a', headers={**ORIGIN, 'Content-Type': 'text/plain'}).status_code, 415)

    def test_disabled_auth_fails_closed_and_migration_is_additive_idempotent(self):
        upgrade_auth(database.engine)
        with database.engine.connect() as connection:
            self.assertEqual(connection.scalar(text("SELECT count(*) FROM schema_migrations WHERE version='20261019_auth_v1'")), 1)
        os.environ['AUTH_ENABLED'] = 'false'
        get_config.cache_clear()
        self.assertEqual(self.client.get('/auth/me').status_code, 503)
        self.assertEqual(self.client.get('/ping').status_code, 200)

    def test_cross_site_cookie_configuration(self):
        os.environ.update(AUTH_ENV='production', AUTH_COOKIE_SECURE='true',
                          AUTH_COOKIE_SAMESITE='none', CORS_ORIGINS='https://libotsystems.netlify.app')
        get_config.cache_clear()
        for cookie_action in (lambda response: set_cookie(response, 'random-session-cookie', 300), clear_cookie):
            response = Response()
            cookie_action(response)
            header = response.headers['set-cookie']
            for expected in ('__Host-libris_session=', 'HttpOnly', 'Secure', 'Path=/', 'SameSite=none'):
                self.assertIn(expected, header)
            self.assertNotIn('Domain=', header)
        os.environ.update(AUTH_ENV='development', AUTH_COOKIE_SECURE='false')
        get_config.cache_clear()
        with self.assertRaisesRegex(ValueError, 'SameSite=None requires'):
            get_config()
        os.environ.update(AUTH_COOKIE_SECURE='true', AUTH_COOKIE_SAMESITE='invalid')
        get_config.cache_clear()
        with self.assertRaisesRegex(ValueError, 'AUTH_COOKIE_SAMESITE'):
            get_config()

    def test_production_configuration_and_cookie_attributes(self):
        os.environ.update(AUTH_ENV='production', AUTH_COOKIE_SECURE='false')
        get_config.cache_clear()
        with self.assertRaises(ValueError):
            get_config()
        os.environ.update(AUTH_COOKIE_SECURE='true', CORS_ORIGINS='https://libris.example')
        get_config.cache_clear()
        response = Response()
        set_cookie(response, 'random-session-cookie', 300)
        header = response.headers['set-cookie']
        for expected in ('__Host-libris_session=', 'HttpOnly', 'Secure', 'Path=/', 'SameSite=lax'):
            self.assertIn(expected, header)
        self.assertNotIn('Domain=', header)
        os.environ['SUPABASE_PUBLISHABLE_KEY'] = 'sb_secret_not_allowed'
        get_config.cache_clear()
        with self.assertRaises(ValueError):
            get_config()


class GuidanceAuthenticationTests(unittest.TestCase):
    """Real cookie/CSRF/device boundaries; only Supabase HTTP is simulated."""
    post = AuthTests.post
    verify = AuthTests.verify

    def setUp(self):
        AuthTests.setUp(self)
        import guidance
        from guidance_migrate import upgrade_guidance
        from guidance_models import GuidanceTask
        database.Base.metadata.create_all(database.engine)
        upgrade_guidance(database.engine)
        with database.SessionLocal() as db:
            db.query(GuidanceTask).delete()
            for model in (database.BookModel, database.ShelfModel, database.RouteModel, database.RobotModel, database.CheckpointModel):
                db.query(model).delete()
            db.commit()
        test_api.runtime.clear(); guidance.deliveries.clear(); guidance.otp_displays.clear(); guidance.arrivals.clear()
        self.addCleanup(self.cleanup_guidance)
        self.config = patch.dict(os.environ, {'GUIDANCE_ENABLED': 'true', 'AUTH_STAFF_USER_IDS': self.fake.id, 'ATLAS_DEVICE_KEYS': '{"robot1":"' + 'd' * 40 + '"}'})
        self.config.start(); self.addCleanup(self.config.stop)
        self.session = self.verify().json()
        self.client.headers.update({**ORIGIN, 'X-CSRF-Token': self.session['csrf_token']})
        helper = test_api.BackendTests(); helper.client = self.client
        self.robot = helper.robot()['id']
        self.home = helper.cp('Home'); self.shelf_cp = helper.cp('Shelf')
        helper.connect(self.home, [self.shelf_cp]); helper.connect(self.shelf_cp, [self.home])
        for start, end, name in ((self.home, self.shelf_cp, 'Outbound'), (self.shelf_cp, self.home, 'Return')):
            response = self.client.post('/robots/robot1/routes', json=helper.route(start, end, status='active', name=name))
            self.assertEqual(response.status_code, 201, response.text)
        self.shelf = self.client.post('/shelves/', json={'shelf_name': 'Fiction', 'floor': 1}).json()['shelf_id']
        self.book = self.client.post('/book/register', json={'isbn': '9780140328721', 'title': 'Matilda', 'author': 'Roald Dahl', 'shelf_id': self.shelf}).json()['id']
        self.assertEqual(self.client.patch(f'/shelves/{self.shelf}/destination', json={'destination_checkpoint_id': self.shelf_cp}).status_code, 200)
        self.assertEqual(self.client.patch('/robots/robot1/home', json={'home_checkpoint_id': self.home}).status_code, 200)
        self.device_headers = {'X-Atlas-Key': 'd' * 40}
        self.device = TestClient(app); self.addCleanup(self.device.close)
        self.poll()
        self.assertEqual(self.client.post('/robots/robot1/checkpoint-confirmation', json={'checkpoint_id': self.home, 'operator_confirmed': True}).status_code, 200)

    def cleanup_guidance(self):
        import guidance
        from guidance_models import GuidanceTask
        with database.SessionLocal() as db:
            db.query(GuidanceTask).delete(); db.commit()
        test_api.runtime.clear(); guidance.deliveries.clear(); guidance.otp_displays.clear(); guidance.arrivals.clear()

    def poll(self):
        response = self.device.get('/robot/robot1/guidance/command?protocol=2&stopped=true', headers=self.device_headers)
        self.assertEqual(response.status_code, 200, response.text)
        return response.json()

    def test_real_verified_cookie_csrf_and_separate_device_auth(self):
        stranger = TestClient(app); self.addCleanup(stranger.close)
        self.assertEqual(stranger.post('/guidance-requests', json={'book_id': self.book}, headers=ORIGIN).status_code, 401)
        no_csrf = TestClient(app); self.addCleanup(no_csrf.close); no_csrf.cookies.update(self.client.cookies)
        self.assertEqual(no_csrf.post('/guidance-requests', json={'book_id': self.book}, headers=ORIGIN).status_code, 403)
        task = self.client.post('/guidance-requests', json={'book_id': self.book}).json()
        code = self.poll()['atlas_otp']
        self.assertEqual(self.client.get('/robot/robot1/guidance/command?protocol=2&stopped=true').status_code, 401)
        self.assertEqual(self.device.get('/robot/robot2/guidance/command?protocol=2&stopped=true', headers=self.device_headers).status_code, 401)
        self.assertEqual(stranger.post(f"/guidance-requests/{task['id']}/verify", json={'atlas_otp': code}, headers=ORIGIN).status_code, 401)
        verified = self.client.post(f"/guidance-requests/{task['id']}/verify", json={'atlas_otp': code})
        self.assertEqual(verified.status_code, 200, verified.text)
        self.assertEqual(verified.json()['status'], 'queued')
        self.assertNotIn(code, self.client.get('/guidance-requests').text)
        self.assertNotIn(code, self.client.get('/robots/robot1/guidance-requests').text)
        self.assertTrue(self.poll()['session_id'])

    def test_guidance_toggle_never_opens_staff_or_robot_operations(self):
        stranger = TestClient(app); self.addCleanup(stranger.close)
        with patch.dict(os.environ, {'GUIDANCE_ENABLED': 'false', 'AUTH_STAFF_USER_IDS': str(uuid4())}):
            self.assertEqual(stranger.get('/robot/robot1/status').status_code, 401)
            self.assertEqual(stranger.get('/robot/robot1/command').status_code, 401)
            self.assertEqual(self.client.post('/robot/robot1/command', json={'command': 'forward'}).status_code, 403)
            self.assertEqual(self.client.post('/shelves/', json={'shelf_name': 'Unauthorized', 'floor': 1}).status_code, 403)
            self.assertEqual(stranger.get('/book/search/Matilda').status_code, 200)

    def test_otp_rejections_redact_input_and_recovery_cookie_cannot_guide(self):
        task = self.client.post('/guidance-requests', json={'book_id': self.book}).json(); self.poll()
        rejected = self.client.post(f"/guidance-requests/{task['id']}/verify", json={'atlas_otp': 'not-secret-OTP'})
        self.assertEqual(rejected.status_code, 422); self.assertNotIn('not-secret-OTP', rejected.text)
        recovered = self.post('/password/verify', {'email': EMAIL, 'token': '654321'})
        self.assertEqual(recovered.status_code, 200)
        self.client.headers['X-CSRF-Token'] = recovered.json()['csrf_token']
        self.assertEqual(self.client.post('/guidance-requests', json={'book_id': self.book}).status_code, 403)


class ProviderBoundaryTests(unittest.TestCase):
    def test_rest_transport_keeps_tokens_server_side_and_sanitizes_failures(self):
        def handler(request):
            self.assertEqual(request.url, 'https://test-project.supabase.co/auth/v1/user')
            self.assertEqual(request.headers['apikey'], 'sb_publishable_test')
            self.assertEqual(request.headers['authorization'], 'Bearer server-access-token')
            return httpx.Response(401, json={'error_code': 'invalid_token', 'message': 'sensitive-provider-response'})

        client = httpx.Client(transport=httpx.MockTransport(handler))
        config = SimpleNamespace(url='https://test-project.supabase.co', public_key='sb_publishable_test')
        with patch('auth_provider.get_config', return_value=config), patch('auth_provider.httpx.Client', return_value=client):
            with self.assertRaises(ProviderError) as caught:
                SupabaseAuth().user('server-access-token')
        self.assertEqual(caught.exception.status, 401)
        self.assertNotIn('sensitive-provider-response', str(caught.exception))

    def test_confirmation_configuration_must_be_enabled(self):
        provider = SupabaseAuth()
        for settings in ({}, {'mailer_autoconfirm': True, 'external': {'email': True}}, {'mailer_autoconfirm': False, 'external': None}):
            with patch.object(provider, 'request', return_value=settings):
                with self.assertRaises(ProviderError):
                    provider.signup(EMAIL, PASSWORD)

    def test_verify_uses_distinct_numeric_email_and_recovery_types(self):
        provider = SupabaseAuth()
        with patch.object(provider, 'request', return_value={}) as request:
            provider.verify(EMAIL, '123456')
            self.assertEqual(request.call_args.args[2], {'email': EMAIL, 'token': '123456', 'type': 'email'})
            provider.verify(EMAIL, '654321', recovery=True)
            self.assertEqual(request.call_args.args[2]['type'], 'recovery')


if __name__ == '__main__':
    unittest.main()
