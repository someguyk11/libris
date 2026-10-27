import { ref, watch } from 'vue'
import { parseGuideRoute, guideRoutePath } from '../services/guestGuideRoutes.js'

const routeNames = { '/auth': 'auth', '/admin': 'admin-dashboard', '/admin/books': 'admin-books', '/admin/shelves': 'admin-shelves', '/admin/atlas': 'admin-atlas', '/admin/settings': 'admin-settings', '/rooms': 'rooms', '/room': 'room', '/explore': 'explore', '/discover': 'discover', '/library': 'library', '/profile': 'profile' }
function idFromHash(prefix) {
  const hash = typeof location === 'undefined' ? '' : location.hash.slice(1)
  const match = hash.match(new RegExp(`^${prefix}/([^/]+)$`))
  try { return match ? decodeURIComponent(match[1]) : null } catch { return null }
}
const path = () => typeof location === 'undefined' ? '' : location.hash.slice(1) || location.pathname.replace(/\/$/, '')
const guestRoute = () => parseGuideRoute(path())
const fromHash = () => guestRoute()?.name || (idFromHash('/admin/atlas') ? 'admin-atlas-detail' : idFromHash('/book') ? 'book' : routeNames[path()] || 'auth')
const current = ref(fromHash()), selectedAtlasId = ref(idFromHash('/admin/atlas')), selectedBookId = ref(idFromHash('/book'))
const selectedGuideBookId = ref(guestRoute()?.name === 'guide-book' ? guestRoute().id : null)
const selectedGuideSessionId = ref(guestRoute()?.name === 'guide-session' ? guestRoute().id : null)
const authMode = ref('login')
const selectedBook = ref(null), authReturnTo = ref(null)
let savedRoom = null
try { const value = JSON.parse(sessionStorage.getItem('libris-active-room')); if (value?.id && value.title) savedRoom = value } catch { /* No stored room. */ }
const selectedRoom = ref(savedRoom), returnTo = ref('explore'), roomReturnTo = ref('rooms')

if (typeof window !== 'undefined') {
  window.addEventListener('hashchange', () => {
    selectedAtlasId.value = idFromHash('/admin/atlas')
    const bookId = idFromHash('/book')
    if (bookId !== selectedBookId.value) selectedBook.value = null
    selectedBookId.value = bookId
    const guide = guestRoute()
    selectedGuideBookId.value = guide?.name === 'guide-book' ? guide.id : null
    selectedGuideSessionId.value = guide?.name === 'guide-session' ? guide.id : null
    current.value = fromHash()
  })
  watch([current, selectedAtlasId, selectedBookId, selectedGuideBookId, selectedGuideSessionId], ([value, atlasId, bookId, guideBookId, guideSessionId]) => {
    const route = guideRoutePath(value, guideBookId, guideSessionId) || (value === 'admin-atlas-detail' && atlasId ? `/admin/atlas/${encodeURIComponent(atlasId)}` : value === 'book' && bookId ? `/book/${encodeURIComponent(bookId)}` : Object.keys(routeNames).find(key => routeNames[key] === value))
    if (route && location.hash !== `#${route}`) history.pushState(null, '', `#${route}`)
    window.scrollTo({ top: 0, behavior: 'instant' })
  })
  if (!location.hash) {
    const route = guideRoutePath(current.value, selectedGuideBookId.value, selectedGuideSessionId.value) || Object.keys(routeNames).find(key => routeNames[key] === current.value) || '/auth'
    history.replaceState(null, '', `#${route}`)
  }
}

export function useNav() {
  return {
    current, selectedAtlasId, selectedBookId, selectedBook, selectedRoom, authMode, selectedGuideBookId, selectedGuideSessionId,
    openGuideBook(id) { selectedGuideBookId.value = id; current.value = 'guide-book' },
    openGuideSession(id) { selectedGuideSessionId.value = id; current.value = 'guide-session' },
    openAtlas(id) { selectedAtlasId.value = id; current.value = 'admin-atlas-detail' },
    requireSignIn() {
      if (current.value !== 'auth') authReturnTo.value = { route: current.value, bookId: selectedBookId.value, book: selectedBook.value, atlasId: selectedAtlasId.value }
      current.value = 'auth'
    },
    afterSignIn() {
      const target = authReturnTo.value
      if (target) { selectedBookId.value = target.bookId; selectedBook.value = target.book; selectedAtlasId.value = target.atlasId }
      current.value = target?.route || 'discover'; authReturnTo.value = null
    },
    enterRoom(room) {
      roomReturnTo.value = current.value === 'room' ? 'rooms' : current.value
      selectedRoom.value = room
      try { sessionStorage.setItem('libris-active-room', JSON.stringify(room)) } catch { /* Available for this visit. */ }
      current.value = 'room'
    },
    leaveRoom() { current.value = roomReturnTo.value },
    set(name) { current.value = name },
    openBook(book, source = current.value) {
      selectedBook.value = book; selectedBookId.value = String(book.backendId ?? book.id)
      returnTo.value = source === 'book' ? 'explore' : source; current.value = 'book'
    },
    closeBook() { current.value = returnTo.value || 'explore' }
  }
}
