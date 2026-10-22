<template>
  <section class="guidance-monitor" aria-label="Atlas guidance operations">
    <header><h2>Shelf guidance</h2><AppButton variant="secondary" :disabled="pending" @click="stop">Emergency stop</AppButton></header>
    <p class="admin-muted">Operator-confirmed checkpoints only. A stop request is not proof that motors have stopped.</p>
    <p v-if="active" role="status">{{ active.shelf_name }} · {{ active.status }} · {{ active.progress_index }}/{{ active.segment_count }} segments acknowledged</p>
    <p v-else class="admin-muted">No active guidance task.</p>
    <p v-if="waiting" class="admin-muted">{{ waiting }} request(s) waiting. No display code until Atlas is confirmed home.</p>
    <form v-if="!active && !waiting" class="admin-form" @submit.prevent="saveHome">
      <label>Home station<select v-model="homeId" required><option value="">Select home checkpoint</option><option v-for="point in points" :key="point.id" :value="point.id">{{ point.name }}</option></select></label>
      <AppButton variant="secondary" :disabled="pending || !homeId">Configure home station</AppButton>
    </form>
    <p v-if="active?.status === 'awaiting_otp'" class="admin-muted">Code displayed only on Atlas. The requesting reader must enter it on their own account.</p>
    <p v-if="active?.awaiting_confirmation">{{ active.phase === 'return' ? 'Return replay finished. Check Atlas is stopped at home.' : 'Outbound replay finished. Check Atlas is stopped at the shelf.' }} Attestation alone does not complete arrival: the ESP32 must also report the matching session.</p>
    <form class="admin-form" @submit.prevent="confirm">
      <label>Actual checkpoint<select v-model="checkpointId" :disabled="Boolean(active)"><option value="">Select physical location</option><option v-for="point in points" :key="point.id" :value="point.id">{{ point.name }}</option></select></label>
      <label class="confirmation"><input v-model="confirmed" type="checkbox" :disabled="Boolean(active && !active.awaiting_confirmation)" required /> I have checked Atlas is physically stopped at this checkpoint.</label>
      <AppButton :disabled="pending || !confirmed || !checkpointId || Boolean(active && !active.awaiting_confirmation)">{{ active ? 'Attest physical arrival' : 'Confirm home placement' }}</AppButton>
    </form>
    <p v-if="error" class="admin-form-error" role="alert">{{ error }}</p>
    <p v-if="message" role="status">{{ message }}</p>
    <ul v-if="history.length"><li v-for="task in history.slice(0, 5)" :key="task.id"><strong>{{ task.shelf_name }}</strong> · {{ task.status }}<small>{{ task.id }}<template v-if="task.failure_reason"> · {{ task.failure_reason }}</template></small></li></ul>
  </section>
</template>
<script setup>
import { computed, onMounted, onBeforeUnmount, ref, watch } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import { backend, authApi } from '@/services/api.js'
const props = defineProps({ robotId: { type: String, required: true } })
const history = ref([]), points = ref([]), checkpointId = ref(''), homeId = ref(''), confirmed = ref(false), pending = ref(false), error = ref(''), message = ref('')
const active = computed(() => history.value.find(task => ['awaiting_otp', 'queued', 'awaiting_robot', 'running', 'returning_home'].includes(task.status)))
const waiting = computed(() => history.value.filter(task => task.status === 'waiting_for_atlas').length)
watch([checkpointId, () => active.value?.id, () => active.value?.awaiting_confirmation], () => { confirmed.value = false })
let timer, disposed = false
async function load() {
  try {
    const tasks = await backend.robotGuidance(props.robotId)
    if (!disposed) { history.value = tasks; if (active.value?.awaiting_confirmation) checkpointId.value = active.value.current_checkpoint_id }
  } catch (cause) { if (!disposed) error.value = cause.message }
  if (!disposed) timer = setTimeout(load, 2000)
}
async function action(callback) {
  if (pending.value) return
  pending.value = true; error.value = ''; message.value = ''
  try { await authApi.session(); const result = await callback(); message.value = result.message; confirmed.value = false; history.value = await backend.robotGuidance(props.robotId) }
  catch (cause) { error.value = cause.message }
  finally { pending.value = false }
}
function confirm() {
  if (!confirmed.value || !checkpointId.value || (active.value && !active.value.awaiting_confirmation)) return
  const checkpoint = checkpointId.value, task = active.value?.id
  return action(() => backend.confirmPosition(props.robotId, checkpoint, task))
}
const stop = () => action(() => backend.stopGuidance(props.robotId))
const saveHome = () => { const checkpoint = homeId.value; return action(async () => { await backend.configureHome(props.robotId, checkpoint); checkpointId.value = checkpoint; return { message: 'Home configured. Confirm actual placement while Atlas reports stopped.' } }) }
onMounted(async () => { try { points.value = (await backend.checkpoints()).filter(point => point.is_active) } catch (cause) { error.value = cause.message } if (!disposed) load() })
onBeforeUnmount(() => { disposed = true; clearTimeout(timer) })
</script>
<style scoped>
.guidance-monitor { padding:1.5rem 0; border-top:1px solid rgba(255,255,255,.08); }
header { display:flex; justify-content:space-between; align-items:center; gap:1rem; flex-wrap:wrap; } h2 { font-size:1rem; }
.admin-form { max-width:38rem; margin:1.5rem 0; } .confirmation { display:flex; align-items:center; gap:.65rem; } .confirmation input { width:auto; }
ul { padding:0; list-style:none; } li { padding:.7rem 0; border-top:1px solid rgba(255,255,255,.06); font-size:.85rem; } small { display:block; overflow-wrap:anywhere; color:var(--text-secondary); margin-top:.3rem; }
</style>
