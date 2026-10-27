import { computed, reactive } from 'vue'
import { guestBooks, guestAtlases } from '@/mock/data/guestGuide.js'
import { eligibleAtlas, createMockSession, advanceMockSession, resumeMockSession } from '@/services/guestGuideModel.js'

const initial = () => ({ query: '', layout: 'grid', bookId: null, atlasId: null, session: null })
let stored
try { stored = JSON.parse(sessionStorage.getItem('libris-guest-demo-v1')) } catch { /* In-memory mode remains available. */ }
const state = reactive(initial())
if (stored && typeof stored === 'object') {
  state.query = typeof stored.query === 'string' ? stored.query.slice(0, 200) : ''
  state.layout = stored.layout === 'list' ? 'list' : 'grid'
}
if (stored && guestBooks.some(book => book.id === stored.bookId)) {
  state.bookId = stored.bookId
  state.atlasId = guestAtlases.some(atlas => atlas.id === stored.atlasId) ? stored.atlasId : null
  const session = stored.session
  if (session?.bookId === state.bookId && session.atlasId === state.atlasId && /^demo-\d+$/.test(session.id) && ['moving', 'paused', 'arrived', 'complete'].includes(session.status) && Number.isInteger(session.stage) && session.stage >= 0 && session.stage <= 3) state.session = session
}
function persist() {
  try { sessionStorage.setItem('libris-guest-demo-v1', JSON.stringify(state)) } catch { /* Storage is optional. */ }
}
const book = computed(() => guestBooks.find(item => item.id === state.bookId))
const atlas = computed(() => guestAtlases.find(item => item.id === state.atlasId))
export function useGuestGuide() {
  return {
    state, book, atlas, books: guestBooks, atlases: guestAtlases, persist,
    selectBook(id) {
      if (!guestBooks.some(item => item.id === id)) return false
      if (state.bookId !== id) Object.assign(state, { bookId: id, atlasId: null, session: null })
      persist(); return true
    },
    selectAtlas(id) {
      if (!eligibleAtlas(guestAtlases.find(item => item.id === id))) return
      if (state.atlasId !== id) Object.assign(state, { atlasId: id, session: null })
      persist()
    },
    start() {
      if (!book.value?.available || !eligibleAtlas(atlas.value)) throw new Error('Select an available book and Atlas first.')
      const withObstacle = typeof location !== 'undefined' && new URLSearchParams(location.search).get('scenario') === 'obstacle'
      state.session = createMockSession(state.bookId, state.atlasId, Date.now(), withObstacle); persist()
    },
    tick() { state.session = advanceMockSession(state.session); persist() },
    resume() { state.session = resumeMockSession(state.session); persist() },
    end() { if (state.session) state.session = { ...state.session, status: 'complete' }; persist() },
    reset() { Object.assign(state, initial()); persist() }
  }
}
