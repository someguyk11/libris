import test from 'node:test'
import assert from 'node:assert/strict'
import { request, authApi, backend } from '../src/services/api.js'
import { useSession } from '../src/composables/useSession.js'

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
