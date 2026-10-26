// Adapt actual LiBot fields without adding unsupported database metadata.
export function shelfFromApi(shelf) {
  return { id: shelf.shelf_id, code: String(shelf.shelf_id), name: shelf.shelf_name, section: shelf.section || '', floor: shelf.floor, path_id: shelf.path_id, description: shelf.description || '', destinationCheckpointId: shelf.destination_checkpoint_id || null }
}
export function bookFromApi(book) {
  return { id: String(book.id), backendId: book.id, title: book.title, author: book.author, isbn: book.isbn,
    shelf: String(book.shelf_id), status: book.available ? 'Available' : 'Unavailable',
    publish_date: book.publish_date || '', published: book.publish_date || '', description: book.bibliography?.description || '', publisher: book.bibliography?.publisher || '', bibliography: book.bibliography || null, genre: '', language: '', archived: false, due: '', borrowerId: '', added: '',
    cover: book.bibliography?.cover_url || `https://covers.openlibrary.org/b/isbn/${book.isbn}-M.jpg` }
}
export function shelfPayload(input) {
  const shelf_name = String(input.name || '').trim()
  if (!shelf_name || shelf_name.length > 100) throw new Error('Enter a shelf name of up to 100 characters.')
  if (!Number.isInteger(Number(input.floor))) throw new Error('Floor must be a whole number.')
  return { shelf_name, section: input.section?.trim() || null, floor: Number(input.floor), path_id: Number(input.path_id || 0), description: input.description?.trim() || null }
}
export function robotFromApi(robot) {
  return { id: robot.id, name: robot.name, identifier: robot.id, snapshot: null, telemetry: null, connectionError: '' }
}
const utcDate = value => value && !/(?:Z|[+-]\d{2}:\d{2})$/.test(value) ? value + 'Z' : value
export function routeFromApi(route) {
  return { id: route.id, atlasId: route.robot_id, name: route.name, startCheckpointId: route.start_checkpoint_id, destination: route.destination_checkpoint_id, status: route.status, recordingVersion: route.recording_version, simulation: false, createdAt: utcDate(route.created_at), updatedAt: utcDate(route.updated_at), steps: route.route_segments }
}
