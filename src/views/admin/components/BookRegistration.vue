<template>
  <AdminDialog :open="true" :title="bulk ? 'Scan books to one shelf' : 'Register a book by ISBN'" @close="close">
    <div class="admin-form isbn-registration" :aria-busy="saving || looking">
      <p class="admin-muted">{{ bulk ? 'Select one shelf, scan consecutive ISBNs, then review each title before saving the batch. Up to 50 distinct editions.' : 'Scan a barcode or enter an ISBN. Look up available information, review it, then save.' }} Existing editions are not additional physical copies.</p>
      <label>Destination shelf<select v-model="shelf" :disabled="saving || looking || Boolean(results)" required><option value="" disabled>Select a shelf</option><option v-for="value in state.shelves" :key="value" :value="value">{{ state.shelfDetails[value]?.name || value }} · #{{ value }}</option></select></label>
      <p v-if="!state.shelves.length" class="admin-form-error">Register a shelf before adding books. <a href="#/admin/shelves">Go to shelves</a></p>
      <template v-if="!results">
        <IsbnScanner v-if="shelf && (bulk || !entries.length)" ref="scanner" :paused="looking || saving || entries.length >= 50" @detected="lookup" />
        <form v-if="bulk || !entries.length" class="isbn-manual" @submit.prevent="lookup(manual)">
          <label>Manual ISBN<input v-model="manual" inputmode="text" autocomplete="off" maxlength="64" placeholder="ISBN-10 or ISBN-13" :disabled="looking || saving" /></label>
          <button class="admin-button secondary" type="submit" :disabled="!shelf || looking || saving || entries.length >= 50">{{ looking ? 'Looking up…' : 'Look up ISBN' }}</button>
        </form>
        <p v-if="message" class="admin-notice" role="status">{{ message }}</p>
        <p v-if="error" class="admin-form-error" role="alert">{{ error }}</p>
        <section v-if="bulk && entries.length" aria-label="Scanned books">
          <h3>{{ entries.length }} scanned · {{ entries.filter(entry => entry.reviewed).length }} confirmed</h3>
          <ul class="isbn-queue"><li v-for="entry in entries" :key="entry.key"><button type="button" class="admin-text-button" :aria-current="activeKey === entry.key ? 'true' : undefined" @click="activeKey = entry.key">{{ entry.draft.title || entry.draft.isbn }} <small>{{ entry.existing ? 'Already registered · skipped on save' : entry.reviewed ? 'Confirmed' : 'Needs review' }}</small></button><button type="button" class="admin-icon-button" :aria-label="'Remove ' + (entry.draft.title || entry.draft.isbn)" :disabled="saving || looking" @click="remove(entry.key)"><span class="material-symbols-rounded">close</span></button></li></ul>
        </section>
        <form v-if="active" class="admin-form isbn-review" @submit.prevent="confirmEntry" @input="active.reviewed = false">
          <h3>Review {{ bulk ? 'scanned book' : 'book details' }}</h3>
          <p class="admin-muted">{{ active.note }} No catalogue changes have been made.</p>
          <p v-if="active.existing" class="admin-notice">This edition already exists on {{ shelfName(active.existing.shelf_id) }}. {{ bulk ? 'It will be skipped, without moving or changing its metadata.' : 'Only an explicitly confirmed shelf reassignment is allowed; existing metadata is preserved.' }}</p>
          <BookCover v-if="active.draft.bibliography.cover_url" :cover="active.draft.bibliography.cover_url" :title="active.draft.title" size="small" />
          <fieldset :disabled="Boolean(active.existing) || saving" class="isbn-fields admin-form-grid">
            <label class="admin-full-width">Title<input v-model="active.draft.title" required maxlength="200" /></label>
            <label>Author(s)<input v-model="active.draft.author" required maxlength="100" /></label>
            <label>ISBN-13 / ISBN-10<input v-model="active.draft.isbn" required maxlength="64" /></label>
            <label>Original ISBN-10 <span class="field-optional">Optional</span><input v-model="active.draft.bibliography.isbn10" maxlength="64" /></label>
            <label>Publisher <span class="field-optional">Optional</span><input v-model="active.draft.bibliography.publisher" maxlength="500" /></label>
            <label>Publication date <span class="field-optional">Optional</span><input v-model="active.draft.publish_date" maxlength="100" /></label>
            <label>Edition <span class="field-optional">Optional</span><input v-model="active.draft.bibliography.edition" maxlength="500" /></label>
            <label class="admin-full-width">Description <span class="field-optional">Optional</span><textarea v-model="active.draft.bibliography.description" rows="3" maxlength="20000" /></label>
            <label class="admin-full-width">Cover image HTTPS URL <span class="field-optional">Optional</span><input v-model="active.draft.bibliography.cover_url" maxlength="2000" /></label>
          </fieldset>
          <p v-if="active.existing && !bulk" class="admin-muted">Selected destination: {{ shelfName(shelf) }}.</p>
          <div class="admin-form-actions"><button v-if="!bulk" type="button" class="admin-button secondary" :disabled="saving" @click="resetSingle">Use another ISBN</button><button type="submit" class="admin-button" :disabled="saving || looking || !shelf">{{ saving ? 'Saving…' : bulk ? 'Confirm book' : active.existing ? 'Confirm shelf reassignment' : 'Register book' }}</button></div>
        </form>
        <div v-if="bulk" class="admin-form-actions"><button type="button" class="admin-button secondary" :disabled="saving" @click="close">Cancel</button><button type="button" class="admin-button" :disabled="saving || looking || !ready || !shelf" @click="saveBatch">{{ saving ? 'Saving batch…' : `Register ${entries.length} scanned books` }}</button></div>
      </template>
      <section v-else aria-live="polite">
        <h3>Batch complete</h3><p>{{ results.filter(item => item.status === 'created').length }} registered · {{ results.filter(item => item.status !== 'created').length }} skipped. Destination: {{ shelfName(shelf) }}.</p>
        <ul class="isbn-results"><li v-for="(item, index) in results" :key="index"><strong>{{ item.book?.title || item.isbn }}</strong><span>{{ item.status === 'created' ? 'Registered' : item.status === 'existing' ? `Already registered on ${shelfName(item.book.shelf_id)} — unchanged` : 'Duplicate ISBN — skipped' }}</span></li></ul>
        <div class="admin-form-actions"><button type="button" class="admin-button" @click="emit('close')">Done</button></div>
      </section>
    </div>
  </AdminDialog>
</template>
<script setup>
import { computed, onBeforeUnmount, ref } from 'vue'
import AdminDialog from './AdminDialog.vue'
import IsbnScanner from './IsbnScanner.vue'
import BookCover from '@/components/book/BookCover.vue'
import { backend } from '@/services/api.js'
import { canonicalIsbn } from '@/utils/isbn.js'
import { registrationDetails, reviewedBatch } from '@/services/bookRegistration.js'
import { useAdminStore } from '../useAdminStore.js'
const props = defineProps({ bulk: Boolean })
const emit = defineEmits(['close', 'saved'])
const { state, acceptRegisteredBooks } = useAdminStore()
const shelf = ref(''), scanner = ref(null), manual = ref(''), entries = ref([]), activeKey = ref(''), looking = ref(false), saving = ref(false), error = ref(''), message = ref(''), results = ref(null)
const active = computed(() => entries.value.find(entry => entry.key === activeKey.value))
const ready = computed(() => entries.value.length && entries.value.every(entry => entry.reviewed))
const shelfName = value => state.shelfDetails[String(value)]?.name || `shelf #${value}`
let alive = true
onBeforeUnmount(() => { alive = false; scanner.value?.stop() })
async function lookup(value) {
  if (looking.value || saving.value || !shelf.value) return
  error.value = ''; message.value = ''
  let isbn
  try { isbn = canonicalIsbn(value) } catch (reason) { error.value = reason.message; return }
  if (entries.value.some(entry => entry.key === isbn || safeIdentity(entry.draft.isbn) === isbn)) { message.value = `${isbn} is already in this batch. No extra copy was added.`; return }
  if (entries.value.length >= 50) { error.value = 'This batch is full. Review and save it before scanning more books.'; return }
  if (!props.bulk) scanner.value?.stop()
  looking.value = true
  let response, note
  try {
    response = await backend.lookupIsbn(isbn)
    note = response.existing ? 'Using existing catalogue information.' : response.found ? 'Available metadata from Open Library. Check every field; missing information is left blank.' : 'No matching edition was found. Enter the book details manually.'
  } catch (reason) { note = reason.message + ' You can enter the details manually.' }
  if (!alive) return
  const source = response?.existing || response?.metadata || {}
  const entry = { key: isbn, reviewed: false, existing: response?.existing || null, note, draft: { isbn, title: source.title || '', author: source.author || '', publish_date: source.publish_date || '', bibliography: { ...(source.bibliography || {}) } } }
  entries.value.push(entry); activeKey.value = isbn; manual.value = ''; looking.value = false
  message.value = props.bulk ? `ISBN ${isbn} added for review. Keep scanning or confirm its details below.` : ''
}
function safeIdentity(value) { try { return canonicalIsbn(value) } catch { return null } }
function remove(key) { entries.value = entries.value.filter(entry => entry.key !== key); if (activeKey.value === key) activeKey.value = entries.value.at(-1)?.key || ''; error.value = '' }
function resetSingle() { entries.value = []; activeKey.value = ''; error.value = ''; message.value = '' }
async function confirmEntry() {
  error.value = ''
  try {
    if (!active.value) return
    // Existing records may contain legacy fields; a bulk skip needs no metadata correction.
    if (props.bulk) {
      if (active.value.existing) canonicalIsbn(active.value.draft.isbn)
      else registrationDetails(active.value.draft)
      active.value.reviewed = true; activeKey.value = ''; message.value = 'Book confirmed. Scan the next ISBN or save the batch.'
      return
    }
    const details = registrationDetails(active.value.draft)
    saving.value = true; scanner.value?.stop()
    const book = await backend.registerBook({ ...details, shelf_id: Number(shelf.value), require_new: !active.value.existing, expected_existing_id: active.value.existing?.id || null })
    acceptRegisteredBooks([book]); emit('saved', active.value.existing ? 'Shelf assignment saved' : 'Book registered'); emit('close')
  } catch (reason) { error.value = reason.message + (reason.status === 409 ? ' Use another ISBN, then look up this ISBN again to review the current catalogue record.' : '') }
  finally { saving.value = false }
}
async function saveBatch() {
  if (saving.value || looking.value) return
  error.value = ''
  try {
    const batch = reviewedBatch(entries.value, shelf.value)
    saving.value = true; scanner.value?.stop()
    const response = await backend.registerBulk(batch)
    acceptRegisteredBooks(response.results.filter(item => item.book).map(item => item.book))
    results.value = response.results; emit('saved', 'ISBN batch processed')
  } catch (reason) { error.value = reason.message + ' Your reviewed batch is still here. If the result is uncertain, retrying skips already registered editions without moving them.' }
  finally { saving.value = false }
}
function close() {
  if (saving.value) { error.value = 'Saving is in progress. Please wait for the result before closing.'; return }
  if (!results.value && entries.value.length && !window.confirm('Discard the scanned books and unsaved review changes?')) return
  scanner.value?.stop(); emit('close')
}
</script>
<style scoped>
.isbn-registration { gap: 18px; }
.isbn-manual { display: flex; align-items: end; gap: 10px; }
.isbn-manual label { flex: 1; min-width: 0; }
.isbn-fields { border: 0; padding: 0; margin: 0; min-width: 0; }
.isbn-review { border-top: 1px solid var(--admin-border, #ffffff15); padding-top: 18px; }
.isbn-queue, .isbn-results { list-style: none; margin: 10px 0; padding: 0; max-height: 240px; overflow: auto; }
.isbn-queue li { display: flex; justify-content: space-between; gap: 10px; padding: 8px 0; border-bottom: 1px solid var(--admin-border, #ffffff15); }
.isbn-queue .admin-text-button { display: flex; flex-direction: column; align-items: start; text-align: left; overflow-wrap: anywhere; }
.isbn-queue small { font-size: 11px; opacity: .7; }
.isbn-queue [aria-current] { color: var(--admin-accent, #c084fc); }
.isbn-results li { display: grid; gap: 4px; padding: 10px 0; overflow-wrap: anywhere; }
.isbn-results span { font-size: 12px; opacity: .7; }
@media (max-width: 540px) { .isbn-manual { align-items: stretch; flex-direction: column; } }
</style>
