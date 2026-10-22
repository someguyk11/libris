import test from 'node:test'
import assert from 'node:assert/strict'
import { defaultLibraryDetails, validateLibraryDetails, validateMember } from '../src/views/admin/settingsData.js'
import { useSettingsStore } from '../src/views/admin/useSettingsStore.js'

test('library details validate optional contacts without retaining arbitrary fields', () => {
  assert.deepEqual(validateLibraryDetails({ ...defaultLibraryDetails(), name: ' Central Library ', extra: 'ignored' }), { name: 'Central Library', location: '', email: '', phone: '' })
  assert.throws(() => validateLibraryDetails({ name: '  ' }), /library name/)
  assert.throws(() => validateLibraryDetails({ name: 'Library', email: 'invalid' }), /email/)
  assert.throws(() => validateLibraryDetails({ name: 'Library', phone: '------' }), /phone/)
  assert.equal(validateLibraryDetails({ name: 'Library', phone: '+91 98765 43210' }).phone, '+91 98765 43210')
})

test('member categories and identifiers validate independently of access permissions', () => {
  const existing = [{ id: 'member-1', name: 'First', email: 'first@example.com', memberNumber: 'ST-001' }]
  assert.throws(() => validateMember({ name: 'Other', email: 'FIRST@example.com' }, existing), /email is already/)
  assert.throws(() => validateMember({ name: 'Other', email: 'other@example.com', memberNumber: 'st-001' }, existing), /ID is already/)
  assert.throws(() => validateMember({ name: 'Other', email: 'other@example.com', category: 'Admin' }, existing), /category/)
  const edited = validateMember({ ...existing[0], category: 'Teacher', name: ' Edited ' }, existing, 'member-1')
  assert.equal(edited.name, 'Edited')
  assert.equal(edited.id, undefined)
  assert.equal(edited.category, 'Teacher')
  assert.equal(validateMember({ name: 'Legacy', email: 'legacy@example.com' }, []).category, 'Unassigned')
})

test('settings save failures retain existing details and allow retry with the same draft', t => {
  const original = globalThis.localStorage
  t.after(() => { if (original === undefined) delete globalThis.localStorage; else globalThis.localStorage = original })
  let persisted
  globalThis.localStorage = { setItem: (key, value) => { persisted = JSON.parse(value) } }
  const store = useSettingsStore()
  store.saveLibrary({ name: 'Saved Library' })
  assert.equal(persisted.library.name, 'Saved Library')
  globalThis.localStorage = { setItem: () => { throw new Error('quota') } }
  const draft = { name: 'Unsaved Library', location: 'First floor' }
  assert.throws(() => store.saveLibrary(draft), /entries are still here/)
  assert.equal(store.library.name, 'Saved Library')
  assert.equal(draft.name, 'Unsaved Library')
  assert.equal(store.storageError.value, true)
  globalThis.localStorage = { setItem: (key, value) => { persisted = JSON.parse(value) } }
  store.saveLibrary(draft)
  assert.equal(store.storageError.value, false)
  assert.equal(store.library.name, 'Unsaved Library')
  assert.equal(persisted.version, 1)
})
