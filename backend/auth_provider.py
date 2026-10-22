"""Supabase Auth REST boundary. Never return provider bodies to the browser."""
import httpx
from auth_config import get_config


class ProviderError(Exception):
    def __init__(self, status, code=''):
        self.status = status
        self.code = code
        super().__init__('Authentication provider request failed')


class SupabaseAuth:
    def request(self, method, path, body=None, access=None):
        config = get_config()
        headers = {'apikey': config.public_key, 'Content-Type': 'application/json'}
        if access:
            headers['Authorization'] = 'Bearer ' + access
        try:
            with httpx.Client(timeout=10, follow_redirects=False) as client:
                response = client.request(method, config.url + '/auth/v1' + path, json=body, headers=headers)
            if response.status_code == 204:
                return {}
            try:
                result = response.json()
            except ValueError:
                raise ProviderError(503) from None
            if not response.is_success:
                raise ProviderError(response.status_code, result.get('error_code', result.get('code', '')) if isinstance(result, dict) else '')
            if not isinstance(result, dict):
                raise ProviderError(503)
            return result
        except httpx.HTTPError:
            raise ProviderError(503) from None

    def require_confirmations(self):
        settings = self.request('GET', '/settings')
        external = settings.get('external')
        if settings.get('mailer_autoconfirm') is not False or not isinstance(external, dict) or external.get('email') is not True:
            raise ProviderError(503, 'confirmation_configuration')

    def signup(self, email, password):
        self.require_confirmations()
        result = self.request('POST', '/signup', {'email': email, 'password': password})
        if result.get('access_token'):
            raise ProviderError(503, 'confirmation_configuration')

    def login(self, email, password):
        return self.request('POST', '/token?grant_type=password', {'email': email, 'password': password})

    def verify(self, email, token, recovery=False):
        return self.request('POST', '/verify', {'email': email, 'token': token, 'type': 'recovery' if recovery else 'email'})

    def resend(self, email):
        self.require_confirmations()
        self.request('POST', '/resend', {'email': email, 'type': 'signup'})

    def recover(self, email):
        self.request('POST', '/recover', {'email': email})

    def user(self, access):
        return self.request('GET', '/user', access=access)

    def refresh(self, refresh):
        return self.request('POST', '/token?grant_type=refresh_token', {'refresh_token': refresh})

    def logout(self, access, all_sessions=False):
        self.request('POST', '/logout?scope=' + ('global' if all_sessions else 'local'), access=access)

    def password(self, access, password):
        return self.request('PUT', '/user', {'password': password}, access=access)


provider = SupabaseAuth()
