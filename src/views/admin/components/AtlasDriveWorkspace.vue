<template>
  <div ref="workspace" class="atlas-drive-workspace">
    <div class="drive-layout">
      <div class="drive-viewport">
        <AtlasScene :camera="camera" :obstacle-scale="parameters.obstacleScale" :grid="{ extent: 24, spacing: parameters.gridSpacing }" />
        <p v-if="apiMode" class="live-scene-caption">Illustrative view · No obstacle or pose telemetry</p>
        <button class="scene-fullscreen admin-icon-button" :aria-label="fullscreen ? 'Exit fullscreen' : 'Enter fullscreen'" @click="toggleFullscreen"><span class="material-symbols-rounded">{{ fullscreen ? 'fullscreen_exit' : 'fullscreen' }}</span></button>
        <div v-if="recording" class="scene-recording" role="status"><i></i>Recording simulation · {{ steps.length }} steps</div>
      </div>
      <section class="drive-controls" :aria-label="apiMode ? 'Hardware driving controls' : 'Simulation driving controls'" @keydown="keyDown" @keyup="keyUp" @focusout="onFocusOut">
        <div class="drive-controls-heading"><h2>Drive controls</h2><span>{{ apiMode ? 'HARDWARE COMMANDS' : 'LOCAL SIMULATION' }}</span></div>
        <p v-if="apiMode" class="admin-muted">{{ driveReady ? 'Firmware polling detected. Keep Atlas in sight and the area clear.' : 'Movement disabled: waiting for recent firmware command polling.' }}</p>
        <div class="direction-pad">
          <div class="direction-center"><span class="material-symbols-rounded">navigation</span></div>
          <button v-for="direction in directions" :key="direction.command" :class="['direction-button', direction.command, { 'is-held': held === direction.command }]" :disabled="apiMode && !driveReady" :aria-label="direction.label + (apiMode ? ' hardware command' : ' in simulation')" :title="direction.label" @pointerdown="pointerDown($event, direction.command)" @pointerup="stop" @pointercancel="stop" @lostpointercapture="stop" @click="keyboardClick($event, direction.command)"><span class="material-symbols-rounded">{{ direction.icon }}</span></button>
        </div>
        <div v-if="apiMode" class="live-turn-controls"><button v-for="turn in [{ command: 'left_turn', label: 'Left turn' }, { command: 'right_turn', label: 'Right turn' }]" :key="turn.command" class="admin-button secondary" :disabled="!driveReady" @pointerdown="pointerDown($event, turn.command)" @pointerup="stop" @pointercancel="stop" @lostpointercapture="stop" @click="keyboardClick($event, turn.command)">{{ turn.label }}</button></div>
        <p class="drive-motion" aria-live="polite">{{ motionLabel }}</p>
        <button class="drive-stop" @click="stop"><span class="material-symbols-rounded" aria-hidden="true">stop_circle</span>STOP</button>
        <div v-if="!apiMode" class="drive-recording">
          <div><h3>Route recording</h3><span :class="{ 'is-recording': recording }">{{ recording ? 'Recording' : steps.length ? 'Unsaved draft' : 'Ready' }}</span></div>
          <p>Capture simulated movements as a local route draft.</p>
          <AppButton v-if="recording" variant="secondary" full-width @click="finishRecording"><span class="material-symbols-rounded" aria-hidden="true">stop</span>Finish recording</AppButton>
          <AppButton v-else-if="steps.length" full-width @click="saveOpen = true"><span class="material-symbols-rounded" aria-hidden="true">save</span>Save recording</AppButton>
          <AppButton v-else full-width @click="startRecording"><span class="material-symbols-rounded" aria-hidden="true">radio_button_checked</span>Record simulation</AppButton>
          <button v-if="steps.length && !recording" class="admin-text-button" @click="discardOpen = true">Discard draft</button>
        </div>
        <p v-else class="admin-muted">Encoder recording is available through the V1 recording API and requires ESP32 command-result acknowledgements. This workspace sends manual commands; it does not infer encoder measurements or checkpoints. Replay is unavailable. Firmware must stop motors on polling failure.</p>
        <p class="drive-controls-caption">Hold a direction, or use arrow keys / WASD while controls are focused. Release to stop.</p>
      </section>
    </div>
    <p v-if="notice" class="drive-feedback" role="status">{{ notice }}</p>
    <details class="scene-parameters">
      <summary><span class="material-symbols-rounded">tune</span>Scene parameters<span>Illustrative 3D model</span></summary>
      <div class="scene-parameter-fields">
        <label :for="`${id}-fov`">Field of view <output :for="`${id}-fov`">{{ parameters.fov }}°</output><input :id="`${id}-fov`" v-model.number="parameters.fov" :disabled="recording" type="range" min="55" max="110" step="1" /></label>
        <label :for="`${id}-height`">Camera height <output :for="`${id}-height`">{{ parameters.height.toFixed(2) }}</output><input :id="`${id}-height`" v-model.number="parameters.height" :disabled="recording" type="range" min="0.5" max="2.5" step="0.05" /></label>
        <label :for="`${id}-scale`">Obstacle scale <output :for="`${id}-scale`">{{ parameters.obstacleScale.toFixed(1) }}×</output><input :id="`${id}-scale`" v-model.number="parameters.obstacleScale" :disabled="recording" type="range" min="0.5" max="1.5" step="0.1" /></label>
        <label :for="`${id}-grid`">Grid spacing <output :for="`${id}-grid`">{{ parameters.gridSpacing }}</output><input :id="`${id}-grid`" v-model.number="parameters.gridSpacing" :disabled="recording" type="range" min="1" max="4" step="1" /></label>
        <button class="admin-text-button" :disabled="recording" @click="resetScene"><span class="material-symbols-rounded">restart_alt</span>Reset view</button>
      </div>
    </details>
    <AdminDialog :open="saveOpen" title="Save simulated route" @close="saveOpen = false">
      <p class="admin-muted">{{ steps.length }} simulated movements for {{ unit.name }}. This is a local draft, not a robot navigation route.</p>
      <form class="admin-form" @submit.prevent="saveRecording">
        <label>Route name<input v-model="routeName" maxlength="100" required placeholder="e.g. Entrance to Fiction" /></label>
        <label>Destination shelf <span class="field-optional">Optional</span><select v-model="destination"><option value="">No destination</option><option v-for="code in adminState.shelves" :key="code" :value="code">{{ adminState.shelfDetails[code]?.name || code }} · {{ code }}</option></select></label>
        <p v-if="saveError" class="admin-form-error" role="alert">{{ saveError }}</p>
        <div class="admin-form-actions"><AppButton type="button" variant="secondary" @click="saveOpen = false">Keep draft</AppButton><AppButton type="submit" :disabled="!routeName.trim() || !steps.length">Save locally</AppButton></div>
      </form>
    </AdminDialog>
    <AdminDialog :open="discardOpen" title="Discard recording?" @close="discardOpen = false"><p class="admin-muted">The {{ steps.length }} unsaved simulation steps will be removed.</p><div class="admin-form-actions"><AppButton variant="secondary" @click="discardOpen = false">Keep draft</AppButton><AppButton @click="discardDraft">Discard draft</AppButton></div></AdminDialog>
  </div>
</template>
<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, useId, watch } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import AdminDialog from './AdminDialog.vue'
import AtlasScene from './AtlasScene.vue'
import { useAdminStore } from '../useAdminStore.js'
import { useAtlasStore } from '../useAtlasStore.js'
import { moveSimulation } from '../atlasScene.js'
import { apiMode, backend } from '@/services/api.js'
const props = defineProps({ unit: { type: Object, required: true }, active: Boolean })
const emit = defineEmits(['saved'])
const id = useId()
const { state: adminState } = useAdminStore()
const { saveRoute, storageError } = useAtlasStore()
const workspace = ref(null), fullscreen = ref(false), held = ref(''), recording = ref(false)
const steps = ref([]), notice = ref(''), saveOpen = ref(false), discardOpen = ref(false), saveError = ref(''), routeName = ref(''), destination = ref('')
const pose = reactive({ x: 0, z: 0, yaw: 0 })
const defaults = () => ({ fov: 90, height: 1.25, obstacleScale: 1, gridSpacing: 2 })
const parameters = reactive(defaults())
const camera = computed(() => ({ ...pose, y: parameters.height, fov: parameters.fov }))
const driveReady = computed(() => Boolean(props.unit.telemetry?.drive_ready && !props.unit.telemetry?.guidance_task_id && props.unit.telemetry?.status === 'Online' && !props.unit.connectionError))
const motionLabel = computed(() => apiMode ? (held.value ? `Requesting ${held.value} · release to send stop` : 'No direction held · movement not verified') : ({ forward: 'Moving forward', reverse: 'Reversing', left: 'Turning left', right: 'Turning right' }[held.value] || 'Stopped'))
const directions = [{ command: 'forward', label: 'Forward', icon: 'keyboard_arrow_up' }, { command: 'reverse', label: 'Reverse', icon: 'keyboard_arrow_down' }, { command: 'left', label: 'Left', icon: 'keyboard_arrow_left' }, { command: 'right', label: 'Right', icon: 'keyboard_arrow_right' }]
const keyCommands = { ArrowUp: 'forward', ArrowDown: 'reverse', ArrowLeft: 'left', ArrowRight: 'right', w: 'forward', s: 'reverse', a: 'left', d: 'right' }
let movementTimer, startedAt = 0
let commandQueue = Promise.resolve(), pendingCommands = 0
function sendLive(command, force = false) {
  if (!force && pendingCommands) return
  pendingCommands++
  commandQueue = commandQueue.then(async () => {
    if (command !== 'stop' && (!props.active || !driveReady.value || held.value !== command)) return
    try {
      await backend.command(props.unit.identifier, command === 'reverse' ? 'backward' : command)
      notice.value = command === 'stop' ? 'Stop accepted by backend; physical stop is not acknowledged.' : `${command} accepted by backend; physical movement is not acknowledged.`
    } catch (error) {
      clearInterval(movementTimer); held.value = ''
      notice.value = `${error.message} Movement is unverified. Use a physical emergency stop if necessary.`
    }
  }).finally(() => { pendingCommands-- })
}
function move(command) {
  if (!props.active || saveOpen.value || discardOpen.value) return
  if (apiMode) { if (driveReady.value) sendLive(command); return }
  Object.assign(pose, moveSimulation(pose, command))
  if (recording.value) {
    steps.value.push({ command, at: Math.round(performance.now() - startedAt), pose: { ...pose } })
    if (steps.value.length >= 5000) { finishRecording(); notice.value = 'Recording limit reached. Save this route before starting another.' }
  }
}
function stop() { clearInterval(movementTimer); movementTimer = null; held.value = ''; if (apiMode) sendLive('stop', true) }
// Leaving an idle manual workspace must not cancel a separately running guidance task.
// A held manual direction still always sends Stop on release/navigation/focus loss.
function stopHeld() { if (held.value) stop() }
function begin(command) {
  if (held.value === command || !props.active || saveOpen.value || discardOpen.value || (apiMode && !driveReady.value)) return
  stop(); held.value = command; move(command)
  if (held.value) movementTimer = setInterval(() => move(command), apiMode ? 250 : 120)
}
function pointerDown(event, command) {
  if (event.button !== 0) return
  event.preventDefault(); event.currentTarget.focus(); event.currentTarget.setPointerCapture(event.pointerId); begin(command)
}
function keyboardClick(event, command) { if (event.detail === 0) { if (apiMode) { begin(command); setTimeout(stop, 250) } else { stop(); move(command) } } }
function keyDown(event) {
  if (event.altKey || event.ctrlKey || event.metaKey) return
  const command = keyCommands[event.key] || keyCommands[event.key.toLowerCase()]
  if (command) { event.preventDefault(); if (!event.repeat) begin(command) }
  if (event.key === 'Escape') stop()
}
function keyUp(event) { if ((keyCommands[event.key] || keyCommands[event.key.toLowerCase()]) === held.value) { event.preventDefault(); stop() } }
function onFocusOut(event) { if (!event.currentTarget.contains(event.relatedTarget)) stopHeld() }
function resetScene() { stop(); Object.assign(pose, { x: 0, z: 0, yaw: 0 }); Object.assign(parameters, defaults()) }
function startRecording() { stop(); steps.value = []; startedAt = performance.now(); recording.value = true; notice.value = ''; routeName.value = ''; destination.value = ''; saveError.value = '' }
function finishRecording() {
  stop(); recording.value = false
  if (steps.value.length) saveOpen.value = true
  else notice.value = 'No movements recorded. Start a recording and move in the simulation.'
}
function discardDraft() { stop(); steps.value = []; discardOpen.value = false; notice.value = 'Simulation draft discarded.' }
function saveRecording() {
  try {
    if (destination.value && !adminState.shelves.includes(destination.value)) throw new Error('Choose a registered shelf.')
    saveRoute({ name: routeName.value, atlasId: props.unit.id, destination: destination.value, steps: steps.value })
    steps.value = []; saveOpen.value = false; saveError.value = ''
    notice.value = storageError.value ? 'Route saved for this visit only. Browser storage is unavailable.' : 'Simulation saved in this browser. Find it in Saved Routes.'
    emit('saved', notice.value)
  } catch (cause) { saveError.value = cause.message }
}
async function toggleFullscreen() {
  stop()
  try {
    if (document.fullscreenElement) await document.exitFullscreen()
    else if (workspace.value.requestFullscreen) await workspace.value.requestFullscreen()
    else notice.value = 'Fullscreen is not supported by this browser.'
  } catch { notice.value = 'Fullscreen is unavailable in this browser window.' }
}
function updateFullscreen() { fullscreen.value = document.fullscreenElement === workspace.value }
function loseFocus() { stopHeld() }
function visibilityChange() { if (document.hidden) stopHeld() }
watch(() => props.active, active => {
  if (!active) { stopHeld(); if (recording.value) { recording.value = false; notice.value = 'Recording stopped. Your simulation draft is ready to save.' } }
})
watch(driveReady, ready => { if (apiMode && !ready && held.value) stop() })
onMounted(() => { window.addEventListener('blur', loseFocus); document.addEventListener('visibilitychange', visibilityChange); document.addEventListener('fullscreenchange', updateFullscreen) })
onBeforeUnmount(() => { stopHeld(); window.removeEventListener('blur', loseFocus); document.removeEventListener('visibilitychange', visibilityChange); document.removeEventListener('fullscreenchange', updateFullscreen) })
</script>
