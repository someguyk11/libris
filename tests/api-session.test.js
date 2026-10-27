import test from 'node:test'
import assert from 'node:assert/strict'
import { request, authApi, backend } from '../src/services/api.js'
import { useSession } from '../src/composables/useSession.js'
import { captureAuthCallback } from '../src/services/authCallback.js'

test('API rejects malformed, empty, null and scalar successes, but accepts 204', async () => {
  const original = globalThis.fetch
  try {
    for (const text of ['{broken', '', 'null', '42']) {
      globalThis.fetch = async () => new Response(text)
      await assert.rejects(request('/ping'), /invalid|empty/)
    }
    globalThis.fetch = async () => new Response(null, { status: 204 })
    assert.equal(await request('/ping'), null)
  } finally { globalThis.fetch = original }
})

test('API preserves server errors and rejects malformed session envelopes', async () => {
  const original = globalThis.fetch
  try {
    globalThis.fetch = async () => Response.json({ detail: 'Authentication is not configured.' }, { status: 503 })
    await assert.rejects(authApi.login('fixture@example.com', 'not-a-real-password'), error => error.status === 503 && /not configured/.test(error.message))
    globalThis.fetch = async () => Response.json({ user: {} })
    await assert.rejects(authApi.session(), /invalid session/)
  } finally { globalThis.fetch = original }
})

test('API distinguishes cancellation from timeout and never retries a mutation', async () => {
  const original = globalThis.fetch
  let mutations = 0
  try {
    globalThis.fetch = async () => Response.json({ authenticated: true, purpose: 'user', csrf_token: 'test-csrf' })
    await authApi.session()
    globalThis.fetch = async (url, options) => {
      mutations++
      assert.equal(options.credentials, 'include')
      assert.equal(options.headers['X-CSRF-Token'], 'test-csrf')
      throw new TypeError('simulated connection failure')
    }
    await assert.rejects(backend.command('fixture', 'stop'), /Backend unavailable/)
    assert.equal(mutations, 1)
    globalThis.fetch = async (url, { signal }) => {
      if (signal.aborted) throw new DOMException('cancelled', 'AbortError')
      return new Promise((resolve, reject) => signal.addEventListener('abort', () => reject(new DOMException('cancelled', 'AbortError')), { once: true }))
    }
    const controller = new AbortController(); controller.abort()
    await assert.rejects(request('/ping', { signal: controller.signal }), { name: 'AbortError' })
    await assert.rejects(request('/ping', { timeout: 5 }), /timed out/)
  } finally { globalThis.fetch = original }
})

test('authoritative session identity excludes recovery sessions and clears after expired logout', async () => {
  const original = globalThis.fetch
  const account = useSession()
  try {
    account.accept({ purpose: 'user', user: { email: 'fixture@example.com', is_staff: true } })
    assert.equal(account.user.value.email, 'fixture@example.com')
    assert.equal(account.isStaff.value, true)
    account.accept({ purpose: 'recovery', user: { is_staff: true } })
    assert.equal(account.user.value, null)
    assert.equal(account.isStaff.value, false)
    globalThis.fetch = async () => Response.json({ detail: 'Session expired.' }, { status: 401 })
    assert.equal((await account.signOut()).signed_out, true)
    assert.equal(account.session.value, null)
  } finally { account.accept(null); globalThis.fetch = original }
})

test('auth requests preserve cooldowns and persist profile updates into session identity', async () => {
  const original = globalThis.fetch
  const account = useSession()
  try {
    globalThis.fetch = async () => Response.json({ detail: 'Wait briefly.' }, { status: 429, headers: { 'Retry-After': '17' } })
    await assert.rejects(authApi.signup('fixture@example.com', 'strong-password-42'), error => error.status === 429 && error.retryAfter === 17)
    globalThis.fetch = async (url, options) => {
      assert.ok(url.endsWith('/auth/signup'))
      assert.deepEqual(JSON.parse(options.body), { email: 'fixture@example.com', password: 'strong-password-42', display_name: 'Reader', username: 'reader42' })
      return Response.json({ message: 'Check email.' }, { status: 202 })
    }
    await authApi.signup('fixture@example.com', 'strong-password-42', { display_name: 'Reader', username: 'reader42' })
    globalThis.fetch = async () => Response.json({ authenticated: true, purpose: 'user', csrf_token: 'profile-csrf', user: { id: 'fixture' } })
    await authApi.session()
    globalThis.fetch = async (url, options) => {
      assert.ok(url.endsWith('/auth/profile'))
      assert.equal(options.method, 'PATCH')
      assert.equal(options.headers['X-CSRF-Token'], 'profile-csrf')
      return Response.json({ authenticated: true, purpose: 'user', csrf_token: 'profile-csrf', user: { id: 'fixture', username: 'reader42', display_name: 'Reader' } })
    }
    await account.updateProfile({ username: 'reader42', display_name: 'Reader' })
    assert.equal(account.user.value.username, 'reader42')
    assert.equal(account.user.value.display_name, 'Reader')
  } finally { account.accept(null); globalThis.fetch = original }
})

test('email callbacks capture recovery tokens and immediately scrub all secrets from URLs', () => {
  for (const href of [
    'https://libris.example/#access_token=access-secret&refresh_token=refresh-secret&type=recovery&expires_in=3600',
    'https://libris.example/?access_token=access-secret&refresh_token=refresh-secret&type=recovery',
  ]) {
    let cleaned
    const callback = captureAuthCallback({ href }, { replaceState: (state, title, url) => { cleaned = url } })
    assert.deepEqual(callback, { type: 'recovery', access_token: 'access-secret', refresh_token: 'refresh-secret' })
    assert.equal(cleaned, '/#/auth')
  }
  let cleaned
  const history = { replaceState: (state, title, url) => { cleaned = url } }
  assert.deepEqual(captureAuthCallback({ href: 'https://libris.example/?token_hash=hash-secret&type=signup&keep=yes' }, history), { type: 'signup', token_hash: 'hash-secret' })
  assert.equal(cleaned, '/?keep=yes#/auth')
  assert.match(captureAuthCallback({ href: 'https://libris.example/#error=access_denied&error_description=sensitive' }, history).error, /expired/)
  assert.equal(cleaned, '/#/auth')
  assert.match(captureAuthCallback({ href: 'https://libris.example/#access_token=incomplete' }, history).error, /Incomplete/)
  assert.equal(captureAuthCallback({ href: 'https://libris.example/#/discover' }, history), null)
})
