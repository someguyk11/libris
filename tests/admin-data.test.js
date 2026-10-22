import test from 'node:test'
import assert from 'node:assert/strict'
import { circulationSeries, createAdminSeed, parseCsv, toCsv, validateBook } from '../src/views/admin/adminData.js'

test('CSV supports quoted commas, escaped quotes, BOM, and CRLF', () => {
  const rows = parseCsv('\uFEFFtitle,author,isbn\r\n"One, Two","An ""Author""",9780000000002\r\n')
  assert.deepEqual(rows, [{ title: 'One, Two', author: 'An "Author"', isbn: '9780000000002' }])
})
test('CSV rejects malformed imports before they reach the catalogue', () => {
  for (const csv of ['title,author\nBook,Author', 'title,author,isbn\n"Unclosed,Author,123', 'title,author,isbn\nBook,Author', 'title,title,author,isbn\nOne,Two,A,9780000000002']) assert.throws(() => parseCsv(csv))
})
test('CSV export round-trips titles and neutralises spreadsheet formulas', () => {
  const seed = createAdminSeed()
  const exported = parseCsv(toCsv(seed.books))
  assert.equal(exported.length, seed.books.length)
  assert.equal(exported[0].title, seed.books[0].title)
  assert.ok(toCsv([{ title: '=1+1' }]).includes("'=1+1"))
})
test('book validation allows editing its own ISBN but rejects duplicates and bad statuses', () => {
  const book = createAdminSeed().books[0]
  assert.equal(validateBook({ ...book, title: 'Updated title' }, [book], book.id).title, 'Updated title')
  assert.throws(() => validateBook(book, [book]), /already/)
  assert.throws(() => validateBook({ ...book, isbn: '123' }), /ISBN/)
  assert.throws(() => validateBook({ ...book, status: 'Missing' }), /status/)
})
test('chart aggregates transaction counts by date and excludes out-of-range history', () => {
  const now = new Date(2026, 9, 15, 12).getTime()
  const at = new Date(2026, 9, 15, 10).toISOString()
  const points = circulationSeries([{ type: 'loan', count: 3, at }, { type: 'loan', count: 1, at }, { type: 'return', count: 2, at }, { type: 'loan', count: 100, at: new Date(2025, 1, 1).toISOString() }], 7, now)
  assert.equal(points.length, 7)
  assert.equal(points.at(-1).loans, 4)
  assert.equal(points.at(-1).returns, 2)
  assert.equal(points.reduce((sum, point) => sum + point.loans, 0), 4)
})
