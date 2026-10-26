import { computed, reactive, ref, watch } from "vue"
import { createAdminSeed, validateBook, validateShelf } from "./adminData.js"
import { validateMember } from "./settingsData.js"
import { apiMode, backend } from '../../services/api.js'
import { bookFromApi, shelfFromApi, shelfPayload } from '../../services/backendData.js'
import { canonicalIsbn } from '../../utils/isbn.js'

const storageKey = "libris-admin-v1"
let saved
try { saved = JSON.parse(localStorage.getItem(storageKey)) } catch { /* Use the demo seed. */ }
const valid = saved?.version === 1 && [saved.books, saved.members, saved.shelves, saved.events].every(Array.isArray) && saved.books.every(book => book.id && book.title && book.isbn)
const state = reactive(valid ? saved : createAdminSeed())
// Live data never shares storage with the demo seed. Unimplemented areas start empty.
if (apiMode) Object.assign(state, { books: [], shelves: [], shelfDetails: {}, members: [], events: [] })
const loading = ref(false), apiError = ref(''), loaded = ref(false)
const hasMoreBooks = ref(false)
let booksOffset = 0
// Additive metadata keeps existing catalogue shelf-code references intact.
if (!state.shelfDetails || typeof state.shelfDetails !== "object" || Array.isArray(state.shelfDetails)) state.shelfDetails = {}
const storageError = ref(false)
watch(state, () => { if (apiMode) return; try { localStorage.setItem(storageKey, JSON.stringify(state)); storageError.value = false } catch { storageError.value = true } }, { deep: true })
const query = ref("")
const catalogueStatus = ref("")
const editorRequest = ref(false)
const activeBooks = computed(() => state.books.filter(book => !book.archived))
const loans = computed(() => activeBooks.value.filter(book => book.status === "Borrowed"))
const attention = computed(() => activeBooks.value.filter(book => book.status === "Maintenance" || book.status === "Borrowed" && book.due && new Date(book.due + "T23:59:59") < new Date()))
function record(type, title, detail = "", bookId = null) { state.events.push({ id: crypto.randomUUID(), type, title, detail, bookId, count: 1, at: new Date().toISOString() }) }
function saveBook(input) {
  if (apiMode) return saveBackendBook(input)
  const existing = state.books.find(book => book.id === input.id)
  const book = validateBook(input, state.books, existing?.id)
  if (!state.shelves.includes(book.shelf)) throw new Error("Choose a registered shelf.")
  if (existing?.archived && book.status === "Borrowed") throw new Error("Restore this title before assigning a loan.")
  if (book.status === "Borrowed" && (!book.due || !state.members.some(member => member.id === book.borrowerId))) throw new Error("Borrowed books need a due date and a member.")
  const previousStatus = existing?.status
  if (!existing || existing.isbn !== book.isbn) book.cover = `https://covers.openlibrary.org/b/isbn/${book.isbn}-M.jpg`
  if (book.status !== "Borrowed") { book.due = ""; book.borrowerId = "" }
  if (existing) Object.assign(existing, book)
  else { book.id = crypto.randomUUID(); book.added = new Date().toISOString(); book.archived = false; book.cover = `https://covers.openlibrary.org/b/isbn/${book.isbn}-M.jpg`; state.books.unshift(book) }
  record(existing ? "book-updated" : "book-added", existing ? "Book updated" : "Book added", book.title, book.id)
  if (book.status === "Borrowed" && previousStatus !== "Borrowed") record("loan", "Book borrowed", book.title, book.id)
  if (previousStatus === "Borrowed" && book.status !== "Borrowed") record("return", "Book returned", book.title, book.id)
  return state.books.find(item => item.id === book.id)
}
function importBooks(rows) {
  if (apiMode) throw new Error('Bulk import is available in demo mode only. The backend has no atomic bulk-import endpoint.')
  const checked = []
  for (const input of rows) {
    const fields = Object.fromEntries(["title", "author", "isbn", "genre", "shelf", "status", "language"].map(key => [key, input[key]]))
    const book = validateBook({ ...fields, shelf: fields.shelf || state.shelves[0] }, [...state.books, ...checked])
    if (!state.shelves.includes(book.shelf)) throw new Error(`Register shelf ${book.shelf} before importing.`)
    if (book.status === "Borrowed") throw new Error("Import books as Available, Reserved, or Maintenance; assign loans in the editor.")
    checked.push(book)
  }
  checked.forEach(saveBook)
  return checked.length
}
function archiveBook(book) {
  if (apiMode) throw new Error('The backend does not support archiving or restoring books.')
  if (!book.archived && book.status === "Borrowed") throw new Error("Return this book before archiving it.")
  book.archived = !book.archived
  record("archived", book.archived ? "Book archived" : "Book restored", book.title, book.id)
}
function markAvailable(book) { if (apiMode) throw new Error('The backend does not support loan returns or availability updates.'); return saveBook({ ...book, status: "Available" }) }
function addMember(name, email) {
  return saveMember({ name, email })
}
function saveMember(input, id = null) {
  if (apiMode) throw new Error('Member management is browser-demo only; no member API is implemented.')
  const fields = validateMember(input, state.members, id)
  const existing = id ? state.members.find(member => member.id === id) : null
  if (id && !existing) throw new Error("Member not found.")
  if (existing) Object.assign(existing, fields)
  else state.members.push({ id: crypto.randomUUID(), ...fields, joined: new Date().toISOString() })
  record("member", existing ? "Member updated locally" : "Member registered locally", fields.name)
}
function addShelf(name) {
  if (apiMode) throw new Error('Use Register Shelf to save a backend shelf.')
  const shelf = name.trim().toUpperCase()
  if (!/^[A-Z0-9-]{2,20}$/.test(shelf)) throw new Error("Use 2–20 letters, numbers, or hyphens for the shelf code.")
  if (state.shelves.includes(shelf)) throw new Error("That shelf is already registered.")
  state.shelves.push(shelf); state.shelves.sort(); record("shelf", "Shelf registered", shelf)
}
function registerShelf(input) {
  if (apiMode) return saveBackendShelf(input)
  const shelf = validateShelf(input, state.shelves)
  state.shelfDetails[shelf.code] = { ...shelf, createdAt: new Date().toISOString() }
  state.shelves.push(shelf.code)
  state.shelves.sort()
  record("shelf", "Shelf registered locally", `${shelf.name} · ${shelf.code}`)
  return shelf
}
async function refresh() {
  if (!apiMode || loading.value) return
  loading.value = true; apiError.value = ''
  try {
    const [shelves, books] = await Promise.all([backend.shelves(), backend.books('%')])
    const details = shelves.map(shelfFromApi)
    state.shelves = details.map(shelf => shelf.code)
    state.shelfDetails = Object.fromEntries(details.map(shelf => [shelf.code, shelf]))
    state.books = books.map(bookFromApi)
    booksOffset = books.length; hasMoreBooks.value = books.length === 50
    loaded.value = true
  } catch (error) { apiError.value = error.message }
  finally { loading.value = false }
}
async function loadMoreBooks() {
  if (!apiMode || loading.value || !hasMoreBooks.value) return
  loading.value = true; apiError.value = ''
  try {
    const books = await backend.books('%', { offset: booksOffset })
    const known = new Set(state.books.map(book => book.id))
    state.books.push(...books.map(bookFromApi).filter(book => !known.has(book.id)))
    booksOffset += books.length; hasMoreBooks.value = books.length === 50
  } catch (cause) { apiError.value = cause.message }
  finally { loading.value = false }
}
async function saveBackendBook(input) {
  const existing = state.books.find(book => book.id === input.id)
  const checked = validateBook({ ...input, isbn: canonicalIsbn(input.isbn), status: 'Available' }, state.books, existing?.id)
  if (!state.shelves.includes(checked.shelf)) throw new Error('Choose a registered shelf.')
  if (existing && ['title', 'author', 'isbn', 'publish_date'].some(key => (input[key] || '') !== (existing[key] || ''))) throw new Error('The backend can reassign existing ISBNs to shelves, but cannot edit their metadata.')
  const result = bookFromApi(await backend.registerBook({ isbn: checked.isbn, title: checked.title, author: checked.author, publish_date: input.publish_date || null, bibliography: input.bibliography || null, shelf_id: Number(checked.shelf), require_new: !existing, expected_existing_id: existing?.backendId || null }))
  const index = state.books.findIndex(book => book.id === result.id)
  if (index < 0) state.books.unshift(result)
  else state.books[index] = result
  return result
}
function acceptRegisteredBooks(books) {
  if (!apiMode) return
  for (const raw of books) {
    const result = bookFromApi(raw)
    const index = state.books.findIndex(book => book.id === result.id)
    if (index < 0) state.books.unshift(result)
    else state.books[index] = result
  }
}
async function saveBackendShelf(input, id = null) {
  const shelf = shelfFromApi(await backend.saveShelf(shelfPayload(input), id))
  state.shelfDetails[shelf.code] = shelf
  if (!state.shelves.includes(shelf.code)) state.shelves.push(shelf.code)
  return shelf
}
async function deleteBackendShelf(id) {
  await backend.deleteShelf(id)
  const code = String(id)
  state.shelves = state.shelves.filter(value => value !== code)
  delete state.shelfDetails[code]
}
export function useAdminStore() { return { state, query, catalogueStatus, editorRequest, storageError, activeBooks, loans, attention, saveBook, importBooks, archiveBook, markAvailable, addMember, saveMember, addShelf, registerShelf, apiMode, loading, loaded, apiError, refresh, hasMoreBooks, loadMoreBooks, saveBackendShelf, deleteBackendShelf, acceptRegisteredBooks } }
