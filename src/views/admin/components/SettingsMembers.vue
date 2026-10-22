<template>
  <section id="settings-members" class="settings-section settings-members" aria-labelledby="members-title" tabindex="-1">
    <header class="settings-section-heading">
      <div><h2 id="members-title">Members</h2><p>Manage member categories and library records.</p></div>
      <div class="settings-section-actions"><button v-if="filteredMembers.length > 4" class="admin-text-button" @click="showAll = !showAll">{{ showAll ? 'Show fewer members' : 'View all members' }}<span class="material-symbols-rounded" aria-hidden="true">{{ showAll ? 'expand_less' : 'chevron_right' }}</span></button><AppButton variant="secondary" size="sm" @click="openEditor()"><span class="material-symbols-rounded" aria-hidden="true">person_add</span>Add member</AppButton></div>
    </header>
    <div class="settings-members-toolbar">
      <div class="settings-member-filters" role="group" aria-label="Filter members by category"><button v-for="value in ['All', ...memberCategories]" :key="value" :aria-pressed="category === value" @click="category = value">{{ value }}</button></div>
      <label class="settings-member-search"><span class="material-symbols-rounded" aria-hidden="true">search</span><span class="admin-sr-only">Search members</span><input v-model="search" type="search" placeholder="Find a member…" /></label>
    </div>
    <div v-if="filteredMembers.length" class="settings-member-table" role="region" aria-label="Library members" tabindex="0">
      <table><thead><tr><th scope="col">Name</th><th scope="col">Member ID</th><th scope="col">Category</th><th scope="col">Status</th><th scope="col">Joined</th><th scope="col"><span class="admin-sr-only">Actions</span></th></tr></thead>
        <tbody><tr v-for="(member, index) in visibleMembers" :key="member.id">
          <td><div class="settings-member-name"><span class="settings-avatar" :class="'settings-avatar--' + (index % 4)" aria-hidden="true">{{ member.name.charAt(0).toUpperCase() }}</span><div><strong>{{ member.name }}</strong><small>{{ member.email }}</small></div></div></td>
          <td>{{ member.memberNumber || '—' }}</td><td><span class="settings-category">{{ member.category || 'Unassigned' }}</span></td><td><span class="settings-member-status"><i aria-hidden="true"></i>Registered</span></td><td>{{ joinedDate(member.joined) }}</td>
          <td><button class="admin-icon-button" :aria-label="'Edit ' + member.name" @click="openEditor(member)"><span class="material-symbols-rounded" aria-hidden="true">edit</span></button></td>
        </tr></tbody>
      </table>
    </div>
    <EmptyState v-else title="No matching members" description="Try a different category or search, or add a member to your library." />
    <p class="settings-members-count" role="status">Showing {{ visibleMembers.length }} of {{ filteredMembers.length }} {{ filteredMembers.length === 1 ? 'member' : 'members' }}<span>Categories are library records, not access permissions.</span></p>
    <AdminDialog :open="editorOpen" :title="editingId ? 'Edit member' : 'Add member'" @close="editorOpen = false">
      <p class="admin-muted">Saved locally and available when assigning a book loan.</p>
      <form class="admin-form" @submit.prevent="submit">
        <label>Member name<input v-model="draft.name" required maxlength="100" autocomplete="name" /></label>
        <label>Email<input v-model="draft.email" required type="email" maxlength="254" autocomplete="email" /></label>
        <div class="admin-form-grid"><label>Member ID <span class="field-optional">Optional</span><input v-model="draft.memberNumber" maxlength="30" pattern="[A-Za-z0-9-]{2,30}" placeholder="e.g. ST-1042" /></label><label>Category<select v-model="draft.category"><option v-for="value in memberCategories" :key="value">{{ value }}</option></select></label></div>
        <p v-if="error" class="admin-form-error" role="alert">{{ error }}</p>
        <div class="admin-form-actions"><AppButton type="button" variant="secondary" @click="editorOpen = false">Cancel</AppButton><AppButton type="submit" :disabled="!draft.name.trim() || !draft.email.trim()">{{ editingId ? 'Save changes' : 'Register member' }}</AppButton></div>
      </form>
    </AdminDialog>
  </section>
</template>
<script setup>
import { computed, nextTick, reactive, ref, watch } from 'vue'
import AppButton from '@/components/common/AppButton.vue'
import EmptyState from '@/components/common/EmptyState.vue'
import AdminDialog from './AdminDialog.vue'
import { useAdminStore } from '../useAdminStore.js'
import { memberCategories } from '../settingsData.js'
const emit = defineEmits(['notice'])
const { state, saveMember, storageError } = useAdminStore()
const search = ref(''), category = ref('All'), showAll = ref(false), editorOpen = ref(false), editingId = ref(null), error = ref('')
const draft = reactive({ name: '', email: '', memberNumber: '', category: 'Unassigned' })
const filteredMembers = computed(() => state.members.filter(member => {
  const term = search.value.trim().toLowerCase()
  return (category.value === 'All' || (member.category || 'Unassigned') === category.value) && [member.name, member.email, member.memberNumber || ''].some(value => value.toLowerCase().includes(term))
}))
const visibleMembers = computed(() => showAll.value ? filteredMembers.value : filteredMembers.value.slice(0, 4))
watch([search, category], () => { showAll.value = false }, { flush: 'sync' })
function joinedDate(value) { const date = new Date(value); return Number.isNaN(date.getTime()) ? 'Not recorded' : date.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' }) }
function openEditor(member = null) {
  editingId.value = member?.id || null
  Object.assign(draft, { name: member?.name || '', email: member?.email || '', memberNumber: member?.memberNumber || '', category: member?.category || 'Unassigned' })
  error.value = ''; editorOpen.value = true
}
async function submit() {
  try {
    saveMember(draft, editingId.value)
    await nextTick()
    editorOpen.value = false
    category.value = 'All'; search.value = ''; showAll.value = true
    emit('notice', storageError.value ? 'Member updated for this visit only. Browser storage is unavailable.' : `Member ${editingId.value ? 'updated' : 'registered'} in this browser.`)
  } catch (cause) { error.value = cause.message }
}
</script>
