<template>
  <div class="admin-dashboard admin-page-enter">
    <header class="admin-page-heading admin-dashboard-hero"><div><p class="admin-eyebrow">YOUR LIBRARY, AT A GLANCE</p><h1>{{ apiMode ? 'Library overview' : greeting + ', Rohit' }}</h1><p>{{ apiMode ? 'Catalogue and locations from your library backend.' : 'Here’s what’s happening with your library today.' }}</p></div><span class="admin-date">{{ today }}</span></header>
    <template v-if="apiMode">
      <div class="admin-kpis"><button class="admin-panel admin-kpi" @click="$emit('books', '')"><span class="admin-kpi-icon material-symbols-rounded">menu_book</span><div><p>Loaded catalogue titles</p><strong>{{ loaded ? activeBooks.length : '—' }}</strong><small>{{ hasMoreBooks ? 'More titles available in Books' : 'From the database' }}</small></div></button><button class="admin-panel admin-kpi" @click="$emit('shelves')"><span class="admin-kpi-icon material-symbols-rounded">shelves</span><div><p>Registered shelves</p><strong>{{ loaded ? state.shelves.length : '—' }}</strong><small>Manage physical locations</small></div></button></div>
      <section class="admin-panel admin-quick-actions"><header class="admin-panel-heading"><h2>Library operations</h2></header><button @click="$emit('add-book')"><span class="material-symbols-rounded">add_box</span>Register a book<span class="material-symbols-rounded">chevron_right</span></button><button @click="$emit('shelves')"><span class="material-symbols-rounded">add_location_alt</span>Manage shelves<span class="material-symbols-rounded">chevron_right</span></button><button @click="current = 'admin-atlas'"><span class="material-symbols-rounded">smart_toy</span>Open Atlas<span class="material-symbols-rounded">chevron_right</span></button><p class="admin-muted">Circulation, member records, and activity history are not provided by the LiBot backend.</p></section>
    </template>
    <template v-else>
    <div class="admin-kpis">
      <button class="admin-panel admin-kpi" @click="$emit('books', '')"><span class="admin-kpi-icon material-symbols-rounded">menu_book</span><div><p>Catalogue titles</p><strong>{{ activeBooks.length }}</strong><small>{{ state.books.filter(book => book.archived).length }} archived</small></div></button>
      <button class="admin-panel admin-kpi" @click="$emit('members')"><span class="admin-kpi-icon material-symbols-rounded">group</span><div><p>Registered members</p><strong>{{ state.members.length }}</strong><small>Manage your readers <span>↗</span></small></div></button>
      <button class="admin-panel admin-kpi" @click="$emit('books', 'Borrowed')"><span class="admin-kpi-icon material-symbols-rounded">auto_stories</span><div><p>Books on loan</p><strong>{{ loans.length }}</strong><small class="admin-positive">{{ activeBooks.filter(book => book.status === 'Available').length }} ready to borrow</small></div></button>
      <button class="admin-panel admin-kpi" @click="$emit('books', 'attention')"><span class="admin-kpi-icon is-warning material-symbols-rounded">priority_high</span><div><p>Needs attention</p><strong>{{ attention.length }}</strong><small>Overdue or in maintenance</small></div></button>
    </div>
    </template>
    <div v-if="!apiMode" class="admin-dashboard-grid">
      <section class="admin-panel admin-loans-panel"><header class="admin-panel-heading"><div><h2>On the shelves. Out in the world.</h2><p class="admin-muted">Keep the next chapter moving.</p></div><button class="admin-text-button" @click="$emit('books', 'Borrowed')">View loans <span class="material-symbols-rounded">east</span></button></header>
        <div class="admin-availability"><div><strong>{{ availablePercent }}<small>%</small></strong><span>of your catalogue<br>is ready to borrow</span></div><div class="admin-availability-track" aria-label="Catalogue availability"><i v-for="status in statuses" :key="status" :style="{ flex: Math.max(0.01, activeBooks.filter(book => book.status === status).length) }" :class="'is-' + status.toLowerCase()" :title="`${status}: ${activeBooks.filter(book => book.status === status).length}`"></i></div><div class="admin-availability-legend"><button v-for="status in statuses" :key="status" @click="$emit('books', status)"><i :class="'is-' + status.toLowerCase()"></i>{{ status }}<b>{{ activeBooks.filter(book => book.status === status).length }}</b></button></div></div>
        <div class="admin-due-list"><p class="admin-eyebrow">NEXT RETURNS</p><article v-for="book in sortedLoans.slice(0, 3)" :key="book.id"><BookCover :cover="book.cover" :title="book.title" size="small" /><div><strong>{{ book.title }}</strong><small>{{ state.members.find(member => member.id === book.borrowerId)?.name || 'Library member' }} · <span :class="{ 'admin-overdue': isOverdue(book.due) }">{{ dueLabel(book.due) }}</span></small></div><button class="admin-text-button" :aria-label="'Return ' + book.title" @click="returnBook(book)">Return <span class="material-symbols-rounded">assignment_return</span></button></article><p v-if="!loans.length" class="admin-muted">All books are back on the shelf.</p></div>
      </section>
      <section class="admin-panel admin-activity"><header class="admin-panel-heading"><h2>Recent activity</h2><button class="admin-text-button" :aria-expanded="allActivity" @click="allActivity = !allActivity">{{ allActivity ? 'Show less' : 'View all' }}</button></header><ul><li v-for="event in recentActivity" :key="event.id"><span class="material-symbols-rounded" :class="{ 'admin-positive': event.type === 'return' }">{{ activityIcon(event.type) }}</span><div><strong>{{ event.title }}</strong><p>{{ event.detail }}</p></div><time :datetime="event.at" :title="new Date(event.at).toLocaleString()">{{ timeAgo(event.at) }}</time></li></ul></section>
      <ActivityChart :events="state.events" />
      <section class="admin-panel admin-quick-actions"><header class="admin-panel-heading"><h2>A few useful shortcuts</h2></header><button @click="$emit('add-book')"><span class="material-symbols-rounded">add_box</span>Add a book<span class="material-symbols-rounded">chevron_right</span></button><button @click="$emit('members')"><span class="material-symbols-rounded">person_add</span>Register a member<span class="material-symbols-rounded">chevron_right</span></button><button @click="$emit('shelves')"><span class="material-symbols-rounded">add_location_alt</span>Register a shelf<span class="material-symbols-rounded">chevron_right</span></button><button @click="current = 'rooms'"><span class="material-symbols-rounded">chair</span>Visit the reading rooms<span class="material-symbols-rounded">north_east</span></button><blockquote><span class="material-symbols-rounded">eco</span>A great library is a place<br>where curiosity finds a home.</blockquote></section>
    </div>
  </div>
</template>
<script setup>
import { computed, ref } from 'vue'
import { useAdminStore } from './useAdminStore'
import { statuses } from './adminData'
import ActivityChart from './components/ActivityChart.vue'
import BookCover from '@/components/book/BookCover.vue'
import { useNav } from '@/composables/useNav'
const emit = defineEmits(['books', 'add-book', 'members', 'shelves', 'notice'])
const { state, activeBooks, loans, attention, markAvailable, apiMode, loaded, hasMoreBooks } = useAdminStore()
const { current } = useNav()
const allActivity = ref(false)
const hour = new Date().getHours()
const greeting = hour < 12 ? 'Good morning' : hour < 17 ? 'Good afternoon' : 'Good evening'
const today = new Date().toLocaleDateString(undefined, { weekday: 'long', month: 'short', day: 'numeric' })
const availablePercent = computed(() => activeBooks.value.length ? Math.round(activeBooks.value.filter(book => book.status === 'Available').length / activeBooks.value.length * 100) : 0)
const sortedLoans = computed(() => [...loans.value].sort((a, b) => a.due.localeCompare(b.due)))
const recentActivity = computed(() => [...state.events].sort((a, b) => new Date(b.at) - new Date(a.at)).slice(0, allActivity.value ? 30 : 5))
const localDate = () => { const date = new Date(); return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}` }
const isOverdue = date => date && date < localDate()
function dueLabel(date) { if (!date) return 'No due date'; if (isOverdue(date)) return 'Overdue'; return 'Due ' + new Date(date + 'T12:00:00').toLocaleDateString(undefined, { month: 'short', day: 'numeric' }) }
function returnBook(book) { markAvailable(book); emit('notice', `${book.title} returned to the shelf`) }
function activityIcon(type) { return ({ loan: 'auto_stories', return: 'assignment_return', 'book-added': 'library_add', 'book-updated': 'edit', member: 'person_add', shelf: 'shelves', archived: 'inventory_2' })[type] || 'history' }
function timeAgo(date) { const minutes = Math.max(0, Math.floor((Date.now() - new Date(date)) / 60000)); return minutes < 1 ? 'Now' : minutes < 60 ? `${minutes}m ago` : minutes < 1440 ? `${Math.floor(minutes / 60)}h ago` : `${Math.floor(minutes / 1440)}d ago` }
</script>
