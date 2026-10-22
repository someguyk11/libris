<template>
  <section class="operations-page atlas-overview admin-page-enter">
    <header class="operations-heading">
      <div><p class="admin-eyebrow">ATLAS</p><h1>Atlas</h1><p>Manage your library robots. A little direction goes a long way.</p></div>
      <AppButton :disabled="loading" @click="openSetup"><span class="material-symbols-rounded" aria-hidden="true">add</span>Add Atlas</AppButton>
    </header>
    <p class="operations-demo-note"><span class="material-symbols-rounded">info</span>{{ apiMode ? 'Robot registrations and route drafts are stored in the database. Registration does not connect hardware. Battery and task telemetry are not implemented.' : 'Frontend preview · Status readings below are sample snapshots. No hardware is connected.' }}</p>
    <p v-if="apiMode" class="admin-muted"><button class="admin-text-button" :disabled="loading || pending" @click="refreshAtlas">{{ loading ? 'Loading Atlas…' : 'Refresh Atlas' }}</button></p>
    <p v-if="apiError" class="admin-notice admin-notice--error" role="alert">{{ apiError }}</p>
    <p v-if="apiMode && legacyUnits.length" class="admin-notice">{{ legacyUnits.length }} previous browser-only robot {{ legacyUnits.length === 1 ? 'record is' : 'records are' }} available. Import copies their names and firmware IDs to the database, not demo simulations. <button class="admin-text-button" :disabled="pending || loading || !loaded" @click="importPrevious">Import previous robots</button></p>
    <p v-if="importError" class="admin-notice admin-notice--error" role="alert">{{ importError }}</p>
    <p v-if="storageError" class="admin-notice admin-notice--error" role="alert">Browser storage is unavailable. Changes will last for this visit only.</p>
    <div class="atlas-grid">
      <article v-for="unit in state.units" :key="unit.id" class="atlas-unit">
        <div class="atlas-unit-status"><span class="atlas-status" :class="unit.snapshot ? (unit.snapshot.online ? 'sample-online' : 'sample-offline') : ''"><i></i>{{ apiMode ? (unit.connectionError ? 'Backend unavailable' : unit.telemetry?.status || 'Checking…') : unit.snapshot ? (unit.snapshot.online ? 'Online · sample' : 'Offline · sample') : 'Not connected' }}</span><span class="atlas-mode">{{ apiMode ? 'Backend status' : unit.snapshot?.mode || 'Registered' }}</span></div>
        <h2><a :href="atlasLink(unit.id)">{{ unit.name }}</a></h2><p class="atlas-identifier">{{ unit.identifier }}</p>
        <dl class="atlas-metrics">
          <div><span class="material-symbols-rounded">battery_charging_full</span><dt>Battery</dt><dd>{{ !apiMode && unit.snapshot ? unit.snapshot.battery + '%' : 'Unavailable' }}</dd></div>
          <div><span class="material-symbols-rounded">wifi</span><dt>Network</dt><dd>{{ apiMode ? (unit.telemetry?.telemetry_at ? unit.telemetry.wifi_ssid : 'Not yet reported') : unit.snapshot ? (unit.snapshot.online ? 'Connected' : 'Disconnected') : 'Unavailable' }}</dd></div>
          <div><span class="material-symbols-rounded">schedule</span><dt>Last seen</dt><dd>{{ apiMode ? atlasLastSeen(unit.telemetry) : unit.snapshot?.lastSeen || 'Never connected' }}</dd></div>
        </dl>
        <p v-if="apiMode && unit.connectionError" class="admin-form-error" role="alert">{{ unit.connectionError }}</p>
        <a class="atlas-open" :href="atlasLink(unit.id)" :aria-label="'Open ' + unit.name"><span class="material-symbols-rounded">route</span><span>{{ apiMode ? 'Open robot workspace' : unit.snapshot?.task || 'Open local workspace' }}</span><span class="material-symbols-rounded">arrow_forward</span></a>
      </article>
      <button class="atlas-add-card" :disabled="loading" @click="openSetup"><span class="material-symbols-rounded">add</span><strong>Add New Atlas</strong><span>{{ apiMode ? 'Register a robot in the database.' : 'Register a robot in your local workspace.' }}</span><span class="atlas-add-link">Add Atlas <span class="material-symbols-rounded">arrow_forward</span></span></button>
    </div>

    <AdminDialog :open="setupOpen" title="Add Atlas" @close="setupOpen = false">
      <p class="admin-muted">{{ apiMode ? 'Register a persistent Atlas using its exact, case-sensitive firmware ID. This does not connect the physical robot. Firmware-discovered robots are listed automatically.' : 'Create a local Atlas record. Connecting a physical robot will be available with hardware integration.' }}</p>
      <form class="admin-form" @submit.prevent="submit">
        <label>Atlas name<input v-model="form.name" required maxlength="80" placeholder="e.g. Atlas-04" /></label>
        <label>Robot identifier<input v-model="form.identifier" required pattern="[A-Za-z0-9\-]{2,40}" maxlength="40" placeholder="e.g. AT-004" /></label>
        <p v-if="error" class="admin-form-error" role="alert">{{ error }}</p>
        <div class="admin-form-actions"><AppButton variant="secondary" type="button" :disabled="pending" @click="setupOpen = false">Cancel</AppButton><AppButton type="submit" :disabled="!canSave || pending">{{ pending ? 'Registering…' : 'Register Atlas' }}</AppButton></div>
      </form>
    </AdminDialog>
  </section>
</template>
<script setup>
import { computed, reactive, ref } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import AdminDialog from './components/AdminDialog.vue'
import { useAtlasStore } from './useAtlasStore.js'
import { validateAtlas } from './atlasData.js'
import { apiMode } from '@/services/api.js'
import { useAtlasTelemetry, atlasLastSeen } from '@/composables/useAtlasTelemetry.js'
const emit = defineEmits(['notice'])
const { state, storageError, saveUnit, loading, loaded, apiError, refreshAtlas, legacyUnits, importLegacyUnits } = useAtlasStore()
useAtlasTelemetry(() => state.units)
const setupOpen = ref(false), error = ref('')
const pending = ref(false), importError = ref('')
const form = reactive({ name: '', identifier: '' })
const canSave = computed(() => Boolean(form.name.trim() && /^[A-Za-z0-9-]{2,40}$/.test(form.identifier.trim())))
const atlasLink = id => `#/admin/atlas/${encodeURIComponent(id)}`
function openSetup() { form.name = ''; form.identifier = ''; error.value = ''; setupOpen.value = true }
async function importPrevious() {
  pending.value = true; importError.value = ''
  try { const count = await importLegacyUnits(); emit('notice', `${count} robot registrations saved to the database.`) }
  catch (cause) { importError.value = cause.message + ' Already imported records are retained; original browser data is unchanged.' }
  finally { pending.value = false }
}
async function submit() {
  if (pending.value) return
  pending.value = true
  try {
    validateAtlas(form, state.units, null, apiMode)
    const unit = await saveUnit(form)
    setupOpen.value = false
    emit('notice', `${unit.name} registered ${apiMode ? 'in the database' : storageError.value ? 'for this visit only' : 'in this browser'}.`)
  } catch (cause) { error.value = cause.message }
  finally { pending.value = false }
}
</script>
