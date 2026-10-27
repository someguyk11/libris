<template>
    <main v-if="blocked" class="session-gate" role="status">
      <p>Staff access is required.</p>
      <button @click="current = 'explore'">Back to Libris</button>
    </main>
    <GuestGuideView v-else-if="current === 'guide' || current.startsWith('guide-')" />
    <main v-else-if="apiMode && ['rooms', 'room', 'groups'].includes(current)" class="session-gate">
      <h1>Reading rooms</h1><p>Live reading rooms are not supported by this backend. No room has been joined.</p>
      <button @click="current = 'discover'">Browse books</button>
    </main>
    <ExploreView v-else-if="current === 'explore'" />
    <DiscoverView v-else-if="current === 'discover'" />
    <MyLibraryView v-else-if="current === 'library'" />
    <ProfileView v-else-if="current === 'profile'" />
    <BookDetailsView v-else-if="current === 'book'" />
    <AuthView v-else-if="current === 'auth'" />
    <RoomView v-else-if="current === 'room'" :room="selectedRoom || rooms[0]" @leave="leaveRoom" />
    <RoomsView v-else-if="current === 'rooms' || current === 'groups'" />
    <AdminView v-else-if="current.startsWith('admin-')" />
    <ExploreView v-else />
</template>

<script setup>
import { useNav } from "@/composables/useNav"
import { computed, watch, defineAsyncComponent } from 'vue'
import { apiMode } from '@/services/api.js'
import { useSession } from '@/composables/useSession.js'
import { hasAuthCallback } from '@/services/authCallback.js'
const ExploreView = defineAsyncComponent(() => import('@/views/ExploreView.vue'))
const DiscoverView = defineAsyncComponent(() => import('@/views/DiscoverView.vue'))
const MyLibraryView = defineAsyncComponent(() => import('@/views/MyLibrary.vue'))
const AuthView = defineAsyncComponent(() => import('@/views/AuthView.vue'))
const ProfileView = defineAsyncComponent(() => import('@/views/ProfileView.vue'))
const BookDetailsView = defineAsyncComponent(() => import('@/views/BookDetailsView.vue'))
const RoomView = defineAsyncComponent(() => import('@/components/reading-rooms/RoomView.vue'))
const RoomsView = defineAsyncComponent(() => import('@/views/RoomsView.vue'))
const AdminView = defineAsyncComponent(() => import('@/views/admin/AdminView.vue'))
const GuestGuideView = defineAsyncComponent(() => import('@/views/guide/GuestGuideView.vue'))
import { rooms } from "@/mock/data"

const { current, selectedRoom, leaveRoom, requireSignIn } = useNav()
if (apiMode && hasAuthCallback()) current.value = 'auth'
const { user, isStaff, refresh } = useSession()
const protectedRoute = computed(() => ['library', 'profile', 'rooms', 'room', 'groups'].includes(current.value) || current.value.startsWith('admin-'))
const blocked = computed(() => apiMode && current.value.startsWith('admin-') && user.value && !isStaff.value)
watch([current, user], () => {
  if (apiMode && protectedRoute.value && !user.value) requireSignIn()
}, { immediate: true, flush: 'sync' })
if (apiMode && !hasAuthCallback()) refresh()
</script>

<style scoped>
.session-gate { min-height:100vh; padding:3rem 1.5rem; color:var(--text-primary); background:var(--bg-primary); }
.session-gate button { margin:.5rem .5rem 0 0; padding:.65rem; color:var(--accent); background:var(--bg-secondary); border:1px solid var(--border-soft); border-radius:10px; }
</style>
