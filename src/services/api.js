// No credentials or database URLs belong in VITE_* variables.
export const apiMode = import.meta.env?.VITE_DATA_MODE === 'api'
const baseUrl = (import.meta.env?.VITE_API_BASE_URL || '/api').replace(/\/$/, '')
let csrfToken = null

export async function request(path, options = {}) {
  if (!csrfToken && !path.startsWith('/auth/') && !['GET', 'HEAD'].includes(options.method || 'GET')) {
    // Best-effort bootstrap for a restored HttpOnly session. Never retry a
    // mutation (especially robot commands) after dispatch.
    try { await sessionRequest('/auth/session') } catch { /* Server authorizes the actual request. */ }
  }
  const { timeout = path.startsWith('/auth/') ? 45000 : 15000, signal, ...fetchOptions } = options
  const controller = new AbortController()
  const abort = () => controller.abort()
  if (signal?.aborted) abort()
  signal?.addEventListener('abort', abort, { once: true })
  const timer = setTimeout(abort, timeout)
  try {
    const response = await fetch(baseUrl + path, {
      ...fetchOptions, credentials: 'include', signal: controller.signal,
      headers: { ...(options.body ? { 'Content-Type': 'application/json' } : {}), ...(csrfToken ? { 'X-CSRF-Token': csrfToken } : {}), ...options.headers },
    })
    const text = await response.text()
    let data = null
    try { data = text ? JSON.parse(text) : null } catch {
      if (response.ok) throw new Error('Backend returned an invalid JSON response.')
    }
    if (!response.ok) {
      if (response.status === 401) {
        csrfToken = null
        if (!path.startsWith('/auth/') && typeof window !== 'undefined') window.dispatchEvent(new Event('libris:session-expired'))
      }
      const detail = Array.isArray(data?.detail) ? data.detail.map(item => `${item.loc?.slice(1).join('.')}: ${item.msg}`).join('; ') : data?.detail
      const error = new Error(typeof detail === 'string' ? detail : `Backend request failed (${response.status}).`)
      error.status = response.status
      throw error
    }
    if (response.status !== 204 && (data === null || typeof data !== 'object')) throw new Error('Backend returned an empty or invalid response.')
    return data
  } catch (error) {
    if (error.name === 'AbortError') {
      if (signal?.aborted) throw error
      throw new Error('Backend request timed out. Check the connection before retrying.')
    }
    if (error instanceof TypeError) throw new Error('Backend unavailable. Start the Python server and check the API URL.')
    throw error
  } finally { clearTimeout(timer); signal?.removeEventListener('abort', abort) }
}
const json = (method, body) => ({ method, body: JSON.stringify(body) })
const robotPath = id => `/robot/${encodeURIComponent(id)}`
async function sessionRequest(path, options) {
  const session = await request(path, options)
  if (typeof session?.authenticated !== 'boolean') throw new Error('Backend returned an invalid session response.')
  csrfToken = session.csrf_token || null
  return session
}
export const authApi = {
  session: () => sessionRequest('/auth/session'),
  login: (email, password) => sessionRequest('/auth/login', json('POST', { email, password })),
  signup: (email, password) => request('/auth/signup', json('POST', { email, password })),
  verify: (email, token) => sessionRequest('/auth/otp/verify', json('POST', { email, token })),
  resend: email => request('/auth/otp/resend', json('POST', { email })),
  forgotPassword: email => request('/auth/password/forgot', json('POST', { email })),
  verifyRecovery: (email, token) => sessionRequest('/auth/password/verify', json('POST', { email, token })),
  resetPassword: async password => { const result = await request('/auth/password/reset', json('POST', { password })); csrfToken = null; return result },
  logout: async () => { const result = await request('/auth/logout', { method: 'POST' }); csrfToken = null; return result },
}
export const backend = {
  requestGuidance: bookId => request('/guidance-requests', json('POST', { book_id: bookId })),
  verifyGuidance: (id, atlasOtp) => request(`/guidance-requests/${encodeURIComponent(id)}/verify`, json('POST', { atlas_otp: atlasOtp })),
  configureHome: (id, checkpointId) => request(`/robots/${encodeURIComponent(id)}/home`, json('PATCH', { home_checkpoint_id: checkpointId })),
  myGuidance: () => request('/guidance-requests'),
  guidance: id => request(`/guidance-requests/${encodeURIComponent(id)}`),
  cancelGuidance: id => request(`/guidance-requests/${encodeURIComponent(id)}/cancel`, { method: 'POST' }),
  robotGuidance: id => request(`/robots/${encodeURIComponent(id)}/guidance-tasks`),
  stopGuidance: id => request(`/robots/${encodeURIComponent(id)}/guidance-stop`, { method: 'POST' }),
  confirmPosition: (id, checkpointId, taskId = null) => request(`/robots/${encodeURIComponent(id)}/checkpoint-confirmation`, json('POST', { checkpoint_id: checkpointId, operator_confirmed: true, task_id: taskId })),
  mapShelf: (id, checkpointId) => request(`/shelves/${id}/destination`, json('PATCH', { destination_checkpoint_id: checkpointId || null })),
  books: (query, { limit = 50, offset = 0, signal } = {}) => request(`/book/search/${encodeURIComponent(query || '%')}?limit=${limit}&offset=${offset}`, { signal }),
  book: id => request(`/book/${encodeURIComponent(id)}`),
  registerBook: book => request('/book/register', json('POST', book)),
  lookupIsbn: isbn => request(`/book/metadata/${encodeURIComponent(isbn)}`),
  registerBulk: batch => request('/book/register-bulk', json('POST', batch)),
  shelves: () => request('/shelves/'),
  saveShelf: (shelf, id) => request(id == null ? '/shelves/' : `/shelves/${id}`, json(id == null ? 'POST' : 'PUT', shelf)),
  deleteShelf: id => request(`/shelves/${id}`, { method: 'DELETE' }),
  robots: () => request('/robots/'),
  checkpoints: () => request('/checkpoints/'),
  createCheckpoint: checkpoint => request('/checkpoints/', json('POST', checkpoint)),
  updateCheckpoint: (id, checkpoint) => request(`/checkpoints/${encodeURIComponent(id)}`, json('PUT', checkpoint)),
  deactivateCheckpoint: id => request(`/checkpoints/${encodeURIComponent(id)}`, { method: 'DELETE' }),
  registerRobot: robot => request('/robots/', json('POST', robot)),
  updateRobot: (id, name) => request(`/robots/${encodeURIComponent(id)}`, json('PATCH', { name })),
  routes: robotId => request(`/robots/${encodeURIComponent(robotId)}/routes`),
  createRoute: (robotId, route) => request(`/robots/${encodeURIComponent(robotId)}/routes`, json('POST', route)),
  updateRoute: (id, route) => request(`/routes/${encodeURIComponent(id)}`, json('PUT', route)),
  renameRoute: (id, name) => request(`/routes/${encodeURIComponent(id)}`, json('PATCH', { name })),
  deleteRoute: id => request(`/routes/${encodeURIComponent(id)}`, { method: 'DELETE' }),
  robotStatus: id => request(`${robotPath(id)}/status`),
  command: (id, command, metadata = {}) => request(`${robotPath(id)}/command`, json('POST', { command, ...metadata })),
  startRecording: (robotId, startCheckpointId) => request(`${robotPath(robotId)}/recordings`, json('POST', { start_checkpoint_id: startCheckpointId })),
  recording: id => request(`/recordings/${encodeURIComponent(id)}`),
  confirmCheckpoint: (id, confirmation) => request(`/recordings/${encodeURIComponent(id)}/checkpoints`, json('POST', confirmation)),
  finalizeRecording: id => request(`/recordings/${encodeURIComponent(id)}/finalize`, { method: 'POST' }),
  saveRecording: (id, metadata) => request(`/recordings/${encodeURIComponent(id)}/save`, json('POST', metadata)),
  discardRecording: id => request(`/recordings/${encodeURIComponent(id)}`, { method: 'DELETE' }),
  reconfigure: (id, config) => request(`${robotPath(id)}/reconfigure`, json('POST', config)),
  reset: id => request(`${robotPath(id)}/reset`, { method: 'POST' }),
  recoverRobot: (id, body) => request(`${robotPath(id)}/recovery`, json('POST', body)),
}
