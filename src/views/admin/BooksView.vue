<template>
  <div class="admin-catalogue admin-page-enter">
    <header class="admin-page-heading admin-catalogue-hero"><div><p class="admin-eyebrow">BOOKS</p><h1>Library Catalogue</h1><p>Manage your collection, keep track of availability,<br class="admin-desktop-break"> and give every book a place.</p></div><div class="admin-heading-actions"><button v-if="apiMode" class="admin-button secondary" :disabled="loading" @click="registrationMode = 'bulk'"><span class="material-symbols-rounded">barcode_scanner</span>Scan to shelf</button><button class="admin-button" :disabled="loading" @click="openEditor()"><span class="material-symbols-rounded">add</span>{{ apiMode ? 'Add book / Scan ISBN' : 'Add book' }}</button></div></header>
    <p v-if="apiMode" class="admin-muted">Register ISBN editions to shelves using the database. Scan individually or review a batch for one shelf. Existing editions are not extra physical copies; shelf reassignment requires confirmation. Existing metadata, loans, availability, and archiving cannot be edited here.</p>
    <p v-if="apiMode" class="admin-muted" role="status">{{ state.books.length }} titles loaded. Filters, counts and export apply only to loaded titles. Refresh after catalogue changes.<button v-if="hasMoreBooks" class="admin-text-button" :disabled="loading" @click="loadMoreBooks">{{ loading ? 'Loading…' : 'Load next 50 titles' }}</button></p>
    <div class="admin-catalogue-body">
      <div class="admin-catalogue-main">
        <nav class="admin-book-tabs" aria-label="Catalogue status"><button :class="{ active: !archived && !selectedStatuses.length && !attentionOnly }" @click="setStatus('')">All books <small>{{ activeBooks.length }}</small></button><button v-for="status in statuses" :key="status" :class="{ active: !archived && selectedStatuses.length === 1 && selectedStatuses[0] === status }" @click="setStatus(status)">{{ status }}</button><button v-if="!apiMode" :class="{ active: archived }" @click="setArchived">Archived</button></nav>
        <div class="admin-catalogue-toolbar"><p aria-live="polite">{{ filtered.length }} {{ archived ? 'archived' : '' }} {{ filtered.length === 1 ? 'title' : 'titles' }}<span v-if="attentionOnly"> needing attention</span></p><div><button class="admin-icon-button admin-filter-toggle" :aria-expanded="filtersOpen" aria-label="Toggle catalogue filters" @click="filtersOpen = !filtersOpen"><span class="material-symbols-rounded">tune</span></button><button class="admin-text-button" :disabled="!filtered.length" @click="exportBooks"><span class="material-symbols-rounded">download</span>Export</button><label><span class="admin-sr-only">Sort catalogue</span><select v-model="sort"><option value="recent">Recently added</option><option value="title">Title A–Z</option><option value="author">Author A–Z</option><option value="shelf">Shelf</option></select></label></div></div>
        <p v-if="error" class="admin-notice admin-notice--error" role="alert">{{ error }}<button aria-label="Dismiss error" @click="error = ''">×</button></p>
        <div v-if="!apiMode && selected.size" class="admin-bulk-bar"><strong>{{ selected.size }} selected</strong><button v-if="!archived" @click="bulkAvailable">Mark available</button><button @click="bulkArchive">{{ archived ? 'Restore' : 'Archive' }}</button><button @click="selected = new Set()">Clear selection</button></div>
        <div class="admin-table-scroll admin-catalogue-table"><table><caption class="admin-sr-only">Library catalogue. Select a title to manage its shelf or details.</caption>
          <thead><tr><th v-if="!apiMode"><input type="checkbox" aria-label="Select all books on this page" :checked="pageBooks.length > 0 && pageBooks.every(book => selected.has(book.id))" :indeterminate="pageBooks.some(book => selected.has(book.id)) && !pageBooks.every(book => selected.has(book.id))" @change="selectPage($event.target.checked)" /></th><th><button @click="sort = 'title'">Title <span class="material-symbols-rounded">unfold_more</span></button></th><th><button @click="sort = 'author'">Author <span class="material-symbols-rounded">unfold_more</span></button></th><th>ISBN</th><th><button @click="sort = 'shelf'">Shelf <span class="material-symbols-rounded">unfold_more</span></button></th><th>Status</th><th><span class="admin-sr-only">Actions</span></th></tr></thead>
          <tbody>
            <tr v-for="book in pageBooks" :key="book.id" :class="{ 'is-selected': selected.has(book.id) }">
              <td v-if="!apiMode"><input type="checkbox" :aria-label="'Select ' + book.title" :checked="selected.has(book.id)" @change="toggleSelection(book.id)" /></td>
              <td><button class="admin-book-title" @click="openEditor(book)"><BookCover :cover="book.cover" :title="book.title" size="small" /><span><strong>{{ book.title }}</strong><small>{{ apiMode ? book.publish_date : book.genre }}</small></span></button></td>
              <td>{{ book.author }}</td><td class="admin-isbn">{{ book.isbn }}</td>
              <td><span>{{ apiMode ? state.shelfDetails[book.shelf]?.name || '#' + book.shelf : book.shelf }}</span><small class="admin-cell-secondary">{{ apiMode ? 'ID ' + book.shelf : book.language }}</small></td>
              <td><span class="admin-status" :class="'is-' + book.status.toLowerCase()">{{ book.status }}</span><small v-if="book.status === 'Borrowed' && isOverdue(book.due)" class="admin-cell-secondary admin-overdue">Overdue</small></td>
              <td><div class="admin-row-actions"><button class="admin-icon-button" :aria-label="(apiMode ? 'Reassign shelf for ' : 'Edit ') + book.title" :title="apiMode ? 'Reassign shelf' : 'Edit book'" @click="openEditor(book)"><span class="material-symbols-rounded">edit</span></button><button v-if="!apiMode" class="admin-icon-button" :aria-label="(book.archived ? 'Restore ' : 'Archive ') + book.title" :title="book.status === 'Borrowed' && !book.archived ? 'Return this book before archiving' : book.archived ? 'Restore book' : 'Archive book'" :disabled="book.status === 'Borrowed' && !book.archived" @click="archive(book)"><span class="material-symbols-rounded">{{ book.archived ? 'unarchive' : 'inventory_2' }}</span></button></div></td>
            </tr>
            <tr v-if="!pageBooks.length"><td :colspan="apiMode ? 6 : 7"><div class="admin-empty"><span class="material-symbols-rounded">search_off</span><h2>{{ loading ? 'Loading catalogue…' : archived ? 'No archived books match' : 'No books match your search' }}</h2><p>Try a different title, or clear your filters.</p><button class="admin-button secondary" @click="clearFilters">Clear filters</button></div></td></tr>
          </tbody>
        </table></div>
        <footer class="admin-pagination"><span>{{ filtered.length ? (page - 1) * pageSize + 1 : 0 }}–{{ Math.min(page * pageSize, filtered.length) }} of {{ filtered.length }}</span><div><button class="admin-icon-button" aria-label="Previous catalogue page" :disabled="page <= 1" @click="page--"><span class="material-symbols-rounded">chevron_left</span></button><span>Page {{ page }} of {{ pageCount }}</span><button class="admin-icon-button" aria-label="Next catalogue page" :disabled="page >= pageCount" @click="page++"><span class="material-symbols-rounded">chevron_right</span></button></div></footer>
      </div>
      <aside class="admin-panel admin-filters" :class="{ 'is-open': filtersOpen }" aria-label="Catalogue filters"><header><h2>Filters</h2><button class="admin-text-button" @click="clearFilters">Clear all</button></header><fieldset :disabled="archived"><legend>Status</legend><label v-for="status in statuses" :key="status"><input v-model="selectedStatuses" type="checkbox" :value="status" @change="attentionOnly = false" /><span>{{ status }}</span><small>{{ activeBooks.filter(book => book.status === status).length }}</small></label></fieldset><label class="admin-filter-field">Genre<select v-model="genre"><option value="">All genres</option><option v-for="value in genres" :key="value">{{ value }}</option></select></label><label class="admin-filter-field">Shelf<select v-model="shelf"><option value="">All shelves</option><option v-for="value in state.shelves" :key="value">{{ value }}</option></select></label><label v-if="languages.length > 1" class="admin-filter-field">Language<select v-model="language"><option value="">All languages</option><option v-for="value in languages" :key="value">{{ value }}</option></select></label><p class="admin-filter-note"><span class="material-symbols-rounded">info</span>Status tabs and filters stay in sync. Archived books are kept separately and can be restored.</p></aside>
    </div>

    <AdminDialog :open="editorOpen" :title="draft.id ? (apiMode ? 'Reassign shelf' : 'Edit book') : 'Add a book'" @close="editorOpen = false">
      <form class="admin-form" @submit.prevent="submitBook">
        <div class="admin-form-grid">
          <label class="admin-full-width">Title<input v-model="draft.title" :readonly="apiMode && Boolean(draft.id)" required maxlength="200" autofocus /></label>
          <label>Author<input v-model="draft.author" :readonly="apiMode && Boolean(draft.id)" required maxlength="100" /></label>
          <label>ISBN<input v-model="draft.isbn" :readonly="apiMode && Boolean(draft.id)" required placeholder="10 or 13 characters" /></label>
          <label v-if="!apiMode">Genre<input v-model="draft.genre" list="admin-genres" required maxlength="80" /><datalist id="admin-genres"><option v-for="value in genres" :key="value">{{ value }}</option></datalist></label>
          <label>Shelf<select v-model="draft.shelf" required><option value="" disabled>Select a shelf</option><option v-for="value in state.shelves" :key="value" :value="value">{{ state.shelfDetails[value]?.name || value }}{{ apiMode ? ' · #' + value : '' }}</option></select></label>
          <label v-if="apiMode">Publication date <span class="field-optional">Optional</span><input v-model="draft.publish_date" :readonly="Boolean(draft.id)" maxlength="100" placeholder="e.g. 2021" /></label>
          <template v-else><label>Status<select v-model="draft.status"><option v-for="status in statuses" :key="status">{{ status }}</option></select></label><label>Language<input v-model="draft.language" required maxlength="50" /></label><template v-if="draft.status === 'Borrowed'"><label>Borrowed by<select v-model="draft.borrowerId" required><option value="" disabled>Select a member</option><option v-for="member in state.members" :key="member.id" :value="member.id">{{ member.name }}</option></select></label><label>Due date<input v-model="draft.due" type="date" required /></label></template></template>
        </div>
        <p v-if="apiMode && !state.shelves.length" class="admin-muted">Register a shelf before adding a book. <a href="#/admin/shelves">Register Shelf</a></p>
        <p v-if="formError" class="admin-form-error" role="alert">{{ formError }}</p>
        <div class="admin-form-actions"><AppButton type="button" variant="secondary" :disabled="saving" @click="editorOpen = false">Cancel</AppButton><AppButton type="submit" :disabled="saving || (apiMode && !draft.shelf)">{{ saving ? 'Saving…' : draft.id ? 'Save changes' : 'Add book' }}</AppButton></div>
      </form>
    </AdminDialog>
    <BookRegistration v-if="registrationMode" :bulk="registrationMode === 'bulk'" @close="registrationMode = ''" @saved="emit('notice', $event)" />
  </div>
</template>
<script setup>
import { computed, ref, watch } from 'vue'
import BookCover from '@/components/book/BookCover.vue'
import AppButton from '@/components/common/AppButton.vue'
import AdminDialog from './components/AdminDialog.vue'
import BookRegistration from './components/BookRegistration.vue'
import { useAdminStore } from './useAdminStore'
import { statuses as demoStatuses, toCsv } from './adminData'
const emit = defineEmits(['notice'])
const { state, activeBooks, query, catalogueStatus, editorRequest, saveBook, archiveBook, markAvailable, apiMode, loading, hasMoreBooks, loadMoreBooks } = useAdminStore()
const registrationMode = ref('')
const statuses = apiMode ? ['Available', 'Unavailable'] : demoStatuses
const selectedStatuses = ref([]), genre = ref(''), shelf = ref(''), language = ref(''), sort = ref('recent'), archived = ref(false), attentionOnly = ref(false), filtersOpen = ref(false), page = ref(1), selected = ref(new Set()), error = ref('')
const pageSize = 8
const genres = computed(() => [...new Set(state.books.map(book => book.genre))].sort())
const languages = computed(() => [...new Set(state.books.map(book => book.language))].sort())
const isOverdue = due => due && new Date(due + 'T23:59:59') < new Date()
const filtered = computed(() => {
  const search = query.value.trim().toLowerCase()
  return state.books.filter(book => Boolean(book.archived) === archived.value && (!search || `${book.title} ${book.author} ${book.isbn} ${book.genre}`.toLowerCase().includes(search)) && (!selectedStatuses.value.length || selectedStatuses.value.includes(book.status)) && (!genre.value || book.genre === genre.value) && (!shelf.value || book.shelf === shelf.value) && (!language.value || book.language === language.value) && (!attentionOnly.value || book.status === 'Maintenance' || book.status === 'Borrowed' && isOverdue(book.due))).sort((a, b) => sort.value === 'recent' ? (apiMode ? b.backendId - a.backendId : new Date(b.added) - new Date(a.added)) : a[sort.value].localeCompare(b[sort.value]))
})
const pageCount = computed(() => Math.max(1, Math.ceil(filtered.value.length / pageSize)))
const pageBooks = computed(() => filtered.value.slice((page.value - 1) * pageSize, page.value * pageSize))
watch([query, selectedStatuses, genre, shelf, language, sort, archived, attentionOnly], () => { page.value = 1; selected.value = new Set() }, { deep: true })
watch(pageCount, count => { page.value = Math.min(page.value, count) })
function setStatus(status) { archived.value = false; attentionOnly.value = status === 'attention'; selectedStatuses.value = statuses.includes(status) ? [status] : [] }
watch(catalogueStatus, setStatus, { immediate: true })
function setArchived() { archived.value = true; selectedStatuses.value = []; attentionOnly.value = false }
function clearFilters() { query.value = ''; genre.value = ''; shelf.value = ''; language.value = ''; setStatus(''); error.value = '' }
function toggleSelection(id) { const next = new Set(selected.value); next.has(id) ? next.delete(id) : next.add(id); selected.value = next }
function selectPage(checked) { const next = new Set(selected.value); pageBooks.value.forEach(book => checked ? next.add(book.id) : next.delete(book.id)); selected.value = next }
function archive(book) { try { archiveBook(book); emit('notice', book.archived ? `${book.title} archived. Find it in Archived to restore.` : `${book.title} restored`); selected.value = new Set(); error.value = '' } catch (reason) { error.value = reason.message } }
function bulkAvailable() { if (apiMode) { error.value = 'Availability updates are not supported by the backend.'; return } const books = state.books.filter(book => selected.value.has(book.id)); books.forEach(markAvailable); selected.value = new Set(); emit('notice', `${books.length} books marked available`) }
function bulkArchive() { if (apiMode) { error.value = 'Archiving is not supported by the backend.'; return } const books = state.books.filter(book => selected.value.has(book.id)); if (books.some(book => !book.archived && book.status === 'Borrowed')) { error.value = 'Return borrowed books before archiving them.'; return } books.forEach(archiveBook); selected.value = new Set(); emit('notice', archived.value ? 'Selected books restored' : 'Selected books archived') }
const editorOpen = ref(false), draft = ref({}), formError = ref(''), saving = ref(false)
function openEditor(book) { if (apiMode && !book) { registrationMode.value = 'single'; return } draft.value = book ? { ...book } : { title: '', author: '', isbn: '', genre: 'Fiction', shelf: state.shelves[0] || '', status: 'Available', language: 'English', due: '', borrowerId: '', publish_date: '' }; formError.value = ''; editorOpen.value = true }
watch(editorRequest, value => { if (value) { openEditor(); editorRequest.value = false } }, { immediate: true })
async function submitBook() { if (saving.value) return; saving.value = true; try { const editing = Boolean(draft.value.id); await saveBook(draft.value); editorOpen.value = false; emit('notice', editing ? (apiMode ? 'Shelf assignment saved to the database' : 'Book details updated') : 'Book added to the catalogue') } catch (reason) { formError.value = reason.message } finally { saving.value = false } }
function download(text, name) { const url = URL.createObjectURL(new Blob([text], { type: 'text/csv;charset=utf-8' })); const link = document.createElement('a'); link.href = url; link.download = name; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000) }
function exportBooks() { download(toCsv(filtered.value), 'libris-catalogue.csv'); emit('notice', `${filtered.value.length} titles exported`) }
</script>
