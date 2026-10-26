"""Production-origin regression tests; in-memory database, no lifespan."""
import os
import unittest
from unittest.mock import patch

from fastapi.testclient import TestClient

# Reuse the suite's in-memory engine before importing the application.
import test_api
from main import app, LibrisAPI
from auth_config import allowed_origins, get_config
from auth_security import AuthSafetyMiddleware


ORIGIN = 'https://libotsystems.netlify.app'
PATHS = ('/checkpoints/', '/book/search/%25', '/robots/', '/shelves/')


class CorsTests(unittest.TestCase):
    def setUp(self):
        env = patch.dict(os.environ, {
            'CORS_ORIGINS': ORIGIN, 'AUTH_ENABLED': 'false',
            'AUTH_COOKIE_SECURE': 'true', 'AUTH_COOKIE_SAMESITE': 'lax',
        })
        env.start()
        self.addCleanup(env.stop)
        get_config.cache_clear()
        self.addCleanup(get_config.cache_clear)
        test_api.database.Base.metadata.create_all(test_api.database.engine)
        # Fresh middleware stack using the real routes and production CORS class.
        api = LibrisAPI()
        api.add_middleware(AuthSafetyMiddleware, origins=allowed_origins())
        api.include_router(app.router)

        @api.get('/test-server-error')
        def server_error():
            raise RuntimeError('simulated unhandled error')

        self.client = TestClient(api, raise_server_exceptions=False)
        self.addCleanup(self.client.close)

    def assert_cors(self, response):
        self.assertEqual(response.headers.get('access-control-allow-origin'), ORIGIN)
        self.assertEqual(response.headers.get('access-control-allow-credentials'), 'true')
        self.assertIn('Origin', response.headers.get('vary', ''))

    def preflight(self, path, origin=ORIGIN, method='GET', headers='content-type,x-csrf-token'):
        return self.client.options(path, headers={
            'Origin': origin, 'Access-Control-Request-Method': method,
            'Access-Control-Request-Headers': headers,
        })

    def test_allowed_origin_on_success_and_auth_errors(self):
        response = self.client.get('/ping', headers={'Origin': ORIGIN})
        self.assertEqual(response.status_code, 200)
        self.assert_cors(response)
        for path in PATHS:
            with self.subTest(path=path):
                response = self.client.get(path, headers={'Origin': ORIGIN})
                expected = 200 if path in ('/book/search/%25', '/shelves/') else 503
                self.assertEqual(response.status_code, expected)
                self.assert_cors(response)

    def test_preflight_bypasses_auth_for_all_failing_paths(self):
        for path in (*PATHS, '/auth/login'):
            for method in ('GET', 'POST', 'PUT', 'PATCH', 'DELETE'):
                with self.subTest(path=path, method=method):
                    response = self.preflight(path, method=method)
                    self.assertEqual(response.status_code, 200)
                    self.assert_cors(response)
                    self.assertIn('X-CSRF-Token', response.headers['access-control-allow-headers'])

    def test_disallowed_origins(self):
        for origin in ('https://evil.example', ORIGIN + '.evil.example', 'null'):
            with self.subTest(origin=origin):
                response = self.preflight('/checkpoints/', origin=origin)
                self.assertEqual(response.status_code, 400)
                self.assertNotIn('access-control-allow-origin', response.headers)
                response = self.client.get('/ping', headers={'Origin': origin})
                self.assertNotIn('access-control-allow-origin', response.headers)
                response = self.client.post('/auth/logout', headers={'Origin': origin}, json={})
                self.assertEqual(response.status_code, 403)
                self.assertNotIn('access-control-allow-origin', response.headers)

    def test_disallowed_preflight_method_and_header(self):
        self.assertEqual(self.preflight('/ping', method='TRACE').status_code, 400)
        self.assertEqual(self.preflight('/ping', headers='X-Not-Allowed').status_code, 400)

    def test_server_errors_and_not_found_retain_cors(self):
        for path, status in (('/test-server-error', 500), ('/not-a-route', 404)):
            response = self.client.get(path, headers={'Origin': ORIGIN})
            self.assertEqual(response.status_code, status)
            self.assert_cors(response)

    def test_request_without_origin_has_no_cors_header(self):
        self.assertNotIn('access-control-allow-origin', self.client.get('/ping').headers)

    def test_exact_comma_separated_configuration(self):
        with patch.dict(os.environ, {'CORS_ORIGINS': f' {ORIGIN}, https://other.example '}):
            self.assertEqual(allowed_origins(), (ORIGIN, 'https://other.example'))
        for value in ('*', '', ORIGIN + '/', ORIGIN + '/path', 'https://*.example'):
            with self.subTest(value=value), patch.dict(os.environ, {'CORS_ORIGINS': value}):
                with self.assertRaises(ValueError):
                    allowed_origins()


if __name__ == '__main__':
    unittest.main()
