// UI-only simulation. No account, telemetry or movement authority.
export const STAGES = ['Leaving station', 'In transit', 'Near shelf', 'Arriving']
export function filterCatalogue(books, query) {
  const term = query.trim().toLocaleLowerCase()
  const isbn = term.replace(/[-\s]/g, '')
  return books.filter(book => `${book.title} ${book.author}`.toLocaleLowerCase().includes(term) || book.isbn.replace(/[-\s]/g, '').includes(isbn))
}
export function eligibleAtlas(atlas) { return Boolean(atlas?.available && atlas.battery >= 20) }
export function createMockSession(bookId, atlasId, now = Date.now(), withObstacle = false) {
  return { id: `demo-${now}`, bookId, atlasId, status: 'moving', stage: 0, nextAt: now + 5000, obstacleClearAt: null, assistanceShown: false, withObstacle }
}
export function advanceMockSession(session, now = Date.now()) {
  if (!session || session.status !== 'moving' || now < session.nextAt) return session
  if (session.withObstacle && session.stage === 1 && !session.assistanceShown) {
    return { ...session, status: 'paused', assistanceShown: true, obstacleClearAt: now + 6000 }
  }
  if (session.stage === 3) return { ...session, status: 'arrived' }
  return { ...session, stage: session.stage + 1, nextAt: now + 5000 }
}
export function resumeMockSession(session, now = Date.now()) {
  if (session.status !== 'paused' || now < session.obstacleClearAt) throw new Error('The simulated obstacle is still present. Atlas will wait until the path is clear.')
  return { ...session, status: 'moving', nextAt: now + 5000 }
}
