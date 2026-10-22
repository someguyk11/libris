<template>
  <section class="reader-guidance" aria-label="Atlas shelf guidance">
    <h2>Find it with Atlas</h2>
    <p v-if="!bookId">Atlas guidance is available for books in the live catalogue. Search Discover to find one.</p>
    <template v-else>
      <p>Atlas can guide you to this shelf. At its home station, enter the code shown on Atlas to begin. This does not borrow the book.</p>
      <p v-if="task" role="status">{{ label }}<span v-if="task.shelf_name"> · {{ task.shelf_name }}</span></p>
      <p v-if="task?.failure_reason">{{ task.failure_reason }}</p>
      <AppButton v-if="!active" :disabled="pending" @click="requestGuide">{{ pending ? 'Checking…' : 'Guide me to this shelf' }}</AppButton>
      <form v-if="task?.status === 'awaiting_otp'" class="atlas-code-form" @submit.prevent="verify">
        <label for="atlas-guidance-code">Six-digit code on Atlas</label>
        <input id="atlas-guidance-code" v-model="code" type="text" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="off" required :disabled="pending || remaining === 0" aria-describedby="atlas-code-help" />
        <p id="atlas-code-help">Go to {{ task.robot_id }} at its home station. Read its display · {{ remaining }} seconds remaining. Never use your email verification code here.</p>
        <AppButton :disabled="pending || !/^[0-9]{6}$/.test(code) || remaining === 0">{{ pending ? 'Verifying…' : 'Verify and start guidance' }}</AppButton>
      </form>
      <button v-if="['waiting_for_atlas', 'awaiting_otp', 'queued'].includes(task?.status)" class="guidance-link" :disabled="pending" @click="cancel">Cancel request</button>
      <p v-if="['queued', 'awaiting_robot', 'running'].includes(task?.status)">Stay with Atlas. If it needs to stop, ask library staff.</p>
      <p v-if="task?.status === 'waiting_for_atlas'">Your request is waiting. No code is issued until Atlas is available at its confirmed home station.</p>
      <p v-if="task?.status === 'returning_home'">You have reached the shelf. Atlas must return home before the next request can begin.</p>
      <button v-if="signIn" class="guidance-link" @click="current = 'auth'">Sign in to request guidance</button>
    </template>
    <p v-if="error" role="alert">{{ error }}</p>
  </section>
</template>
<script setup>
import { computed, onMounted, onBeforeUnmount, ref, watch } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import { backend, authApi } from '@/services/api.js'
import { useNav } from '@/composables/useNav'
const props = defineProps({ bookId: Number })
const { current } = useNav()
const task = ref(null), pending = ref(false), error = ref(''), signIn = ref(false), code = ref(''), clock = ref(Date.now())
let timer, clockTimer, disposed = false, generation = 0
const active = computed(() => ['waiting_for_atlas', 'awaiting_otp', 'queued', 'awaiting_robot', 'running', 'returning_home'].includes(task.value?.status))
const remaining = computed(() => Math.max(0, Math.ceil((Date.parse(task.value?.otp_expires_at || '') - clock.value) / 1000)) || 0)
const label = computed(() => task.value?.awaiting_confirmation ? (task.value.phase === 'return' ? 'Home reached · staff confirming arrival' : 'Shelf reached · staff confirming arrival') : ({ waiting_for_atlas: 'Waiting for Atlas', awaiting_otp: 'Atlas is ready · enter its display code', queued: 'Code verified · guidance authorized', awaiting_robot: 'Atlas preparing', running: 'Atlas guiding', returning_home: 'Atlas returning home', completed: 'Guidance finished · Atlas is home', failed: 'Guidance stopped', expired: 'Request expired · you can try again', cancelled: 'Request cancelled' }[task.value?.status] || ''))
watch(() => `${task.value?.id}:${task.value?.status}`, () => { code.value = '' })
function report(cause) { error.value = cause.message; signIn.value = cause.status === 401 }
async function restore() {
  const version = ++generation
  task.value = null; error.value = ''; signIn.value = false
  if (!props.bookId) return
  try {
    const session = await authApi.session()
    if (session.purpose !== 'user') return
    const tasks = await backend.myGuidance()
    if (!disposed && version === generation) task.value = tasks.find(item => item.book_id === props.bookId) || null
  } catch (cause) { if (cause.status !== 401 && !disposed && version === generation) report(cause) }
}
async function refresh() {
  if (disposed) return
  const id = task.value?.id, version = generation
  try { if (active.value) { const result = await backend.guidance(id); if (!disposed && version === generation) task.value = result } }
  catch (cause) { if (!disposed) report(cause) }
  if (!disposed) timer = setTimeout(refresh, 2000)
}
async function requestGuide() {
  if (pending.value) return
  pending.value = true; error.value = ''; signIn.value = false
  try { await authApi.session(); task.value = await backend.requestGuidance(props.bookId) }
  catch (cause) { report(cause) }
  finally { pending.value = false }
}
async function cancel() {
  pending.value = true
  try { task.value = await backend.cancelGuidance(task.value.id) }
  catch (cause) { report(cause) }
  finally { pending.value = false }
}
async function verify() {
  if (pending.value || !/^[0-9]{6}$/.test(code.value) || !remaining.value) return
  const id = task.value.id, submitted = code.value
  pending.value = true; error.value = ''; code.value = ''
  try { task.value = await backend.verifyGuidance(id, submitted) }
  catch (cause) { report(cause); try { task.value = await backend.guidance(id) } catch { /* Keep the original verification error. */ } }
  finally { pending.value = false }
}
onMounted(() => { restore(); timer = setTimeout(refresh, 2000); clockTimer = setInterval(() => { clock.value = Date.now() }, 1000) })
watch(() => props.bookId, restore)
onBeforeUnmount(() => { disposed = true; clearTimeout(timer); clearInterval(clockTimer); code.value = '' })
</script>
<style scoped>
.reader-guidance { margin-top:1.5rem; padding-top:1.5rem; border-top:1px solid var(--border-subtle,rgba(255,255,255,.09)); }
h2 { font:600 1rem var(--font-body); color:var(--text-primary); }
p { font-size:.85rem; line-height:1.6; color:var(--text-secondary); margin:.65rem 0; }
.guidance-link { display:block; margin-top:.8rem; padding:0; background:none; border:0; color:var(--accent); cursor:pointer; }
.atlas-code-form { display:grid; justify-items:start; gap:.65rem; margin:1rem 0; }
.atlas-code-form label { font-size:.85rem; color:var(--text-primary); }
.atlas-code-form input { box-sizing:border-box; width:100%; max-width:15rem; border:1px solid var(--border-subtle); border-radius:var(--radius-sm,10px); background:var(--bg-surface,transparent); color:var(--text-primary); padding:.75rem; font:600 1.1rem var(--font-body); letter-spacing:.3em; }
.atlas-code-form input:focus-visible { outline:2px solid var(--accent); outline-offset:2px; }
</style>
