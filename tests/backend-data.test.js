import test from 'node:test'
import assert from 'node:assert/strict'
import { bookFromApi, shelfFromApi, shelfPayload, robotFromApi, routeFromApi } from '../src/services/backendData.js'
import { backend } from '../src/services/api.js'
import { validateAtlas } from '../src/views/admin/atlasData.js'

test('backend robot identifiers preserve firmware case without changing demo normalization', () => {
  assert.equal(validateAtlas({ name: 'Atlas', identifier: 'atlas-esp32' }, [], null, true).identifier, 'atlas-esp32')
  assert.equal(validateAtlas({ name: 'Atlas', identifier: 'atlas-esp32' }, []).identifier, 'ATLAS-ESP32')
  assert.throws(() => validateAtlas({ name: 'Atlas', identifier: 'atlas-esp32' }, [{ id: '1', identifier: 'atlas-esp32' }], null, true), /already/)
})

test('API adapters preserve real IDs and never invent loans or sensor metadata', () => {
  const shelf = shelfFromApi({ shelf_id: 17, shelf_name: 'Fiction', section: null, floor: 2, path_id: 0, description: null })
  assert.equal(shelf.code, '17')
  assert.equal(shelf.name, 'Fiction')
  const book = bookFromApi({ id: 2, isbn: '9780593135204', title: 'Project Hail Mary', author: 'Andy Weir', shelf_id: 17, available: false })
  assert.equal(book.shelf, shelf.code)
  assert.equal(book.status, 'Unavailable')
  assert.equal(book.due, '')
  assert.equal(book.borrowerId, '')
  assert.equal(book.genre, '')
})
test('Atlas V1 text identities and measured encoder routes remain distinct from simulations', () => {
  const unit = robotFromApi({ id: 'robot1', name: 'Atlas' })
  assert.equal(unit.id, 'robot1')
  assert.equal(unit.identifier, 'robot1')
  assert.equal(unit.telemetry, null)
  const movement = { left_encoder_ticks: 20, right_encoder_ticks: 18, left_direction: 'forward', right_direction: 'forward' }
  const route = routeFromApi({ id: 'route-1', robot_id: unit.id, name: 'Entrance', start_checkpoint_id: 'checkpoint-a', destination_checkpoint_id: 'checkpoint-b', recording_version: 1, status: 'draft', created_at: '2026-10-18T10:00:00', updated_at: '2026-10-18T11:00:00Z', route_segments: [{ sequence: 1, checkpoint_id: null, movement }] })
  assert.equal(route.atlasId, unit.id)
  assert.equal(route.destination, 'checkpoint-b')
  assert.equal(route.startCheckpointId, 'checkpoint-a')
  assert.equal(route.simulation, false)
  assert.equal(route.createdAt, '2026-10-18T10:00:00Z')
  assert.equal(route.updatedAt, '2026-10-18T11:00:00Z')
  assert.equal(route.steps[0].pose, undefined)
  assert.equal(route.steps[0].duration_ms, undefined)
  assert.deepEqual(route.steps[0].movement, movement)
})
test('shelf payload uses supported fields only and preserves path placeholder on editing', () => {
  const payload = shelfPayload({ name: ' Fiction ', section: ' North ', floor: 2, path_id: 7, category: 'Books', aisle: '3' })
  assert.deepEqual(payload, { shelf_name: 'Fiction', section: 'North', floor: 2, path_id: 7, description: null })
  assert.throws(() => shelfPayload({ name: ' ', floor: 1 }), /name/)
  assert.throws(() => shelfPayload({ name: 'Fiction', floor: 1.5 }), /whole/)
})
test('API client encodes searches/identifiers and reports server validation and connection failures', async () => {
  const original = globalThis.fetch
  try {
    let captured
    globalThis.fetch = async (url, options) => { captured = { url, options }; return new Response(JSON.stringify({ status: 200, data: {} })) }
    await backend.books('%')
    assert.equal(captured.url, '/api/book/search/%25?limit=50&offset=0')
    await backend.command('AT/001', 'backward')
    assert.equal(captured.url, '/api/robot/AT%2F001/command')
    assert.deepEqual(JSON.parse(captured.options.body), { command: 'backward' })
    await backend.registerRobot({ name: 'Atlas', robot_id: 'atlas-esp32' })
    assert.equal(captured.url, '/api/robots/')
    await backend.createRoute('robot1', { name: 'Entrance', start_checkpoint_id: 'checkpoint-a', destination_checkpoint_id: 'checkpoint-b', route_segments: [] })
    assert.equal(captured.url, '/api/robots/robot1/routes')
    assert.equal(captured.options.method, 'POST')
    await backend.renameRoute('route-1', 'Renamed')
    assert.equal(captured.options.method, 'PATCH')
    assert.equal(captured.url, '/api/routes/route-1')
    await backend.deleteRoute('route-1')
    assert.equal(captured.options.method, 'DELETE')
    await backend.startRecording('robot1', 'checkpoint-a')
    assert.equal(captured.url, '/api/robot/robot1/recordings')
    assert.deepEqual(JSON.parse(captured.options.body), { start_checkpoint_id: 'checkpoint-a' })
    await backend.saveRecording('recording-1', { name: 'Recorded', destination_checkpoint_id: 'checkpoint-b' })
    assert.equal(captured.url, '/api/recordings/recording-1/save')
    globalThis.fetch = async () => new Response(JSON.stringify({ detail: [{ loc: ['body', 'shelf_name'], msg: 'Required' }] }), { status: 422 })
    await assert.rejects(backend.shelves(), /shelf_name: Required/)
    globalThis.fetch = async () => { throw new TypeError('fetch failed') }
    await assert.rejects(backend.shelves(), /Backend unavailable/)
  } finally { globalThis.fetch = original }
})
