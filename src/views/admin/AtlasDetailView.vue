<template>
  <section class="operations-page atlas-detail admin-page-enter">
    <a class="operations-back" href="#/admin/atlas"><span class="material-symbols-rounded">arrow_back</span>Back to Atlas</a>
    <p v-if="apiError" class="admin-notice admin-notice--error" role="alert">{{ apiError }} <button class="admin-text-button" :disabled="loading" @click="refreshAtlas">Retry</button></p>
    <template v-if="unit">
      <header class="operations-heading atlas-detail-heading">
        <div><div class="atlas-title-line"><h1>{{ unit.name }}</h1><span class="atlas-status"><i></i>{{ apiMode ? (unit.connectionError ? 'Backend unavailable' : unit.telemetry?.status || 'Checking…') : 'Local preview' }}</span></div><p>{{ unit.identifier }}<span class="atlas-header-separator">/</span>Robot workspace</p></div>
        <button class="admin-icon-button atlas-settings" aria-label="Atlas settings" @click="openSettings"><span class="material-symbols-rounded">settings</span></button>
      </header>
      <div class="atlas-tabs" role="tablist" aria-label="Atlas workspace">
        <button v-for="(tab, index) in tabs" :id="'atlas-tab-' + tab.id" :key="tab.id" role="tab" :aria-controls="'atlas-panel-' + tab.id" :aria-selected="activeTab === tab.id" :tabindex="activeTab === tab.id ? 0 : -1" @click="activeTab = tab.id" @keydown="tabKeydown($event, index)"><span class="material-symbols-rounded" aria-hidden="true">{{ tab.icon }}</span>{{ tab.label }}<small v-if="tab.id === 'routes' && routes.length">{{ routes.length }}</small></button>
      </div>
      <p class="operations-demo-note"><span class="material-symbols-rounded">info</span>{{ apiMode ? 'Atlas V1 · Measured encoder routes and acknowledged shelf-guidance tasks require compatible, physically tested ESP32 firmware. The scene is illustrative; battery telemetry is unavailable.' : activeTab === 'drive' ? 'Local simulation · Controls move the illustrative scene only. No hardware commands or sensor data.' : 'Frontend preview · Sample snapshots and browser-local records, not live robot data.' }}</p>
      <p v-if="apiMode && unit.connectionError" class="admin-notice admin-notice--error" role="alert">{{ unit.connectionError }}</p>
      <p v-if="storageError" class="admin-notice admin-notice--error" role="alert">Browser storage is unavailable. Atlas changes will last for this visit only.</p>
      <div id="atlas-panel-drive" v-show="activeTab === 'drive'" role="tabpanel" aria-labelledby="atlas-tab-drive" tabindex="0"><AtlasDriveWorkspace :unit="unit" :active="activeTab === 'drive' && !settingsOpen" @saved="$emit('notice', $event)" /></div>
      <div id="atlas-panel-monitor" v-show="activeTab === 'monitor'" class="atlas-monitor" role="tabpanel" aria-labelledby="atlas-tab-monitor" tabindex="0">
        <section v-if="apiMode"><div class="monitor-section-heading"><h2>Operational state</h2><span>BACKEND STATUS · 2s POLLING</span></div><dl class="monitor-state">
          <div><dt>Connection</dt><dd>{{ unit.connectionError ? 'Backend unavailable' : unit.telemetry?.status || 'Checking…' }}</dd></div>
          <div><dt>Network (last reported)</dt><dd>{{ unit.telemetry?.telemetry_at ? unit.telemetry.wifi_ssid : 'Not yet reported' }}</dd></div>
          <div><dt>Signal strength (reported)</dt><dd>{{ unit.telemetry?.telemetry_at ? unit.telemetry.signal_strength : 'Not yet reported' }}</dd></div>
          <div><dt>Uptime (last reported)</dt><dd>{{ unit.telemetry?.telemetry_at ? unit.telemetry.uptime_seconds + ' seconds' : 'Not yet reported' }}</dd></div>
          <div><dt>Last seen</dt><dd>{{ atlasLastSeen(unit.telemetry) }}</dd></div>
          <div><dt>Last command / sequence</dt><dd>{{ unit.telemetry ? unit.telemetry.command + ' / ' + unit.telemetry.sequence : 'Unavailable' }}</dd></div>
          <div><dt>Drive readiness</dt><dd>{{ unit.telemetry?.drive_ready ? 'Firmware polling commands' : 'No recent command poll' }}</dd></div>
          <div><dt>Network change</dt><dd>{{ unit.telemetry?.network_change_pending ? 'Queued for pickup' : 'No staged change reported' }}</dd></div>
          <div><dt>Last confirmed checkpoint</dt><dd>{{ checkpointName(unit.telemetry?.residing_checkpoint_id) }} · Historical, not live position</dd></div>
          <div><dt>Recording</dt><dd>{{ unit.telemetry?.recording_id || 'No temporary recording' }}</dd></div>
          <div><dt>Guidance task</dt><dd>{{ unit.telemetry?.guidance_task_id || 'No active guidance task reported' }}</dd></div>
          <div><dt>Battery</dt><dd>Not exposed by the backend</dd></div>
        </dl><p class="admin-muted">Command acceptance is not proof of physical movement. Telemetry may be stale when offline. Queue flags clearing indicate firmware pickup, not successful application.</p></section>
        <section v-else><div class="monitor-section-heading"><h2>Operational state</h2><span>{{ unit.snapshot ? 'SAMPLE SNAPSHOT' : 'AWAITING INTEGRATION' }}</span></div><dl class="monitor-state">
          <div><dt>Connection</dt><dd>{{ unit.snapshot ? (unit.snapshot.online ? 'Online · sample' : 'Offline · sample') : 'Not connected' }}</dd></div>
          <div><dt>Network</dt><dd>{{ unit.snapshot ? (unit.snapshot.online ? 'Connected · sample' : 'Disconnected · sample') : 'Unavailable' }}</dd></div>
          <div><dt>Battery</dt><dd>{{ unit.snapshot ? unit.snapshot.battery + '% · sample' : 'Unavailable' }}</dd></div>
          <div><dt>Last seen</dt><dd>{{ unit.snapshot ? unit.snapshot.lastSeen + ' · sample' : 'Never connected' }}</dd></div>
          <div><dt>Operating mode</dt><dd>{{ unit.snapshot?.mode || 'Unavailable' }}</dd></div>
          <div><dt>Current task</dt><dd>{{ unit.snapshot?.task || 'Unavailable' }}</dd></div>
          <div><dt>Active hardware route</dt><dd>Unavailable</dd></div>
          <div><dt>Sensor readings</dt><dd>Not connected</dd></div>
        </dl></section>
        <GuidanceMonitor v-if="apiMode" :key="unit.identifier" :robot-id="unit.identifier" />
        <section v-else><div class="monitor-section-heading"><h2>Task queue</h2></div><p class="admin-muted">Demo only · No live task queue is connected.</p></section>
        <section><div class="monitor-section-heading"><h2>Connection & errors</h2></div><p class="admin-muted">{{ apiMode ? unit.connectionError || 'No backend request error. Hardware diagnostics are not exposed.' : 'Hardware diagnostics are unavailable in this frontend preview.' }}</p></section>
        <section v-if="!apiMode"><div class="monitor-section-heading"><h2>Local activity</h2></div><ul v-if="routes.length" class="atlas-events"><li v-for="route in routes.slice(0, 5)" :key="route.id"><span class="material-symbols-rounded">route</span><div><strong>Simulation saved: {{ route.name }}</strong><small>{{ formatDate(route.createdAt) }}</small></div></li></ul><p v-else class="admin-muted">Saved simulations will appear here.</p></section>
      </div>
      <div id="atlas-panel-routes" v-show="activeTab === 'routes'" class="atlas-routes" role="tabpanel" aria-labelledby="atlas-tab-routes" tabindex="0">
        <div class="monitor-section-heading"><div><h2>Saved Routes</h2><p class="admin-muted">{{ apiMode ? 'Encoder routes with ordered checkpoint visits. Active means data validation passed—not a verified physical trajectory or running replay.' : 'Local simulations for ' + unit.name + '. Hardware replay is not connected.' }}</p></div><button v-if="apiMode" class="admin-text-button" :disabled="pending" @click="openNewRoute">+ Import encoder draft</button><span>{{ routes.length }} {{ routes.length === 1 ? 'ROUTE' : 'ROUTES' }}</span></div>
        <EmptyState v-if="!routes.length" title="No saved routes yet" :description="apiMode ? 'Record measured encoder results through the V1 recording API, or import existing measured segments as a draft. Never substitute durations for encoder values.' : 'Record movements in the local simulation to create your first route draft.'"><template #illustration><span class="material-symbols-rounded atlas-empty-icon">route</span></template><template v-if="!apiMode" #action><AppButton variant="secondary" @click="activeTab = 'drive'">Open Drive & Record</AppButton></template></EmptyState>
        <ul v-else class="atlas-route-list"><li v-for="route in routes" :key="route.id"><span class="material-symbols-rounded route-list-icon">route</span><div class="atlas-route-info"><button @click="viewRoute = route">{{ route.name }}</button><p>{{ route.simulation ? adminState.shelfDetails[route.destination]?.name || route.destination || 'No destination' : checkpointName(route.destination) }} · {{ route.steps.length }} segments · {{ route.simulation ? 'Simulation' : route.status }}</p><small>{{ formatDate(route.updatedAt || route.createdAt) }}</small></div><div class="route-actions"><button class="admin-icon-button" :aria-label="'View ' + route.name" @click="viewRoute = route"><span class="material-symbols-rounded">visibility</span></button><button class="admin-icon-button" :aria-label="'Rename ' + route.name" :disabled="pending" @click="openRename(route)"><span class="material-symbols-rounded">edit</span></button><button class="admin-icon-button" :aria-label="'Delete ' + route.name" :disabled="pending" @click="deleteError = ''; deleteTarget = route"><span class="material-symbols-rounded">delete</span></button></div></li></ul>
      </div>
    </template>
    <EmptyState v-else :title="loading ? 'Loading Atlas…' : 'Atlas not found'" :description="apiMode ? 'Registrations are loaded from the database. Previous browser-only records can be imported from the Atlas overview.' : 'This robot is not registered in this browser.'"><template #action><a class="admin-button secondary" href="#/admin/atlas">Back to Atlas</a></template></EmptyState>

    <AdminDialog :open="settingsOpen" title="Atlas settings" @close="closeSettings">
      <p class="admin-muted">{{ apiMode ? 'The name is saved to the database. The firmware identifier is stable and cannot be renamed.' : 'These details update the local registration only.' }}</p>
      <form class="admin-form" @submit.prevent="saveSettings"><label>Atlas name<input v-model="settings.name" required maxlength="80" /></label><label>Robot identifier<input v-model="settings.identifier" :readonly="apiMode" required pattern="[A-Za-z0-9\-]{2,40}" maxlength="40" /></label><p v-if="settingsError" class="admin-form-error" role="alert">{{ settingsError }}</p><div class="admin-form-actions"><AppButton variant="secondary" type="button" @click="closeSettings">Close</AppButton><AppButton type="submit" :disabled="!settings.name.trim() || !settings.identifier.trim()">Save details</AppButton></div></form>
      <template v-if="apiMode">
        <hr class="atlas-settings-divider" />
        <h2>Network provisioning</h2><p class="admin-muted">Stages credentials for firmware pickup. This does not confirm a connection. Use only on a trusted network, with HTTPS outside localhost.</p>
        <form class="admin-form" @submit.prevent="provision"><label>Wi-Fi SSID<input v-model="network.ssid" required maxlength="32" autocomplete="off" /></label><label>Wi-Fi password<input v-model="network.password" type="password" maxlength="63" autocomplete="new-password" /><small>Leave blank only for an open network.</small></label><p v-if="networkError" class="admin-form-error" role="alert">{{ networkError }}</p><p v-if="networkNotice" role="status" class="admin-muted">{{ networkNotice }}</p><AppButton type="submit" :disabled="networkPending || !network.ssid.trim()">{{ networkPending ? 'Queuing…' : 'Queue network change' }}</AppButton></form>
        <hr class="atlas-settings-divider" /><h2>Factory reset</h2><p class="admin-muted">Queues a firmware preferences wipe. Atlas may lose its Wi-Fi connection. Completion is not acknowledged by this API.</p><button class="admin-button secondary" :disabled="networkPending" @click="resetOpen = true">Queue factory reset…</button>
      </template>
    </AdminDialog>
    <AdminDialog :open="resetOpen" title="Queue factory reset?" @close="resetOpen = false"><p class="admin-muted">This instructs {{ unit?.name }} to clear saved firmware preferences on its next command poll. You may need to provision Wi-Fi again.</p><p v-if="resetError" class="admin-form-error" role="alert">{{ resetError }}</p><div class="admin-form-actions"><AppButton variant="secondary" :disabled="networkPending" @click="resetOpen = false">Keep settings</AppButton><AppButton :disabled="networkPending" @click="factoryReset">Queue reset</AppButton></div></AdminDialog>
    <AdminDialog :open="Boolean(renameTarget)" title="Rename route" @close="renameTarget = null"><form class="admin-form" @submit.prevent="submitRename"><label>Route name<input v-model="renamedName" required maxlength="100" /></label><p v-if="renameError" class="admin-form-error" role="alert">{{ renameError }}</p><div class="admin-form-actions"><AppButton variant="secondary" type="button" @click="renameTarget = null">Cancel</AppButton><AppButton type="submit" :disabled="!renamedName.trim() || pending">{{ pending ? 'Saving…' : 'Save name' }}</AppButton></div></form></AdminDialog>
    <AdminDialog :open="Boolean(deleteTarget)" title="Delete this route?" @close="deleteTarget = null"><p class="admin-muted">“{{ deleteTarget?.name }}” will be removed {{ apiMode ? 'from the database' : 'from this local workspace' }} with all its steps. This cannot be undone.</p><p v-if="deleteError" class="admin-form-error" role="alert">{{ deleteError }}</p><div class="admin-form-actions"><AppButton variant="secondary" :disabled="pending" @click="deleteTarget = null">Keep route</AppButton><AppButton :disabled="pending" @click="confirmDelete">{{ pending ? 'Deleting…' : 'Delete route' }}</AppButton></div></AdminDialog>
    <AdminDialog :open="Boolean(viewRoute)" :title="viewRoute?.name || 'Route details'" @close="viewRoute = null"><template v-if="viewRoute"><p class="admin-muted">{{ viewRoute.simulation ? 'Simulation only' : 'Encoder recording · ' + viewRoute.status }} · {{ viewRoute.steps.length }} segments<br>{{ formatDate(viewRoute.createdAt) }}</p><div class="route-steps"><table><caption>{{ viewRoute.simulation ? 'Recorded simulation movements' : 'Measured left/right wheel differences, not absolute positions' }}</caption><thead><tr><th>{{ viewRoute.simulation ? 'Time' : 'Sequence' }}</th><th>{{ viewRoute.simulation ? 'Movement' : 'Left / Right encoders' }}</th><th>{{ viewRoute.simulation ? 'Position (x, z)' : 'Confirmed checkpoint' }}</th></tr></thead><tbody><tr v-for="(step, index) in viewRoute.steps" :key="index"><td>{{ viewRoute.simulation ? (step.at / 1000).toFixed(1) + 's' : step.sequence }}</td><td>{{ viewRoute.simulation ? step.command : step.movement.left_encoder_ticks + ' ' + step.movement.left_direction + ' / ' + step.movement.right_encoder_ticks + ' ' + step.movement.right_direction }}</td><td>{{ viewRoute.simulation ? step.pose.x.toFixed(2) + ', ' + step.pose.z.toFixed(2) : checkpointName(step.checkpoint_id) }}</td></tr></tbody></table></div><p class="admin-muted">Replay requires encoder-feedback firmware and task execution; no replay command is sent.</p></template></AdminDialog>
    <AdminDialog :open="newRouteOpen" title="Import encoder draft" @close="newRouteOpen = false">
      <p class="admin-muted">Import existing measured wheel differences for {{ unit?.name }}. Do not invent encoder ticks or convert timed drafts. Checkpoints are managed through the checkpoint API; this import never moves hardware.</p>
      <form class="admin-form" @submit.prevent="createRoute">
        <label>Route name<input v-model="routeDraft.name" required maxlength="100" /></label>
        <label>Start checkpoint<select v-model="routeDraft.startCheckpointId" required><option value="" disabled>Choose checkpoint</option><option v-for="point in state.checkpoints.filter(point => point.is_active)" :key="point.id" :value="point.id">{{ point.name }}</option></select></label>
        <label>Destination checkpoint<select v-model="routeDraft.destination" required><option value="" disabled>Choose checkpoint</option><option v-for="point in state.checkpoints.filter(point => point.is_active)" :key="point.id" :value="point.id">{{ point.name }}</option></select></label>
        <label>Measured route segments (JSON array)<textarea v-model="routeDraft.segmentsJson" rows="8" required spellcheck="false"></textarea></label>
        <p v-if="routeError" class="admin-form-error" role="alert">{{ routeError }}</p>
        <div class="admin-form-actions"><AppButton variant="secondary" type="button" :disabled="pending" @click="newRouteOpen = false">Cancel</AppButton><AppButton type="submit" :disabled="pending || !routeDraft.name.trim()">{{ pending ? 'Saving…' : 'Save route draft' }}</AppButton></div>
      </form>
    </AdminDialog>
  </section>
</template>
<script setup>
import GuidanceMonitor from './components/GuidanceMonitor.vue'
import { computed, reactive, ref } from 'vue'
import { useNav } from '@/composables/useNav.js'
import AppButton from '@/components/common/AppButton.vue'
import EmptyState from '@/components/common/EmptyState.vue'
import AdminDialog from './components/AdminDialog.vue'
import AtlasDriveWorkspace from './components/AtlasDriveWorkspace.vue'
import { useAtlasStore } from './useAtlasStore.js'
import { useAdminStore } from './useAdminStore.js'
import { apiMode, backend } from '@/services/api.js'
import { useAtlasTelemetry, atlasLastSeen } from '@/composables/useAtlasTelemetry.js'
const emit = defineEmits(['notice'])
const { selectedAtlasId } = useNav()
const { state, storageError, saveUnit, saveRoute, renameRoute, deleteRoute, loading, apiError, refreshAtlas, findUnit } = useAtlasStore()
const { state: adminState } = useAdminStore()
const unit = computed(() => findUnit(selectedAtlasId.value))
useAtlasTelemetry(() => unit.value ? [unit.value] : [])
const routes = computed(() => state.routes.filter(route => route.atlasId === unit.value?.id))
const activeTab = ref('drive')
const tabs = [{ id: 'drive', label: 'Drive & Record', icon: 'sports_esports' }, { id: 'monitor', label: 'Monitor', icon: 'monitoring' }, { id: 'routes', label: 'Saved Routes', icon: 'route' }]
const settingsOpen = ref(false), settingsError = ref(''), settings = reactive({ name: '', identifier: '' })
const network = reactive({ ssid: '', password: '' }), networkError = ref(''), networkNotice = ref(''), networkPending = ref(false), resetOpen = ref(false), resetError = ref('')
function closeSettings() { settingsOpen.value = false; network.password = '' }
async function provision() {
  if (networkPending.value) return
  networkPending.value = true; networkError.value = ''; networkNotice.value = ''
  try { const result = await backend.reconfigure(unit.value.identifier, { ssid: network.ssid.trim(), password: network.password }); network.password = ''; networkNotice.value = result.message }
  catch (error) { networkError.value = error.message }
  finally { networkPending.value = false }
}
async function factoryReset() {
  if (networkPending.value) return
  networkPending.value = true; resetError.value = ''
  try { const result = await backend.reset(unit.value.identifier); resetOpen.value = false; closeSettings(); emit('notice', result.message) }
  catch (error) { resetError.value = error.message }
  finally { networkPending.value = false }
}
const renameTarget = ref(null), renamedName = ref(''), renameError = ref(''), deleteTarget = ref(null), viewRoute = ref(null)
const pending = ref(false), deleteError = ref(''), newRouteOpen = ref(false), routeError = ref('')
const routeDraft = reactive({ name: '', startCheckpointId: '', destination: '', segmentsJson: '[]' })
function checkpointName(id) { return state.checkpoints.find(point => point.id === id)?.name || id || 'Not associated' }
function openNewRoute() { Object.assign(routeDraft, { name: '', startCheckpointId: '', destination: '', segmentsJson: '[]' }); routeError.value = ''; newRouteOpen.value = true }
async function createRoute() {
  if (pending.value) return
  pending.value = true; routeError.value = ''
  try { const segments = JSON.parse(routeDraft.segmentsJson); if (!Array.isArray(segments)) throw new Error('Segments must be an ordered JSON array.'); await saveRoute({ ...routeDraft, route_segments: segments, atlasId: unit.value.id }); newRouteOpen.value = false; emit('notice', 'Encoder draft saved. No robot commands were sent.') }
  catch (cause) { routeError.value = cause.message }
  finally { pending.value = false }
}
const formatDate = value => new Date(value).toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' })
function tabKeydown(event, index) {
  const keys = { ArrowRight: (index + 1) % tabs.length, ArrowLeft: (index + tabs.length - 1) % tabs.length, Home: 0, End: tabs.length - 1 }
  if (!(event.key in keys)) return
  event.preventDefault(); activeTab.value = tabs[keys[event.key]].id
  document.getElementById(`atlas-tab-${activeTab.value}`)?.focus()
}
function openSettings() { if (!unit.value) return; Object.assign(settings, { name: unit.value.name, identifier: unit.value.identifier }); settingsError.value = ''; settingsOpen.value = true }
async function saveSettings() {
  if (pending.value) return
  pending.value = true
  try { await saveUnit(settings, unit.value.id); settingsOpen.value = false; emit('notice', apiMode ? 'Atlas name saved to the database.' : storageError.value ? 'Details updated for this visit only.' : 'Atlas details updated in this browser.') }
  catch (cause) { settingsError.value = cause.message }
  finally { pending.value = false }
}
function openRename(route) { renameTarget.value = route; renamedName.value = route.name; renameError.value = '' }
async function submitRename() {
  if (pending.value) return
  pending.value = true
  try { await renameRoute(renameTarget.value.id, renamedName.value); renameTarget.value = null; emit('notice', apiMode ? 'Route renamed in the database.' : storageError.value ? 'Route renamed for this visit only.' : 'Route renamed locally.') }
  catch (cause) { renameError.value = cause.message }
  finally { pending.value = false }
}
async function confirmDelete() {
  if (pending.value) return
  pending.value = true; deleteError.value = ''
  try { await deleteRoute(deleteTarget.value.id); deleteTarget.value = null; emit('notice', apiMode ? 'Route draft deleted from the database.' : storageError.value ? 'Route removed for this visit only.' : 'Local route deleted.') }
  catch (cause) { deleteError.value = cause.message }
  finally { pending.value = false }
}
</script>
