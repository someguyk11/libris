// Capture before routing or async views run. Tokens stay in memory only and are
// removed from the URL immediately, including expired-link error descriptions.
export function captureAuthCallback(location, history) {
  const url = new URL(location.href)
  const hash = new URLSearchParams(url.hash.replace(/^#(?:\/auth\??)?/, ''))
  const query = url.searchParams
  const get = key => hash.get(key) || query.get(key)
  if (!['access_token', 'refresh_token', 'token_hash', 'error', 'error_code'].some(key => get(key))) return null
  const type = get('type') || 'email'
  let result
  if (get('error') || get('error_code')) result = { error: 'This email link is invalid or expired. Request a new one.' }
  else if (!['signup', 'email', 'recovery'].includes(type)) result = { error: 'Unsupported email link. Request a new verification code.' }
  else if (get('token_hash')) result = { type, token_hash: get('token_hash') }
  else if (get('access_token') && get('refresh_token')) result = { type, access_token: get('access_token'), refresh_token: get('refresh_token') }
  else result = { error: 'Incomplete email link. Request a new one.' }
  for (const key of ['access_token', 'refresh_token', 'token_hash', 'token_type', 'expires_in', 'expires_at', 'type', 'error', 'error_code', 'error_description']) query.delete(key)
  url.hash = '/auth'
  history.replaceState(null, '', url.pathname + url.search + url.hash)
  return result
}

let callback = typeof window === 'undefined' ? null : captureAuthCallback(window.location, window.history)
export const hasAuthCallback = () => callback !== null
export function takeAuthCallback() { const value = callback; callback = null; return value }
