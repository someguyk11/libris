import test from 'node:test'
import assert from 'node:assert/strict'
import { validateAtlas, validateSimulationRoute } from '../src/views/admin/atlasData.js'
import { cameraPoint, clipFace, moveSimulation, projectPoint, sceneFaces, sceneGrid } from '../src/views/admin/atlasScene.js'
import { atlasSceneDefaults, createAtlasSeed } from '../src/mock/data/atlas.js'

test('Atlas identifiers are unique while editing preserves the existing unit', () => {
  const { units } = createAtlasSeed()
  assert.throws(() => validateAtlas({ name: 'New unit', identifier: ' at-001 ' }, units), /already registered/)
  assert.deepEqual(validateAtlas({ name: ' Renamed ', identifier: 'at-001' }, units, 'atlas-01'), { name: 'Renamed', identifier: 'AT-001' })
  assert.throws(() => validateAtlas({ name: 'Unit', identifier: 'http://robot' }, units), /identifier/)
})

test('recordings require a known Atlas and ordered, finite simulation steps', () => {
  const { units } = createAtlasSeed()
  const input = { name: ' Test route ', atlasId: units[0].id, steps: [{ command: 'forward', at: 0, pose: { x: 0, z: .22, yaw: 0 } }, { command: 'left', at: 120, pose: { x: 0, z: .22, yaw: -.07 } }] }
  const route = validateSimulationRoute(input, units)
  assert.equal(route.simulation, true)
  assert.equal(route.name, 'Test route')
  assert.throws(() => validateSimulationRoute({ ...input, atlasId: 'missing' }, units), /registered Atlas/)
  assert.throws(() => validateSimulationRoute({ ...input, steps: [] }, units), /steps/)
  assert.throws(() => validateSimulationRoute({ ...input, steps: [input.steps[1], input.steps[0]] }, units), /invalid movement/)
  assert.throws(() => validateSimulationRoute({ ...input, steps: [{ command: 'forward', at: 0, pose: { x: NaN, z: 0, yaw: 0 } }] }, units), /invalid movement/)
  input.steps[0].pose.x = 99
  assert.equal(route.steps[0].pose.x, 0, 'saved route must not share mutable pose references')
})

test('perspective model responds to camera and geometry parameters', () => {
  const { camera, obstacles } = atlasSceneDefaults
  const origin = projectPoint(cameraPoint({ x: 0, y: camera.y, z: 5 }, camera), camera)
  assert.deepEqual(origin, [500, 650 * .47])
  const narrow = projectPoint({ x: 1, y: 0, z: 5 }, { ...camera, fov: 55 })
  const wide = projectPoint({ x: 1, y: 0, z: 5 }, { ...camera, fov: 110 })
  assert.ok(narrow[0] > wide[0])
  assert.notDeepEqual(sceneFaces(obstacles, camera, 1), sceneFaces(obstacles, camera, 1.5))
  assert.notDeepEqual(sceneFaces(obstacles, camera), sceneFaces(obstacles, { ...camera, yaw: .5 }))
})

test('near-plane clipping keeps cuboids and floor finite when moving through the scene', () => {
  const clipped = clipFace([{ x: -1, y: 0, z: -1 }, { x: 1, y: 0, z: 1 }, { x: 1, y: 2, z: 1 }])
  assert.ok(clipped.length >= 3)
  assert.ok(clipped.every(point => point.z >= .15))
  for (const yaw of [0, Math.PI / 2, Math.PI, -Math.PI / 2]) {
    const camera = { ...atlasSceneDefaults.camera, x: 4.7, z: 5.7, yaw }
    const faces = sceneFaces(atlasSceneDefaults.obstacles, camera)
    assert.ok(faces.every(face => !/NaN|Infinity/.test(face.points)))
    assert.ok(sceneGrid(camera).flat(2).every(Number.isFinite))
  }
})

test('simulated movements follow orientation and remain within the illustrative bounds', () => {
  const pose = { x: 0, z: 0, yaw: Math.PI / 2 }
  const forward = moveSimulation(pose, 'forward')
  assert.ok(Math.abs(forward.x - .22) < .0001)
  assert.ok(Math.abs(forward.z) < .0001)
  assert.equal(pose.x, 0)
  assert.equal(moveSimulation({ x: 20, z: 0, yaw: Math.PI / 2 }, 'forward').x, 20)
  assert.ok(moveSimulation(pose, 'left').yaw < pose.yaw)
})
