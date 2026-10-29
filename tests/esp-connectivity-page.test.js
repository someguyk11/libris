import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import vm from 'node:vm'

const source = readFileSync(new URL('../hardware/Atlas/portal/index.html', import.meta.url), 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1]
const flush = async () => { for (let i = 0; i < 80; i++) await Promise.resolve() }
async function page() {
  const elements = new Map(), requests = [], timers = [], location = { href: '' }
  const state = { localControlEnabled: true, localMode: 'drive', operation: 1, operationStatus: 'complete', connected: false }
  function element(id) {
    if (!elements.has(id)) elements.set(id, { value: '', dataset: {}, disabled: false, textContent: '', type: '',
      setAttribute() {}, replaceChildren() {}, append() {}, reportValidity: () => true })
    return elements.get(id)
  }
  vm.runInNewContext(source, { document: { getElementById: element, querySelectorAll: () => [], createElement: () => ({ setAttribute() {}, append() {} }) },
    TextEncoder, AbortController, location, Date, setTimeout: (fn, ms) => { timers.push({ fn, ms }); return timers.length }, clearTimeout() {},
    fetch: async (path, options) => {
      if (path === '/history') return { ok: true, json: async () => [] }
      if (path === '/status') return { ok: true, json: async () => state }
      const body = JSON.parse(options.body); requests.push({ path, body })
      if (path === '/local/mode') {
        state.localMode = body.mode; state.operation++
        return { ok: true, json: async () => ({ operation: state.operation }) }
      }
      return { ok: false, json: async () => ({ error: 'Unexpected mutation' }) }
    } })
  await flush()
  return { elements, requests, state, location }
}
test('connectivity page inhibits credentials/scanning while drive mode is active', async () => {
  const b = await page()
  b.elements.get('ssid').value = 'Router'
  b.elements.get('ssid').oninput()
  assert.equal(b.elements.get('save').disabled, true)
  assert.equal(b.elements.get('connect').disabled, true)
  assert.equal(b.elements.get('scan').disabled, true)
})
test('Wi-Fi connection option enables setup only after mode confirmation', async () => {
  const b = await page()
  b.elements.get('ssid').value = 'Router'
  b.elements.get('wifi-mode').onclick(); await flush()
  assert.deepEqual(b.requests[0], { path: '/local/mode', body: { mode: 'wifi' } })
  assert.equal(b.elements.get('connect').disabled, false)
  assert.equal(b.elements.get('scan').disabled, false)
})
test('Drive & replay option selects drive mode without issuing any movement', async () => {
  const b = await page()
  b.elements.get('drive-mode').onclick({ preventDefault() {} }); await flush()
  assert.deepEqual(b.requests[0], { path: '/local/mode', body: { mode: 'drive' } })
  assert.equal(b.location.href, '/drive')
  assert.equal(b.requests.some(r => r.path === '/drive/command'), false)
})
