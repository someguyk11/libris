import test from 'node:test'
import assert from 'node:assert/strict'
import { createIsbnCamera, cameraError } from '../src/services/isbnCamera.js'

function setup(options = {}) {
  let callback, requested = 0, stopped = 0, decoderStopped = 0, time = 0, constraints
  const stream = { getTracks: () => [{ stop: () => stopped++ }] }
  const mediaDevices = { getUserMedia: async value => { requested++; constraints = value; return stream } }
  const reader = { decodeFromStream: async (_stream, _video, cb) => { callback = cb; return { stop: () => decoderStopped++ } } }
  const camera = createIsbnCamera({ secure: true, mediaDevices, decoder: async () => reader, now: () => time, ...options })
  const found = [], rejected = [], failures = []
  const handlers = { detected: value => found.push(value), rejected: value => rejected.push(value), failed: value => failures.push(value) }
  return { camera, handlers, found, rejected, failures, stream, reader, emit: (value, error) => callback(value ? { getText: () => value } : null, error), advance: () => { time += 3000 }, state: () => ({ requested, stopped, decoderStopped, constraints }) }
}
test('camera is opt-in, prefers rear lens, deduplicates bursts and releases tracks on stop/restart', async () => {
  const f = setup()
  assert.equal(f.state().requested, 0)
  assert.equal(await f.camera.start({}, f.handlers), true)
  assert.equal(f.state().constraints.video.facingMode.ideal, 'environment')
  f.emit('9780306406157'); f.emit('9780306406157')
  assert.deepEqual(f.found, ['9780306406157'])
  f.advance(); f.emit('9780306406157')
  assert.equal(f.found.length, 2)
  await f.camera.start({}, f.handlers)
  assert.equal(f.state().stopped, 1)
  f.camera.stop()
  assert.equal(f.state().stopped, 2)
  assert.equal(f.state().decoderStopped, 2)
  f.emit('9780804429573')
  assert.equal(f.found.length, 2)
})
test('paused scanner ignores detections; non-ISBN barcode explains failure; normal decode misses are harmless', async () => {
  const f = setup(); let paused = true
  await f.camera.start({}, { ...f.handlers, paused: () => paused })
  f.emit('9780306406157'); assert.equal(f.found.length, 0)
  paused = false
  f.emit('4006381333931'); f.emit('4006381333931')
  assert.equal(f.rejected.length, 1)
  for (const name of ['NotFoundException', 'ChecksumException', 'FormatException']) f.emit(null, { name })
  assert.equal(f.failures.length, 0)
  f.emit(null, { name: 'UnexpectedDecodeFailure' })
  assert.equal(f.failures.length, 1)
  assert.equal(f.state().stopped, 1)
})
test('closing during pending permission immediately releases late streams', async () => {
  let release, asked
  const requested = new Promise(resolve => { asked = resolve })
  const permission = new Promise(resolve => { release = resolve })
  const f = setup({ mediaDevices: { getUserMedia: () => { asked(); return permission } } })
  const starting = f.camera.start({}, f.handlers)
  await requested
  f.camera.stop(); release(f.stream)
  assert.equal(await starting, false)
  assert.equal(f.state().stopped, 1)
})
test('closing while decoder starts stops decoder controls and tracks', async () => {
  let finish, ready
  const decoding = new Promise(resolve => { ready = resolve })
  const f = setup({ decoder: async () => ({ decodeFromStream: () => { ready(); return new Promise(resolve => { finish = resolve }) } }) })
  const starting = f.camera.start({}, f.handlers)
  await decoding
  f.camera.stop(); let controlsStopped = false
  finish({ stop: () => { controlsStopped = true } })
  assert.equal(await starting, false)
  assert.equal(controlsStopped, true)
  assert.ok(f.state().stopped >= 1)
})
test('unsupported contexts, permission failures and decoder startup failures preserve manual fallback', async () => {
  const unsupported = setup({ secure: false })
  await assert.rejects(unsupported.camera.start({}, unsupported.handlers), reason => /HTTPS/.test(cameraError(reason)))
  assert.equal(unsupported.state().requested, 0)
  const denied = setup({ mediaDevices: { getUserMedia: async () => { throw { name: 'NotAllowedError' } } } })
  await assert.rejects(denied.camera.start({}, denied.handlers), reason => /permission was denied/.test(cameraError(reason)))
  const broken = setup({ decoder: async () => ({ decodeFromStream: async () => { throw new Error('start failed') } }) })
  await assert.rejects(broken.camera.start({}, broken.handlers))
  assert.equal(broken.state().stopped, 1)
  assert.match(cameraError({ name: 'NotFoundError' }), /No suitable camera/)
  assert.match(cameraError({ name: 'NotReadableError' }), /busy/)
})
test('a canceled decoder startup failure cannot stop the restarted camera', async () => {
  let rejectOld, ready, calls = 0, stopped = 0
  const decoding = new Promise(resolve => { ready = resolve })
  const f = setup({
    mediaDevices: { getUserMedia: async () => ({ getTracks: () => [{ stop: () => stopped++ }] }) },
    decoder: async () => ({ decodeFromStream: () => {
      if (++calls === 1) { ready(); return new Promise((_resolve, reject) => { rejectOld = reject }) }
      return Promise.resolve({ stop() {} })
    } }),
  })
  const oldStart = f.camera.start({}, f.handlers)
  await decoding
  assert.equal(await f.camera.start({}, f.handlers), true)
  assert.equal(stopped, 1)
  rejectOld(new Error('canceled startup failed'))
  assert.equal(await oldStart, false)
  assert.equal(stopped, 1)
  f.camera.stop()
  assert.equal(stopped, 2)
})
