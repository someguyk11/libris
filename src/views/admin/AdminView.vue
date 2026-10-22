<template>
  <main class="admin-app">
    <button v-if="menuOpen" class="admin-sidebar-backdrop" aria-label="Close navigation" @click="menuOpen = false"></button>
    <aside class="admin-sidebar" :class="{ 'is-open': menuOpen }">
      <a class="admin-brand" href="#/admin"><span class="material-symbols-rounded">eco</span>Libris<small>ADMIN</small></a>
      <nav aria-label="Administration">
        <a href="#/admin" :aria-current="current === 'admin-dashboard' ? 'page' : undefined" @click="menuOpen = false"><span class="material-symbols-rounded">space_dashboard</span>Dashboard</a>
        <a href="#/admin/books" :aria-current="current === 'admin-books' ? 'page' : undefined" @click="menuOpen = false"><span class="material-symbols-rounded">menu_book</span>Books</a>
        <a href="#/admin/shelves" :aria-current="current === 'admin-shelves' ? 'page' : undefined" @click="menuOpen = false"><span class="material-symbols-rounded">location_on</span>Register Shelf<span class="admin-nav-count">{{ state.shelves.length }}</span></a>
        <a href="#/admin/atlas" :aria-current="current.startsWith('admin-atlas') ? 'page' : undefined" @click="menuOpen = false"><span class="material-symbols-rounded">smart_toy</span>Atlas</a>
        <a href="#/admin/settings" :aria-current="current === 'admin-settings' ? 'page' : undefined" @click="menuOpen = false"><span class="material-symbols-rounded" aria-hidden="true">settings</span>Settings</a>
      </nav>
       <div class="admin-sidebar-bottom"><a href="#/explore"><span class="material-symbols-rounded">arrow_back</span>Reader view</a><p class="admin-library-name"><i></i><span>{{ library.name }}</span></p><small>{{ apiMode ? 'Catalogue & shelves: server-backed' : 'Changes saved in this browser' }}</small></div>
    </aside>
    <div class="admin-workspace">
      <header class="admin-topbar">
        <button class="admin-icon-button admin-menu-toggle" aria-label="Open navigation" :aria-expanded="menuOpen" @click="menuOpen = !menuOpen"><span class="material-symbols-rounded">menu</span></button>
        <label class="admin-global-search"><span class="material-symbols-rounded">search</span><span class="admin-sr-only">Search catalogue</span><input ref="searchInput" v-model="query" type="search" placeholder="Search by title, author, ISBN, or genre…" @input="current = 'admin-books'" /><kbd>Ctrl K</kbd></label>
         <span class="admin-demo-label">{{ apiMode ? 'Backend mode' : 'Local demo' }}</span>
        <button class="admin-icon-button admin-motion-toggle" :aria-pressed="animationsEnabled" :aria-label="animationsEnabled ? 'Turn animations off' : 'Turn animations on'" :title="animationsEnabled ? 'Animations on' : 'Animations off'" @click="setAnimationsEnabled(!animationsEnabled)"><span class="material-symbols-rounded">{{ animationsEnabled ? 'motion_photos_on' : 'motion_photos_off' }}</span></button>
        <div class="admin-user"><span>R</span><div>Rohit M<small>Librarian</small></div></div>
      </header>
       <p v-if="storageError" class="admin-notice admin-notice--error" role="alert">Browser storage is unavailable. Your changes will last for this visit only.</p>
       <p v-if="apiMode" class="admin-notice">Books, shelves, and Atlas use the LiBot API. Reader pages remain demos; library preferences are browser-local. Members, loans, and route replay have no backend support. <button class="admin-text-button" :disabled="loading" @click="refresh">{{ loading ? 'Loading…' : 'Refresh catalogue' }}</button></p>
       <p v-if="apiError" class="admin-notice admin-notice--error" role="alert">{{ apiError }} No demo data has been substituted.</p>
      <DashboardView v-if="current === 'admin-dashboard'" @books="openBooks" @add-book="addBook" @members="openMembers" @shelves="current = 'admin-shelves'" @notice="notify" />
      <BooksView v-else-if="current === 'admin-books'" @notice="notify" />
      <ShelvesView v-else-if="current === 'admin-shelves'" />
      <AtlasOverviewView v-else-if="current === 'admin-atlas'" @notice="notify" />
      <AtlasDetailView v-else-if="current === 'admin-atlas-detail'" :key="selectedAtlasId" @notice="notify" />
      <SettingsView v-else-if="current === 'admin-settings'" @notice="notify" />
    </div>
    <Transition name="admin-toast"><div v-if="toast" class="admin-toast" role="status"><span class="material-symbols-rounded">check_circle</span>{{ toast }}</div></Transition>
  </main>
</template>
<script setup>
import { nextTick, onBeforeUnmount, onMounted, ref } from 'vue'
import DashboardView from './DashboardView.vue'
import BooksView from './BooksView.vue'
import ShelvesView from './ShelvesView.vue'
import AtlasOverviewView from './AtlasOverviewView.vue'
import AtlasDetailView from './AtlasDetailView.vue'
import SettingsView from './SettingsView.vue'
import { useNav } from '@/composables/useNav'
import { animationsEnabled, setAnimationsEnabled } from '@/composables/useReducedMotion'
import { useAdminStore } from './useAdminStore'
import { useSettingsStore } from './useSettingsStore.js'
import { useAtlasStore } from './useAtlasStore.js'
import './admin.css'
import './operations.css'
const { current, selectedAtlasId } = useNav()
const { state, query, catalogueStatus, editorRequest, storageError, apiMode, loading, apiError, refresh } = useAdminStore()
const { library } = useSettingsStore()
const { refreshAtlas } = useAtlasStore()
const menuOpen = ref(false), searchInput = ref(null), toast = ref('')
let toastTimer
function notify(message) { toast.value = message; clearTimeout(toastTimer); toastTimer = setTimeout(() => toast.value = '', 3000) }
function openBooks(status = '') { query.value = ''; catalogueStatus.value = status; current.value = 'admin-books' }
function addBook() { editorRequest.value = true; openBooks() }
async function openMembers() { current.value = 'admin-settings'; await nextTick(); const section = document.getElementById('settings-members'); section?.focus({ preventScroll: true }); section?.scrollIntoView({ block: 'start' }) }
function shortcut(event) { if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k' && !document.querySelector('dialog[open]')) { event.preventDefault(); searchInput.value?.focus() } }
onMounted(() => { window.addEventListener('keydown', shortcut); refresh(); refreshAtlas() })
onBeforeUnmount(() => { window.removeEventListener('keydown', shortcut); clearTimeout(toastTimer) })
</script>
