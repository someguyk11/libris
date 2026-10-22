<template>
  <section class="operations-page settings-page admin-page-enter">
    <header class="operations-heading"><div><p class="admin-eyebrow">SETTINGS</p><h1>Settings</h1><p>Manage your library’s information, members, and workspace preferences.</p></div><span class="settings-local-label"><span class="material-symbols-rounded" aria-hidden="true">devices</span>This browser</span></header>

    <section class="settings-section" aria-labelledby="library-details-title">
      <header class="settings-section-heading"><div><h2 id="library-details-title">Library Details</h2><p>Update your library’s basic information.</p></div></header>
      <dl class="settings-library-details"><div v-for="field in libraryFields" :key="field.key"><dt>{{ field.label }}</dt><dd><span :class="{ 'settings-unset': !library[field.key] }">{{ library[field.key] || 'Not provided' }}</span><button class="admin-icon-button" :aria-label="'Edit ' + field.label.toLowerCase()" @click="openLibrary(field.key)"><span class="material-symbols-rounded" aria-hidden="true">edit</span></button></dd></div></dl>
      <p v-if="settingsStorageError" class="admin-form-error" role="alert">Library details could not be saved. Reopen the editor to try again.</p>
    </section>

    <SettingsMembers v-if="!apiMode" @notice="$emit('notice', $event)" />
    <section v-else id="settings-members" class="settings-section" tabindex="-1"><header class="settings-section-heading"><div><h2>Members</h2><p>Not available in backend mode. LiBot has no member or authentication endpoints.</p></div></header></section>

    <section class="settings-section" aria-labelledby="atlas-settings-title">
      <header class="settings-section-heading"><div><h2 id="atlas-settings-title">Atlas</h2><p>{{ apiMode ? 'Manage database-backed robots and command drafts.' : 'Manage local robot records and simulation workspaces.' }}</p></div><a class="admin-text-button" href="#/admin/atlas">Manage Atlas<span class="material-symbols-rounded" aria-hidden="true">chevron_right</span></a></header>
      <div class="settings-atlas-grid">
        <div class="settings-atlas-item"><span class="material-symbols-rounded" aria-hidden="true">smart_toy</span><div><h3>Registered Atlas</h3><p>{{ atlasState.units.length }} {{ atlasState.units.length === 1 ? 'unit' : 'units' }} {{ apiMode ? 'in the database' : 'in this browser' }}</p></div><strong>{{ atlasState.units.length }}</strong></div>
        <div class="settings-atlas-item"><span class="material-symbols-rounded" aria-hidden="true">route</span><div><h3>{{ apiMode ? 'Encoder routes' : 'Saved simulations' }}</h3><p>{{ apiMode ? 'Measured wheel differences · no autonomous replay' : 'Recorded movement drafts' }}</p></div><strong>{{ atlasState.routes.length }}</strong></div>
        <div class="settings-atlas-item"><span class="material-symbols-rounded" aria-hidden="true">wifi_off</span><div><h3>Hardware connection</h3><p>{{ apiMode ? 'Open a unit for backend connection status' : 'Not connected · frontend preview' }}</p></div></div>
      </div>
      <div class="settings-atlas-workspace"><div><label for="settings-atlas-unit">Atlas workspace</label><p>Open a unit to edit its name, identifier, or simulation.</p></div><div><select id="settings-atlas-unit" v-model="selectedUnitId" :disabled="!atlasState.units.length"><option v-for="unit in atlasState.units" :key="unit.id" :value="unit.id">{{ unit.name }} · {{ unit.identifier }}</option></select><a v-if="selectedUnitId" class="admin-text-button" :href="`#/admin/atlas/${encodeURIComponent(selectedUnitId)}`">Open workspace<span class="material-symbols-rounded" aria-hidden="true">arrow_forward</span></a></div></div>
      <p class="settings-hardware-note"><span class="material-symbols-rounded" aria-hidden="true">info</span>{{ apiMode ? 'Wi-Fi provisioning and reset are available in each Atlas settings dialog. Firmware must poll the backend; registration alone does not connect a robot.' : 'Firmware, network configuration, and physical movement preferences require hardware integration.' }}</p>
    </section>

    <section class="settings-section settings-preferences" aria-labelledby="preferences-title">
      <header class="settings-section-heading"><div><h2 id="preferences-title">Workspace Preferences</h2><p>Make Libris feel right for you.</p></div></header>
      <div class="settings-preference-row"><div><h3 id="animations-label">Interface animations</h3><p id="animations-description">Subtle transitions across Libris. Your device’s reduced-motion preference always takes priority.</p><small v-if="animationsEnabled && reducedMotion">Reduced motion is currently active on this device.</small></div><button class="settings-switch" role="switch" :aria-checked="animationsEnabled" aria-labelledby="animations-label" aria-describedby="animations-description" @click="setAnimationsEnabled(!animationsEnabled)"><span aria-hidden="true"></span><span class="admin-sr-only">{{ animationsEnabled ? 'On' : 'Off' }}</span></button></div>
      <p v-if="animationStorageError" class="admin-form-error" role="alert">Animation preferences could not be saved. This change will last for this visit only.</p>
    </section>
    <footer class="settings-footer"><span class="material-symbols-rounded" aria-hidden="true">hard_drive</span>{{ apiMode ? 'Library profile and interface preferences stay in this browser. Robot registrations and route drafts are stored on the server.' : 'Settings and records are saved locally. They are not shared across devices.' }}</footer>

    <AdminDialog :open="libraryEditorOpen" title="Library details" @close="libraryEditorOpen = false">
      <p class="admin-muted">Update this browser’s library profile. Only the library name is required.</p>
      <form class="admin-form settings-library-form" @submit.prevent="submitLibrary">
        <label>Library name<input v-model="libraryDraft.name" :autofocus="editingField === 'name'" required maxlength="100" autocomplete="organization" /></label>
        <label>Location <span class="field-optional">Optional</span><textarea v-model="libraryDraft.location" :autofocus="editingField === 'location'" rows="2" maxlength="250" autocomplete="street-address"></textarea></label>
        <label>Contact email <span class="field-optional">Optional</span><input v-model="libraryDraft.email" :autofocus="editingField === 'email'" type="email" maxlength="254" autocomplete="email" /></label>
        <label>Contact phone <span class="field-optional">Optional</span><input v-model="libraryDraft.phone" :autofocus="editingField === 'phone'" type="tel" maxlength="30" autocomplete="tel" /></label>
        <p v-if="libraryError" class="admin-form-error" role="alert">{{ libraryError }}</p>
        <div class="admin-form-actions"><AppButton type="button" variant="secondary" @click="libraryEditorOpen = false">Cancel</AppButton><AppButton type="submit" :disabled="!libraryDraft.name.trim()">Save details</AppButton></div>
      </form>
    </AdminDialog>
  </section>
</template>
<script setup>
import { reactive, ref, watch } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import AdminDialog from './components/AdminDialog.vue'
import SettingsMembers from './components/SettingsMembers.vue'
import { useSettingsStore } from './useSettingsStore.js'
import { useAtlasStore } from './useAtlasStore.js'
import { apiMode } from '@/services/api.js'
import useReducedMotion, { animationsEnabled, setAnimationsEnabled, animationStorageError } from '@/composables/useReducedMotion.js'
import './settings.css'
const emit = defineEmits(['notice'])
const { library, storageError: settingsStorageError, saveLibrary } = useSettingsStore()
const { state: atlasState } = useAtlasStore()
const reducedMotion = useReducedMotion()
const selectedUnitId = ref(atlasState.units[0]?.id || '')
watch(() => atlasState.units.map(unit => unit.id), ids => { if (!ids.includes(selectedUnitId.value)) selectedUnitId.value = ids[0] || '' })
const libraryFields = [{ key: 'name', label: 'Library Name' }, { key: 'location', label: 'Location' }, { key: 'email', label: 'Contact Email' }, { key: 'phone', label: 'Contact Phone' }]
const libraryEditorOpen = ref(false), libraryError = ref(''), editingField = ref('name')
const libraryDraft = reactive({ ...library })
function openLibrary(field) { Object.assign(libraryDraft, library); editingField.value = field; libraryError.value = ''; libraryEditorOpen.value = true }
function submitLibrary() {
  try { saveLibrary(libraryDraft); libraryEditorOpen.value = false; emit('notice', 'Library details saved in this browser.') }
  catch (cause) { libraryError.value = cause.message }
}
</script>
