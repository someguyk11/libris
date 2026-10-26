import test from 'node:test'
import assert from 'node:assert/strict'
import { normalizeIsbn, canonicalIsbn } from '../src/utils/isbn.js'
import { registrationDetails, reviewedBatch } from '../src/services/bookRegistration.js'

test('ISBN-10, X, ISBN-13, spaces, hyphens and equivalent edition identity', () => {
  assert.equal(normalizeIsbn(' 0-306-40615-2\n'), '0306406152')
  assert.equal(canonicalIsbn('0 306 40615 2'), '9780306406157')
  assert.equal(normalizeIsbn('0-8044-2957-x'), '080442957X')
  assert.equal(canonicalIsbn('080442957X'), '9780804429573')
  assert.equal(canonicalIsbn('978-0-306-40615-7'), '9780306406157')
  assert.equal(canonicalIsbn('9791090636071'), '9791090636071')
})
test('ISBN rejects incorrect checksums and malformed or non-ISBN barcode values', () => {
  for (const value of ['0306406153', '9780306406158', '0804429570', '978030640615', '4006381333931', 'ISBN 0306406152', '０３０６４０６１５２', '123X456789', '', null, ' '.repeat(65)]) {
    assert.throws(() => canonicalIsbn(value), undefined, String(value))
  }
})
const draft = () => ({ isbn: '0306406152', title: ' A title ', author: ' An author ', bibliography: {} })
test('review payload trims corrections, preserves missing fields and validates bibliography', () => {
  const value = registrationDetails(draft())
  assert.equal(value.isbn, '9780306406157')
  assert.equal(value.title, 'A title')
  assert.equal(value.publish_date, null)
  assert.equal(value.bibliography, null)
  assert.throws(() => registrationDetails({ ...draft(), title: '' }), /Title is required/)
  assert.throws(() => registrationDetails({ ...draft(), author: 'a'.repeat(101) }), /100/)
  for (const cover_url of ['javascript:alert(1)', 'http://example.com/cover', 'https://user:pass@example.com/cover']) {
    assert.throws(() => registrationDetails({ ...draft(), bibliography: { cover_url } }), /HTTPS/)
  }
  assert.throws(() => registrationDetails({ ...draft(), bibliography: { isbn10: '080442957X' } }), /same edition/)
  assert.equal(registrationDetails({ ...draft(), bibliography: { isbn10: '0-306-40615-2', publisher: ' Publisher ' } }).bibliography.publisher, 'Publisher')
})
test('bulk payload requires a shelf, confirmed entries, bounded size and distinct editions', () => {
  const entry = { reviewed: true, draft: draft() }
  assert.throws(() => reviewedBatch([entry], ''), /shelf/)
  assert.throws(() => reviewedBatch([], '1'), /1 and 50/)
  assert.throws(() => reviewedBatch(Array(51).fill(entry), '1'), /1 and 50/)
  assert.throws(() => reviewedBatch([{ ...entry, reviewed: false }], '1'), /Review/)
  assert.throws(() => reviewedBatch([entry, { ...entry, draft: { ...draft(), isbn: '9780306406157' } }], '1'), /same edition/)
  assert.equal(reviewedBatch([entry], '2').shelf_id, 2)
  // Legacy metadata is not rewritten or revalidated when an existing title is skipped.
  assert.deepEqual(reviewedBatch([{ reviewed: true, existing: { id: 17 }, draft: { isbn: '0306406152' } }], '2').books, [{ isbn: '9780306406157', existing_id: 17 }])
})
