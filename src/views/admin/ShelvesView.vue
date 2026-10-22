<template>
  <section class="operations-page shelf-page admin-page-enter">
    <a class="operations-back" href="#/admin"><span class="material-symbols-rounded">arrow_back</span>Back to dashboard</a>
    <header class="operations-heading">
      <div><p class="admin-eyebrow">LIBRARY LOCATIONS</p><h1>{{ editingId === null ? 'Register Shelf' : 'Edit Shelf' }}</h1><p>Give every book a place. Organise your library by shelf and section.</p></div>
    </header>

    <form class="admin-form shelf-form" @submit.prevent="submit">
      <div class="shelf-form-intro"><span class="material-symbols-rounded">shelves</span><div><h2>Shelf details</h2><p>A name, an identifier, and a place in your library.</p></div></div>
      <div class="admin-form-grid">
        <label>Shelf name<input v-model="form.name" required maxlength="100" placeholder="e.g. Fiction Shelf A" autocomplete="off" /></label>
        <label v-if="!apiMode">Shelf identifier<input v-model="form.code" required maxlength="20" pattern="[A-Za-z0-9\-]{2,20}" placeholder="e.g. SH-D01" aria-describedby="shelf-code-help" autocomplete="off" /><small id="shelf-code-help">Unique code used in the book catalogue.</small><small v-if="duplicate" class="admin-form-error">This identifier is already registered.</small></label>
        <label v-else>Shelf identifier<input :value="editingId ?? 'Assigned by the database on registration'" disabled /><small>Stable numeric ID; not an Atlas coordinate.</small></label>
        <label v-if="!apiMode">Shelf type<select v-model="form.category" required><option v-for="category in shelfCategories" :key="category">{{ category }}</option></select></label>
        <label v-if="apiMode">Floor<input v-model.number="form.floor" type="number" step="1" required /></label>
        <label>Location / section<input v-model="form.section" :required="!apiMode" maxlength="100" list="shelf-sections" placeholder="e.g. Ground floor · Fiction" /><datalist id="shelf-sections"><option v-for="section in sections" :key="section" :value="section" /></datalist></label>
      </div>
      <label>Description <span class="field-optional">Optional</span><textarea v-model="form.description" rows="3" maxlength="500" placeholder="Anything useful to know about this shelf…"></textarea></label>
      <details v-if="!apiMode" class="shelf-location-details"><summary>Physical location <span class="field-optional">Optional</span></summary><div class="admin-form-grid"><label>Aisle<input v-model="form.aisle" maxlength="40" placeholder="e.g. Aisle 3" /></label><label>Row<input v-model="form.row" maxlength="40" placeholder="e.g. Row B" /></label></div></details>
      <p v-else class="admin-muted">Category, aisle, and row are not supported by this backend. Path ID {{ form.path_id }} is a backend placeholder, not a working navigation route.</p>
      <p class="admin-muted shelf-local-note"><span class="material-symbols-rounded">info</span>{{ apiMode ? 'Saved to the backend database.' : 'Saved in this browser.' }} Shelf registration does not create an Atlas navigation route.</p>
      <p v-if="error" class="admin-form-error" role="alert">{{ error }}</p>
      <p v-if="success" class="shelf-success" role="status"><span class="material-symbols-rounded">check_circle</span>{{ success }}</p>
      <div class="shelf-submit"><AppButton type="submit" :disabled="!valid || pending || loading"><span class="material-symbols-rounded" aria-hidden="true">add_location_alt</span>{{ pending ? 'Saving…' : editingId === null ? 'Register Shelf' : 'Save Shelf' }}</AppButton><button v-if="editingId !== null" type="button" class="admin-text-button" :disabled="pending" @click="reset">Finish editing</button></div>
    </form>
    <section v-if="apiMode && editingId !== null" class="admin-form shelf-form" aria-label="Atlas destination mapping">
      <h2>Atlas destination</h2>
      <p class="admin-muted">Link this shelf to an existing confirmed checkpoint. This does not record or activate a route.</p>
      <label>Destination checkpoint<select v-model="destination"><option value="">Not mapped</option><option v-for="point in checkpoints" :key="point.id" :value="point.id">{{ point.name }}</option></select></label>
      <AppButton :disabled="pending" @click="saveDestination">Save destination mapping</AppButton>
      <p v-if="mappingMessage" role="status">{{ mappingMessage }}</p>
      <p v-if="mappingError" class="admin-form-error" role="alert">{{ mappingError }}</p>
    </section>

    <details class="registered-shelves" :open="apiMode"><summary>Registered shelves <span>{{ state.shelves.length }}</span></summary><ul><li v-for="code in state.shelves" :key="code"><span class="material-symbols-rounded">shelves</span><div><strong>{{ state.shelfDetails[code]?.name || code }}</strong><small>{{ state.shelfDetails[code]?.section || 'Location not specified' }}<template v-if="apiMode"> · Floor {{ state.shelfDetails[code]?.floor }}</template></small></div><code>{{ code }}</code><template v-if="apiMode"><button class="admin-icon-button" :aria-label="'Edit shelf ' + code" :disabled="pending" @click="edit(code)"><span class="material-symbols-rounded">edit</span></button><button class="admin-icon-button" :aria-label="'Delete shelf ' + code" :disabled="pending" @click="deleteTarget = state.shelfDetails[code]"><span class="material-symbols-rounded">delete</span></button></template></li></ul><p v-if="!state.shelves.length" class="admin-muted">{{ loading ? 'Loading shelves…' : 'No shelves registered.' }}</p></details>
    <AdminDialog :open="Boolean(deleteTarget)" title="Delete shelf?" @close="deleteTarget = null"><p class="admin-muted">Delete {{ deleteTarget?.name }} from the database? Shelves assigned to books cannot be deleted.</p><p v-if="deleteError" class="admin-form-error" role="alert">{{ deleteError }}</p><div class="admin-form-actions"><AppButton variant="secondary" :disabled="pending" @click="deleteTarget = null">Keep shelf</AppButton><AppButton :disabled="pending" @click="confirmDelete">{{ pending ? 'Deleting…' : 'Delete shelf' }}</AppButton></div></AdminDialog>
  </section>
</template>

<script setup>
import { computed, nextTick, reactive, ref } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import AdminDialog from './components/AdminDialog.vue'
import { shelfPayload } from '@/services/backendData.js'
import { shelfCategories, validateShelf } from './adminData.js'
import { useAdminStore } from './useAdminStore.js'
import { backend, authApi } from '@/services/api.js'
const { state, registerShelf, storageError, apiMode, loading, saveBackendShelf, deleteBackendShelf } = useAdminStore()
const defaults = () => ({ name: '', code: '', category: 'Books', section: '', description: '', aisle: '', row: '', floor: 1, path_id: 0 })
const form = reactive(defaults())
const checkpoints = ref([]), destination = ref(''), mappingError = ref(''), mappingMessage = ref('')
const error = ref(''), success = ref('')
const pending = ref(false), editingId = ref(null), deleteTarget = ref(null), deleteError = ref('')
const sections = computed(() => [...new Set(Object.values(state.shelfDetails).map(shelf => shelf?.section).filter(Boolean))])
const duplicate = computed(() => state.shelves.includes(form.code.trim().toUpperCase()))
const valid = computed(() => { try { apiMode ? shelfPayload(form) : validateShelf(form, state.shelves); return true } catch { return false } })
function reset() { editingId.value = null; Object.assign(form, defaults()) }
async function edit(code) {
  error.value = ''; success.value = ''; mappingError.value = ''; mappingMessage.value = ''
  Object.assign(form, defaults(), state.shelfDetails[code]); editingId.value = state.shelfDetails[code].id
  destination.value = state.shelfDetails[code].destinationCheckpointId || ''
  window.scrollTo({ top: 0, behavior: 'smooth' })
  try { checkpoints.value = (await backend.checkpoints()).filter(point => point.is_active) }
  catch (cause) { mappingError.value = cause.message }
}
async function saveDestination() {
  pending.value = true; mappingError.value = ''; mappingMessage.value = ''
  try {
    await authApi.session()
    await backend.mapShelf(editingId.value, destination.value)
    state.shelfDetails[String(editingId.value)].destinationCheckpointId = destination.value || null
    mappingMessage.value = 'Shelf destination mapping saved.'
  } catch (cause) { mappingError.value = cause.message }
  finally { pending.value = false }
}
async function confirmDelete() {
  pending.value = true; deleteError.value = ''
  try { const id = deleteTarget.value.id; await deleteBackendShelf(id); if (editingId.value === id) reset(); deleteTarget.value = null; success.value = 'Shelf deleted from the database.' }
  catch (cause) { deleteError.value = cause.message }
  finally { pending.value = false }
}
async function submit() {
  if (pending.value) return
  error.value = ''; success.value = ''; pending.value = true
  try {
    const shelf = await (apiMode ? saveBackendShelf(form, editingId.value) : registerShelf(form))
    await nextTick()
    success.value = `${shelf.name} saved ${apiMode ? 'to the database' : storageError.value ? 'for this visit only' : 'in this browser'}.`
    reset()
  } catch (cause) { error.value = cause.message }
  finally { pending.value = false }
}
</script>
