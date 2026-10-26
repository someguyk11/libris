<template>
  <section class="atlas-recovery">
    <div class="monitor-section-heading"><div><h2>Connection recovery</h2><p class="admin-muted">Polled by the ESP32. Recovery stops navigation; it never resumes a route.</p></div><span>{{ !telemetry?.recovery_capable ? 'FIRMWARE UPDATE REQUIRED' : telemetry?.drive_ready && !connectionError ? 'COMMAND POLLING LIVE' : 'COMMAND POLLING STALE' }}</span></div>
    <dl class="monitor-state">
      <div><dt>Automatic Wi-Fi reconnection</dt><dd>{{ telemetry?.recovery_capable ? (telemetry.auto_reconnect ? 'Enabled · bounded retry backoff' : 'Disabled · local setup may be needed') : 'Not reported' }}</dd></div>
      <div><dt>Last recovery request</dt><dd aria-live="polite">{{ recoveryLabel }}</dd></div>
      <div><dt>Firmware boot</dt><dd class="atlas-recovery-id">{{ telemetry?.boot_id || 'Not reported' }}</dd></div>
      <div><dt>Backend route replay</dt><dd>Not connected in current firmware</dd></div>
      <div><dt>LSM6DSO self-test / low-pass filter</dt><dd>Not implemented · no IMU driver</dd></div>
    </dl>
    <div class="atlas-recovery-actions">
      <AppButton variant="secondary" :disabled="!ready" @click="submit('auto_reconnect', !telemetry.auto_reconnect)">{{ telemetry?.auto_reconnect ? 'Disable auto reconnect…' : 'Enable auto reconnect' }}</AppButton>
      <AppButton variant="secondary" :disabled="!ready" @click="submit('reconnect')">Reconnect Wi-Fi</AppButton>
      <AppButton variant="secondary" :disabled="!ready" @click="restartOpen = true">Restart ESP32…</AppButton>
    </div>
    <p class="admin-muted">Requests expire after 60 seconds. “Accepted” means the stopped firmware acknowledged the request, not that recovery finished. A restart is confirmed only by a new boot ID. Disabling retries can require the local setup portal or a power cycle to reconnect.</p>
    <p v-if="error" class="admin-form-error" role="alert">{{ error }}</p>
    <p v-if="notice" class="admin-muted" role="status">{{ notice }}</p>
    <AdminDialog :open="restartOpen" title="Restart this ESP32?" @close="restartOpen = false">
      <p class="admin-muted">Motors will be stopped before restarting. Wi-Fi credentials are kept. Any recording or guidance task is interrupted and will not resume.</p>
      <div class="admin-form-actions"><AppButton variant="secondary" @click="restartOpen = false">Cancel</AppButton><AppButton :disabled="!ready" @click="submit('restart')">Queue restart</AppButton></div>
    </AdminDialog>
    <AdminDialog :open="disableOpen" title="Disable automatic reconnection?" @close="disableOpen = false">
      <p class="admin-muted">If Wi-Fi drops, backend controls cannot reach the robot. Use the local setup portal or power cycle to reconnect. Navigation will be stopped.</p>
      <div class="admin-form-actions"><AppButton variant="secondary" @click="disableOpen = false">Keep enabled</AppButton><AppButton :disabled="!ready" @click="send('auto_reconnect', false)">Disable retries</AppButton></div>
    </AdminDialog>
  </section>
</template>
<script setup>
import { computed, ref, watch } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import AdminDialog from './AdminDialog.vue'
import { backend } from '@/services/api.js'
const props = defineProps({ robotId: { type: String, required: true }, telemetry: Object, connectionError: String })
const pending = ref(false), restartOpen = ref(false), disableOpen = ref(false), error = ref(''), notice = ref('')
const submitted = ref(null)
const recovery = computed(() => {
  const reported = props.telemetry?.recovery
  return submitted.value && Date.now() / 1000 - submitted.value.created_at < 65 && reported?.request_id !== submitted.value.request_id ? submitted.value : reported
})
watch(() => props.telemetry?.recovery, value => {
  if (value?.request_id === submitted.value?.request_id) submitted.value = null
})
watch(() => props.robotId, () => { submitted.value = null; notice.value = ''; error.value = '' })
const ready = computed(() => !pending.value && !props.connectionError && props.telemetry?.drive_ready && props.telemetry?.recovery_capable && !['queued', 'accepted'].includes(recovery.value?.status))
const recoveryLabel = computed(() => {
  const value = recovery.value
  return value ? `${value.action.replaceAll('_', ' ')} · ${value.status}` : 'No recovery requested'
})
function submit(action, enabled) {
  if (action === 'auto_reconnect' && !enabled) { disableOpen.value = true; return }
  return send(action, enabled)
}
async function send(action, enabled) {
  if (!ready.value) return
  pending.value = true; error.value = ''; notice.value = ''
  try {
    // Retried transport is not automatic; backend request IDs still deduplicate double submissions.
    const body = { action, request_id: crypto.randomUUID() }
    if (action === 'auto_reconnect') body.enabled = enabled
    const result = await backend.recoverRobot(props.robotId, body)
    submitted.value = result.data
    restartOpen.value = false; disableOpen.value = false
    notice.value = `Recovery ${result.data.status}. Waiting for firmware acknowledgement.`
  } catch (cause) { error.value = cause.message }
  finally { pending.value = false }
}
</script>
<style scoped>
.atlas-recovery-actions { display: flex; flex-wrap: wrap; gap: 10px; margin: 18px 0; }
.atlas-recovery-id { overflow-wrap: anywhere; }
.atlas-recovery .monitor-section-heading { flex-wrap: wrap; gap: 12px; }
</style>
