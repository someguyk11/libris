import { computed, ref } from 'vue'
import { apiMode, authApi } from '../services/api.js'

const session = ref(null), ready = ref(!apiMode), error = ref('')
const user = computed(() => session.value?.purpose === 'user' ? session.value.user : null)
const isStaff = computed(() => user.value?.is_staff === true)
let pending
let revision = 0

function accept(value) {
  revision++
  session.value = value
  ready.value = true
  error.value = ''
}
async function refresh() {
  if (!apiMode) return
  if (pending) return pending
  const startedAt = revision
  pending = (async () => {
    try { const value = await authApi.session(); if (startedAt === revision) accept(value) }
    catch (cause) {
      if (startedAt !== revision) return
      session.value = null
      error.value = cause.status === 401 ? '' : cause.message
      ready.value = true
    } finally { pending = null }
  })()
  return pending
}
async function signOut() {
  try { const result = await authApi.logout(); accept(null); return result }
  catch (cause) { if (cause.status === 401) { accept(null); return { signed_out: true, provider_revoked: false } } throw cause }
}
if (typeof window !== 'undefined') window.addEventListener('libris:session-expired', () => accept(null))
export function useSession() { return { session, user, isStaff, ready, error, accept, refresh, signOut } }
