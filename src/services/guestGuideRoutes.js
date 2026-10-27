// Uses the application's existing hash-router convention; direct URLs also parse.
export function parseGuideRoute(path) {
  if (path === '/guide') return { name: 'guide' }
  if (path === '/guide/setup') return { name: 'guide-setup' }
  // Old bookmarks return to setup; the retired verification screen never mounts.
  if (path === '/guide/verify') return { name: 'guide-setup' }
  const match = path.match(/^\/guide\/(book|session)\/([^/]+)$/)
  if (!match) return null
  try { return { name: `guide-${match[1]}`, id: decodeURIComponent(match[2]) } } catch { return null }
}
export function guideRoutePath(name, bookId, sessionId) {
  if (name === 'guide') return '/guide'
  if (name === 'guide-setup') return '/guide/setup'
  if (name === 'guide-book' && bookId) return `/guide/book/${encodeURIComponent(bookId)}`
  if (name === 'guide-session' && sessionId) return `/guide/session/${encodeURIComponent(sessionId)}`
  return null
}
