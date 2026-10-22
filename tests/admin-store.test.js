import test from 'node:test'
import assert from 'node:assert/strict'
import { useAdminStore } from '../src/views/admin/useAdminStore.js'

test('catalogue edits, loans, returns, and archive restoration share one consistent store', () => {
  const store = useAdminStore()
  const initial = store.activeBooks.value.length
  const book = store.saveBook({ title: 'Test title', author: 'Test author', isbn: '9780000000002', shelf: store.state.shelves[0], status: 'Available' })
  assert.equal(store.activeBooks.value.length, initial + 1)
  store.saveBook({ ...book, title: 'Edited title' })
  assert.equal(store.activeBooks.value.length, initial + 1)
  const loansBefore = store.loans.value.length
  store.saveBook({ ...book, status: 'Borrowed', borrowerId: store.state.members[0].id, due: '2026-12-31' })
  assert.equal(store.loans.value.length, loansBefore + 1)
  assert.throws(() => store.archiveBook(book), /Return/)
  store.markAvailable(book)
  assert.equal(store.loans.value.length, loansBefore)
  assert.equal(book.borrowerId, '')
  assert.equal(book.due, '')
  store.archiveBook(book)
  assert.equal(store.activeBooks.value.length, initial)
  assert.throws(() => store.saveBook({ ...book, status: 'Borrowed', borrowerId: store.state.members[0].id, due: '2030-01-15' }), /Restore/)
  store.archiveBook(book)
  assert.equal(store.activeBooks.value.length, initial + 1)
  assert.ok(store.state.events.some(event => event.type === 'return' && event.bookId === book.id))
})
test('an invalid import leaves the catalogue unchanged', () => {
  const store = useAdminStore()
  const before = store.state.books.length
  assert.throws(() => store.importBooks([{ title: 'Valid', author: 'Author', isbn: '9780000000003' }, { title: 'Invalid', author: 'Author', isbn: 'bad' }]))
  assert.equal(store.state.books.length, before)
  assert.throws(() => store.importBooks([{ title: 'Duplicate', author: 'Author', isbn: store.state.books[0].isbn }]))
  assert.equal(store.state.books.length, before)
})

test('CSV metadata cannot overwrite an existing internal record ID', () => {
  const store = useAdminStore()
  const existing = store.state.books[0]
  const title = existing.title
  const before = store.state.books.length
  store.importBooks([{ id: existing.id, title: 'Imported separately', author: 'Author', isbn: '9780000000004' }])
  assert.equal(store.state.books.length, before + 1)
  assert.equal(existing.title, title)
  assert.notEqual(store.state.books[0].id, existing.id)
})

test('rich shelf registration preserves catalogue codes and rejects duplicates atomically', () => {
  const store = useAdminStore()
  const previousBooks = store.state.books.map(book => [book.id, book.shelf])
  const input = { code: ' sh-test ', name: ' Fiction Shelf ', category: 'Books', section: 'Ground floor', description: 'Local test', aisle: '3', row: 'B' }
  const shelf = store.registerShelf(input)
  assert.equal(shelf.code, 'SH-TEST')
  assert.ok(store.state.shelves.includes('SH-TEST'))
  assert.equal(store.state.shelfDetails['SH-TEST'].name, 'Fiction Shelf')
  assert.deepEqual(store.state.books.map(book => [book.id, book.shelf]), previousBooks)
  const count = store.state.shelves.length
  assert.throws(() => store.registerShelf({ ...input, name: 'Different name' }), /already registered/)
  assert.equal(store.state.shelves.length, count)
  assert.equal(store.state.shelfDetails['SH-TEST'].name, 'Fiction Shelf')
  assert.throws(() => store.registerShelf({ ...input, code: 'SH-INVALID', section: '' }), /section/)
  assert.equal(store.state.shelfDetails['SH-INVALID'], undefined)
  const book = store.saveBook({ title: 'Shelf compatibility', author: 'Author', isbn: '9780000000005', shelf: shelf.code, status: 'Available' })
  assert.equal(book.shelf, 'SH-TEST')
})

test('member edits preserve loan references, internal identity, and original join date', () => {
  const store = useAdminStore()
  const loan = store.state.books.find(book => book.status === 'Borrowed')
  const member = store.state.members.find(item => item.id === loan.borrowerId)
  const originalId = member.id, joined = member.joined
  store.saveMember({ name: 'Updated member', email: member.email, memberNumber: 'STAFF-001', category: 'Staff', id: 'forged', joined: '2099-01-01' }, member.id)
  assert.equal(member.id, originalId)
  assert.equal(member.joined, joined)
  assert.equal(loan.borrowerId, originalId)
  assert.equal(member.category, 'Staff')
  const count = store.state.members.length
  assert.throws(() => store.saveMember({ name: 'Duplicate', email: member.email }), /already registered/)
  assert.equal(store.state.members.length, count)
  assert.throws(() => store.saveMember({ name: 'Missing', email: 'missing@example.com' }, 'does-not-exist'), /not found/)
})
