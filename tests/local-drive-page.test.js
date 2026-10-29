import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import vm from 'node:vm'

const page = readFileSync(new URL('../hardware/Atlas/src/LocalDrivePage.h', import.meta.url), 'utf8')
const script = page.match(/<script>([\s\S]*?)<\/script>/)[1]
const flush = async () => { for (let i = 0; i < 20; i++) await Promise.resolve() }
async function browser(overrides = {}) {
  const elements = new Map(), events = {}, intervals = [], timeouts = [], requests = [], modeRequests = []
  const location = { href: '', reload() {} }
  let now = 100, failCommands = false, delayMode = false, allowConfirm = true, pendingMode = ''
  const state = { token: 'boot-token', sequence: 10, fresh: true, ready: true, latched: false,
    turnsCalibrated: true, distanceCalibrated: true, stepCm: 20, fault: 'OK', navigation: 'Idle',
    message: '', leftEdges: 0, rightEdges: 0, storageReady: true, recording: false, replaying: false,
    storageBusy: false, routeCount: 0, routeIndex: 0, stepPending: false, appliedSequence: 10,
    localMode: 'drive', modeChanging: false, operation: 1, operationStatus: 'complete',
    routes: [{ slot: 0, valid: true, name: 'First path', segments: 2 }, { slot: 1, valid: false, name: '', segments: 0 }],
    sensors: Array.from({ length: 4 }, () => ({ valid: true, cm: 100 })), ...overrides }
  function element(id) {
    if (!elements.has(id)) elements.set(id, { dataset: { command: id }, disabled: true, handlers: {}, value: '', checked: false, options: [],
      addEventListener(name, fn) { this.handlers[name] = fn }, setPointerCapture() {}, setAttribute() {},
      replaceChildren() { this.options = []; this.value = '' },
      append(option) { this.options.push(option); if (this.options.length === 1) this.value = option.value } })
    return elements.get(id)
  }
  const document = { hidden: false, querySelector: s => element(s.slice(1)),
    querySelectorAll: () => ['front', 'back', 'left', 'right'].map(element),
    createElement: () => ({ value: '', textContent: '' }),
    addEventListener: (name, fn) => { events[name] = fn } }
  vm.runInNewContext(script, { document, AbortController, confirm: () => allowConfirm, performance: { now: () => now },
    location, addEventListener: (name, fn) => { events[name] = fn },
    setInterval: fn => intervals.push(fn), setTimeout: (fn, ms) => { timeouts.push({ fn, ms }); return timeouts.length }, clearTimeout() {},
    fetch: async (path, options) => {
      if (path === '/drive/status') return { ok: true, json: async () => state }
      if (path === '/status') return { ok: true, json: async () => state }
      if (path === '/local/mode') {
        const body = JSON.parse(options.body); modeRequests.push(body); pendingMode = body.mode
        state.operation++
        state.operationStatus = delayMode ? 'pending' : 'complete'
        if (!delayMode) state.localMode = pendingMode
        return { ok: true, json: async () => ({ operation: state.operation }) }
      }
      requests.push(JSON.parse(options.body))
      if (failCommands) throw Error('Offline')
      return { ok: true, status: 202 }
    } })
  await flush()
  return { state, document, elements, events, intervals, timeouts, requests, modeRequests, location,
    advance(ms) { now += ms }, offline() { failCommands = true },
    cancelConfirm() { allowConfirm = false },
    delayMode() { delayMode = true },
    async confirmMode() { state.operationStatus = 'complete'; state.localMode = pendingMode; timeouts.findLast(t => t.ms === 150).fn(); await flush() },
    async refresh() { await timeouts.findLast(t => t.ms === 300).fn(); await flush() },
    async press(id = 'front') { element(id).handlers.pointerdown({ button: 0, pointerId: 1, preventDefault() {} }); await flush() } }
}

test('local page polls status and submits authenticated, sequenced directional commands', async () => {
  const b = await browser()
  assert.equal(b.elements.get('front').disabled, false)
  await b.press()
  assert.deepEqual(b.requests[0], { token: 'boot-token', sequence: 11, command: 'front' })
  b.intervals[0](); await flush()
  assert.equal(b.requests.at(-1).command, 'front')
  assert.equal(b.requests.at(-1).sequence, 12)
})
test('release sends stop and ends heartbeat commands', async () => {
  const b = await browser(); await b.press()
  b.elements.get('front').handlers.pointerup(); await flush()
  assert.equal(b.requests.at(-1).command, 'stop')
  const count = b.requests.length
  b.intervals[0](); await flush()
  assert.equal(b.requests.length, count)
})
test('focus loss, pointer cancellation and hidden page send stop', async () => {
  for (const event of ['blur', 'pointercancel', 'visibilitychange', 'pagehide']) {
    const b = await browser(); await b.press()
    if (event === 'pointercancel') b.elements.get('front').handlers[event]()
    else { if (event === 'visibilitychange') b.document.hidden = true; b.events[event]() }
    await flush(); assert.equal(b.requests.at(-1).command, 'stop')
  }
})
test('stale status stops a held direction and disables movement', async () => {
  const b = await browser(); await b.press(); b.advance(701)
  b.intervals[0](); await flush()
  assert.equal(b.requests.at(-1).command, 'stop')
  assert.equal(b.elements.get('front').disabled, true)
})
test('uncalibrated turns and unsafe state disable controls', async () => {
  const b = await browser({ turnsCalibrated: false })
  assert.equal(b.elements.get('left').disabled, true)
  assert.equal(b.elements.get('front').disabled, false)
  const unsafe = await browser({ ready: false })
  await unsafe.press(); assert.equal(unsafe.requests.length, 0)
})
test('command transport failure cancels heartbeat, without automatic retry', async () => {
  const b = await browser(); b.offline(); await b.press()
  b.intervals[0](); await flush()
  assert.equal(b.requests.length, 1)
  assert.equal(b.elements.get('front').disabled, true)
})
test('emergency stop and reset use separate explicit commands', async () => {
  const b = await browser()
  b.elements.get('emergency').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'emergency')
  b.elements.get('reset').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'reset')
})

test('movement and replay tabs do not start movement when selecting a route', async () => {
  const b = await browser()
  b.elements.get('replay-tab').onclick()
  assert.equal(b.elements.get('movement-panel').hidden, true)
  assert.equal(b.elements.get('replay-panel').hidden, false)
  assert.equal(b.elements.get('replay-slot').options.length, 1)
  b.elements.get('replay-slot').handlers.input()
  assert.equal(b.requests.length, 0)
  assert.equal(b.elements.get('route-json').href, '/drive/route/0')
})
test('save sends a name and selected ESP32 slot, not route timing or browser poses', async () => {
  const b = await browser({ recording: true, routeCount: 2 })
  b.elements.get('route-name').value = 'Shelf route'
  b.elements.get('save-slot').value = '1'
  b.elements.get('route-name').handlers.input()
  assert.equal(b.elements.get('save-route').disabled, false)
  b.elements.get('save-route').onclick(); await flush()
  assert.deepEqual(b.requests.at(-1), { token: 'boot-token', sequence: 11, command: 'save', slot: 1, name: 'Shelf route' })
})
test('automatic saving is the default even when every slot is full', async () => {
  const b = await browser({ recording: true, routeCount: 2, routes: [
    { slot: 0, valid: true, name: 'A', segments: 2 },
    { slot: 1, valid: true, name: 'B', segments: 2 },
    { slot: 2, valid: true, name: 'C', segments: 2 },
  ] })
  assert.equal(b.elements.get('save-slot').value, '-1')
  b.elements.get('route-name').value = 'D'
  b.elements.get('route-name').handlers.input()
  b.elements.get('save-route').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'save')
  assert.equal(b.requests.at(-1).slot, -1) // actual target is chosen by the ESP32, not the browser
})
test('saving is inhibited while an encoder step is still pending', async () => {
  const b = await browser({ recording: true, routeCount: 1, stepPending: true })
  b.elements.get('route-name').value = 'Incomplete'
  b.elements.get('route-name').handlers.input()
  assert.equal(b.elements.get('save-route').disabled, true)
})
test('recording UI reports the expanded 32-segment cap and allows saving a full route', async () => {
  const b = await browser({ recording: true, routeCount: 32, maxRouteSegments: 32 })
  b.elements.get('route-name').value = 'Full route'
  b.elements.get('route-name').handlers.input()
  assert.match(b.elements.get('record-status').textContent, /32 \/ 32 validated steps/)
  assert.equal(b.elements.get('save-route').disabled, false)
})
test('replay requires explicit start confirmation and then sends heartbeat, never repeats replay', async () => {
  const b = await browser()
  b.elements.get('replay-route').onclick(); await flush()
  assert.equal(b.requests.length, 0)
  b.elements.get('start-confirmed').checked = true
  b.elements.get('start-confirmed').handlers.input()
  assert.equal(b.elements.get('replay-route').disabled, false)
  b.elements.get('replay-route').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'replay')
  assert.equal(b.requests.at(-1).slot, 0)
  assert.equal(b.requests.at(-1).startConfirmed, true)
  b.intervals[0](); await flush()
  assert.equal(b.requests.at(-1).command, 'heartbeat')
  b.elements.get('replay-stop').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'stop')
})
test('storage busy and recording inhibit replay controls', async () => {
  for (const state of [{ storageBusy: true }, { recording: true }]) {
    const b = await browser(state)
    b.elements.get('start-confirmed').checked = true
    b.elements.get('start-confirmed').handlers.input()
    assert.equal(b.elements.get('replay-route').disabled, true)
    b.elements.get('replay-route').onclick(); await flush()
    assert.equal(b.requests.length, 0)
  }
})

test('replay completion stops heartbeat without restarting the route', async () => {
  const b = await browser()
  b.elements.get('start-confirmed').checked = true
  b.elements.get('replay-route').onclick(); await flush()
  b.state.appliedSequence = 11; b.state.replaying = false; b.state.storageBusy = false
  await b.refresh()
  const count = b.requests.length
  b.intervals[0](); await flush()
  assert.equal(b.requests.length, count)
  assert.equal(b.elements.get('start-confirmed').checked, false)
})
test('route changes require a new start-position confirmation', async () => {
  const b = await browser()
  b.elements.get('start-confirmed').checked = true
  b.elements.get('replay-slot').handlers.change()
  assert.equal(b.elements.get('start-confirmed').checked, false)
  assert.equal(b.elements.get('replay-route').disabled, true)
})
test('switching away from replay sends stop', async () => {
  const b = await browser()
  b.elements.get('start-confirmed').checked = true
  b.elements.get('replay-route').onclick(); await flush()
  b.elements.get('drive-tab').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'stop')
})
test('Wi-Fi option stops motion, requests setup mode and waits for confirmation before navigating', async () => {
  const b = await browser(); await b.press(); b.delayMode()
  b.elements.get('wifi-tab').onclick(); await flush()
  assert.equal(b.requests.at(-1).command, 'stop')
  assert.equal(b.modeRequests[0].mode, 'wifi')
  assert.equal(b.location.href, '')
  assert.equal(b.elements.get('front').disabled, true)
  await b.confirmMode()
  assert.equal(b.location.href, '/')
})
test('cancelled Wi-Fi switch preserves the current recording and sends no mode change', async () => {
  const b = await browser({ recording: true }); b.cancelConfirm()
  b.elements.get('wifi-tab').onclick(); await flush()
  assert.equal(b.modeRequests.length, 0)
  assert.equal(b.requests.length, 0)
})
test('Wi-Fi setup inhibits drive and replay until explicitly switching back', async () => {
  const b = await browser({ localMode: 'wifi' })
  assert.equal(b.elements.get('front').disabled, true)
  assert.equal(b.elements.get('mode-notice').hidden, false)
  await b.press(); assert.equal(b.requests.length, 0)
  b.elements.get('drive-enable').onclick(); await flush()
  assert.equal(b.modeRequests.at(-1).mode, 'drive')
  assert.equal(b.requests.at(-1).command, 'stop')
  assert.equal(b.requests.some(r => ['front', 'back', 'left', 'right', 'replay'].includes(r.command)), false)
})
