import test from 'node:test'
import assert from 'node:assert/strict'
import { existsSync, readFileSync } from 'node:fs'
import { guestBooks, guestAtlases } from '../src/mock/data/guestGuide.js'
import { filterCatalogue, eligibleAtlas, createMockSession, advanceMockSession, resumeMockSession } from '../src/services/guestGuideModel.js'
import { parseGuideRoute, guideRoutePath } from '../src/services/guestGuideRoutes.js'

test('guest catalogue filters title, author and normalized ISBN from one source', () => {
  assert.equal(filterCatalogue(guestBooks, '').length, 6)
  assert.equal(filterCatalogue(guestBooks, '  ALCHEMIST  ')[0].id, 'the-alchemist')
  assert.equal(filterCatalogue(guestBooks, 'james clear')[0].title, 'Atomic Habits')
  assert.equal(filterCatalogue(guestBooks, '978-0441172719')[0].title, 'Dune')
  assert.equal(filterCatalogue(guestBooks, 'no such book').length, 0)
  assert.equal(new Set(guestBooks.map(book => book.id)).size, guestBooks.length)
  for (const book of guestBooks) {
    assert.ok(book.author && book.isbn && book.description && book.location)
    assert.ok(existsSync(new URL(`../public${book.cover}`, import.meta.url)), book.cover)
  }
  assert.ok(existsSync(new URL('../public/guide-assets/nebula.jpg', import.meta.url)))
})

test('only available Atlas units with sufficient battery are eligible', () => {
  assert.deepEqual(guestAtlases.filter(eligibleAtlas).map(unit => unit.id), ['Atlas-01', 'Atlas-02'])
  assert.equal(eligibleAtlas({ available: true, battery: 12 }), false)
  assert.equal(eligibleAtlas(null), false)
})

test('Atlas selection starts the next screen directly, with no code step', () => {
  const shell = readFileSync(new URL('../src/views/guide/GuestGuideView.vue', import.meta.url), 'utf8')
  assert.match(shell, /@continue="startGuidance"/)
  assert.match(shell, /guide\.start\(\)/)
  assert.match(shell, /openGuideSession/)
  assert.doesNotMatch(shell, /AtlasOtp|guide-verify|requestOtp/)
  assert.equal(existsSync(new URL('../src/views/guide/AtlasOtpPage.vue', import.meta.url)), false)
})

test('primary simulated journey reaches arrival without forcing assistance', () => {
  let session = createMockSession('the-alchemist', 'Atlas-01', 0)
  assert.equal(session.stage, 0)
  assert.equal(advanceMockSession(session, 4999), session)
  for (let now = 5000; now <= 20000; now += 5000) session = advanceMockSession(session, now)
  assert.equal(session.status, 'arrived')
  assert.equal(session.stage, 3)
  assert.equal(session.assistanceShown, false)
  assert.equal(advanceMockSession(session, 99999), session)
})

test('obstacle scenario pauses once and cannot resume before the path is clear', () => {
  let session = createMockSession('dune', 'Atlas-02', 0, true)
  session = advanceMockSession(session, 5000)
  session = advanceMockSession(session, 10000)
  assert.equal(session.status, 'paused')
  assert.equal(session.obstacleClearAt, 16000)
  assert.equal(advanceMockSession(session, 50000), session)
  assert.throws(() => resumeMockSession(session, 15999), /still present/)
  session = resumeMockSession(session, 16000)
  assert.equal(session.status, 'moving')
  session = advanceMockSession(session, 21000)
  assert.equal(session.stage, 2)
  session = advanceMockSession(session, 26000)
  session = advanceMockSession(session, 31000)
  assert.equal(session.status, 'arrived')
})

test('guest routes round-trip IDs and reject unknown or malformed steps', () => {
  for (const name of ['guide', 'guide-setup']) assert.equal(parseGuideRoute(guideRoutePath(name)).name, name)
  assert.equal(parseGuideRoute('/guide/verify').name, 'guide-setup')
  for (const name of ['guide-book', 'guide-session']) {
    const path = guideRoutePath(name, 'book/id', 'demo-123')
    assert.equal(parseGuideRoute(path).name, name)
    assert.equal(parseGuideRoute(path).id, name === 'guide-book' ? 'book/id' : 'demo-123')
  }
  assert.equal(parseGuideRoute('/guide/unknown'), null)
  assert.equal(parseGuideRoute('/guide/book/%zz'), null)
  assert.equal(parseGuideRoute('/guide/session/'), null)
})

test('guide UI is explicitly simulated and does not call authenticated guidance or auth APIs', () => {
  const shell = readFileSync(new URL('../src/views/guide/GuestGuideView.vue', import.meta.url), 'utf8')
  assert.match(shell, /Simulated catalogue/)
  assert.doesNotMatch(shell, /BottomNav|notification/)
  const state = readFileSync(new URL('../src/composables/useGuestGuide.js', import.meta.url), 'utf8')
  assert.doesNotMatch(state, /authApi|backend\.|fetch\(/)
})

test('auth entry and guest guide remain separate without replacing Explore', () => {
  const nav = readFileSync(new URL('../src/composables/useNav.js', import.meta.url), 'utf8')
  const app = readFileSync(new URL('../src/App.vue', import.meta.url), 'utf8')
  const auth = readFileSync(new URL('../src/views/AuthView.vue', import.meta.url), 'utf8')
  const hosting = readFileSync(new URL('../netlify.toml', import.meta.url), 'utf8')
  assert.match(nav, /routeNames\[path\(\)\] \|\| 'auth'/)
  assert.match(app, /<GuestGuideView/)
  assert.doesNotMatch(app, /apiMode && current === 'explore'/)
  assert.doesNotMatch(auth, /Back to Libris/)
  assert.match(auth, /<a class="auth-guest-link" href="#\/guide">Continue as guest — Book guidance<\/a>/)
  assert.match(hosting, /from = "\/guide\/\*"/)
  assert.match(hosting, /from = "\/api\/\*"/)
})
