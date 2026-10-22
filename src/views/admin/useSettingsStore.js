import { reactive, readonly, ref } from 'vue'
import { defaultLibraryDetails, validateLibraryDetails } from './settingsData.js'

const storageKey = 'libris-settings-v1'
let initial = defaultLibraryDetails()
try {
  const saved = JSON.parse(localStorage.getItem(storageKey))
  if (saved?.version === 1) initial = validateLibraryDetails(saved.library)
} catch { /* Invalid or unavailable storage falls back to the local defaults. */ }
const library = reactive(initial)
const storageError = ref(false)

function saveLibrary(input) {
  const checked = validateLibraryDetails(input)
  // Persist before replacing the displayed details, so a failed save preserves the form.
  try {
    localStorage.setItem(storageKey, JSON.stringify({ version: 1, library: checked }))
    storageError.value = false
  } catch {
    storageError.value = true
    throw new Error('Could not save to browser storage. Your entries are still here; enable storage and try again.')
  }
  Object.assign(library, checked)
}

export function useSettingsStore() { return { library: readonly(library), storageError, saveLibrary } }
