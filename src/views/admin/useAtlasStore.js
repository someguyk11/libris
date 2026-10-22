import { computed, reactive, ref, watch } from 'vue'
import { createAtlasSeed } from '@/mock/data/atlas.js'
import { validateAtlas, validateSimulationRoute } from './atlasData.js'
import { apiMode, backend } from '@/services/api.js'
import { robotFromApi, routeFromApi } from '@/services/backendData.js'

const storageKey = apiMode ? 'libris-atlas-api-registrations-v1' : 'libris-atlas-demo-v1'
let saved
try { saved = JSON.parse(localStorage.getItem(storageKey)) } catch { /* Use isolated demo fixtures. */ }
function validSaved(value) {
  if (value?.version !== 1 || !Array.isArray(value.units) || !Array.isArray(value.routes)) return false
  try {
    const ids = new Set()
    value.units.forEach(unit => {
      if (typeof unit.id !== 'string' || !unit.id || ids.has(unit.id)) throw new Error('Invalid unit')
      ids.add(unit.id)
      validateAtlas(unit, value.units, unit.id, apiMode)
    })
    value.routes.forEach(route => {
      if (typeof route.id !== 'string' || !route.simulation || !Number.isFinite(Date.parse(route.createdAt))) throw new Error('Invalid route')
      validateSimulationRoute(route, value.units)
    })
    return true
  } catch { return false }
}
const state = reactive(apiMode ? { version: 1, units: [], routes: [] } : validSaved(saved) ? saved : createAtlasSeed())
if (!state.checkpoints) state.checkpoints = []
const legacyRecords = apiMode && validSaved(saved) ? saved.units : []
const legacyUnits = computed(() => legacyRecords.filter(unit => !state.units.some(item => item.identifier === unit.identifier)))
const loading = ref(false), apiError = ref(''), loaded = ref(false)
let refreshPromise
const storageError = ref(false)
watch(state, () => {
  if (apiMode) return
  try { localStorage.setItem(storageKey, JSON.stringify(state)); storageError.value = false }
  catch { storageError.value = true }
}, { deep: true, flush: 'sync' })

function saveUnit(input, id = null) {
  const fields = validateAtlas(input, state.units, id, apiMode)
  if (apiMode) return saveBackendUnit(fields, id)
  if (id) {
    const unit = state.units.find(item => item.id === id)
    if (!unit) throw new Error('Atlas not found.')
    Object.assign(unit, fields)
    return unit
  }
  const unit = { id: crypto.randomUUID(), ...fields, snapshot: null }
  state.units.push(unit)
  return unit
}
function saveRoute(input) {
  if (apiMode) return saveBackendRoute(input)
  const route = { ...validateSimulationRoute(input, state.units), id: crypto.randomUUID(), createdAt: new Date().toISOString() }
  state.routes.unshift(route)
  return route
}
function renameRoute(id, name) {
  if (apiMode) return renameBackendRoute(id, name)
  const route = state.routes.find(item => item.id === id)
  if (!route) throw new Error('Route not found.')
  const checked = validateSimulationRoute({ ...route, name }, state.units)
  route.name = checked.name
}
function deleteRoute(id) {
  if (apiMode) return deleteBackendRoute(id)
  const index = state.routes.findIndex(route => route.id === id)
  if (index !== -1) state.routes.splice(index, 1)
}
async function refreshAtlas() {
  if (!apiMode) return
  if (refreshPromise) return refreshPromise
  loading.value = true; apiError.value = ''
  refreshPromise = (async () => {
    try {
      const [robots, checkpoints] = await Promise.all([backend.robots(), backend.checkpoints()])
      const routes = await Promise.all(robots.map(robot => backend.routes(robot.id)))
      state.units = robots.map(robot => {
        const previous = state.units.find(unit => unit.id === robot.id)
        return Object.assign(previous || {}, robotFromApi(robot), previous ? { telemetry: previous.telemetry, connectionError: previous.connectionError } : {})
      })
      state.routes = routes.flat().map(routeFromApi)
      state.checkpoints = checkpoints
      loaded.value = true
    } catch (error) { apiError.value = error.message }
    finally { loading.value = false; refreshPromise = null }
  })()
  return refreshPromise
}
async function saveBackendUnit(fields, id) {
  if (id && state.units.find(unit => unit.id === id)?.identifier !== fields.identifier) throw new Error('Firmware identifiers cannot be changed after registration.')
  const result = robotFromApi(await (id ? backend.updateRobot(id, fields.name) : backend.registerRobot({ id: fields.identifier, name: fields.name })))
  const existing = state.units.find(unit => unit.id === result.id)
  if (existing) Object.assign(existing, { name: result.name })
  else state.units.push(result)
  return existing || result
}
async function importLegacyUnits() {
  let count = 0
  for (const unit of [...legacyUnits.value]) {
    await saveBackendUnit(validateAtlas(unit, state.units, null, true), null)
    count++
  }
  return count
}
async function saveBackendRoute(input) {
  const payload = { name: input.name.trim(), start_checkpoint_id: input.startCheckpointId, destination_checkpoint_id: input.destination, status: 'draft', recording_version: 1, route_segments: input.route_segments }
  const route = routeFromApi(await backend.createRoute(input.atlasId, payload))
  state.routes.unshift(route)
  return route
}
async function renameBackendRoute(id, name) {
  const route = routeFromApi(await backend.renameRoute(id, name.trim()))
  const index = state.routes.findIndex(item => item.id === id)
  if (index >= 0) state.routes[index] = route
}
async function deleteBackendRoute(id) {
  await backend.deleteRoute(id)
  const index = state.routes.findIndex(route => route.id === id)
  if (index >= 0) state.routes.splice(index, 1)
}
function findUnit(id) {
  return state.units.find(unit => unit.id === id) || (apiMode ? state.units.find(unit => unit.identifier === legacyRecords.find(old => old.id === id)?.identifier) : null)
}
export function useAtlasStore() { return { state, storageError, saveUnit, saveRoute, renameRoute, deleteRoute, loading, loaded, apiError, legacyUnits, refreshAtlas, importLegacyUnits, findUnit } }
