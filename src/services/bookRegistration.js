import { canonicalIsbn } from '../utils/isbn.js'

export function registrationDetails(draft) {
  const result = { isbn: canonicalIsbn(draft.isbn) }
  for (const [field, label, max, required] of [['title', 'Title', 200, true], ['author', 'Author', 100, true], ['publish_date', 'Publication date', 100, false]]) {
    const value = String(draft[field] || '').trim()
    if (required && !value) throw new Error(`${label} is required. Enter it manually if Open Library has no information.`)
    if (value.length > max) throw new Error(`${label} must be ${max} characters or fewer. Review the imported value.`)
    result[field] = value || null
  }
  const bibliography = {}
  for (const [key, max] of [['publisher', 500], ['description', 20000], ['edition', 500], ['cover_url', 2000], ['isbn10', 64]]) {
    const value = String(draft.bibliography?.[key] || '').trim()
    if (value.length > max) throw new Error(`${key.replace('_', ' ')} is too long. Review the imported value.`)
    if (value) bibliography[key] = value
  }
  if (bibliography.isbn10 && (bibliography.isbn10.replace(/[\s-]/g, '').length !== 10 || canonicalIsbn(bibliography.isbn10) !== result.isbn)) throw new Error('ISBN-10 must identify the same edition as ISBN-13.')
  if (bibliography.cover_url) {
    let url
    try { url = new URL(bibliography.cover_url) } catch { throw new Error('Cover image must be a valid HTTPS URL.') }
    if (url.protocol !== 'https:' || !url.hostname || url.username || url.password) throw new Error('Cover image must be a valid HTTPS URL without credentials.')
  }
  result.bibliography = Object.keys(bibliography).length ? bibliography : null
  return result
}

export function reviewedBatch(entries, shelf) {
  if (!shelf || !Number.isInteger(Number(shelf))) throw new Error('Choose a registered shelf.')
  if (!entries.length || entries.length > 50) throw new Error('Scan between 1 and 50 books in this batch.')
  const seen = new Set()
  const books = entries.map(entry => {
    if (!entry.reviewed) throw new Error('Review and confirm every book before registering the batch.')
    const book = entry.existing ? { isbn: canonicalIsbn(entry.draft.isbn), existing_id: entry.existing.id } : registrationDetails(entry.draft)
    if (seen.has(book.isbn)) throw new Error('Two entries identify the same edition. Remove the duplicate before saving.')
    seen.add(book.isbn)
    return book
  })
  return { shelf_id: Number(shelf), books }
}
