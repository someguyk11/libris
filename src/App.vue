<template>
    <ExploreView v-if="current === 'explore'" />
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
import ExploreView from "@/views/ExploreView.vue"
import DiscoverView from "@/views/DiscoverView.vue"
import MyLibraryView from "@/views/MyLibrary.vue"
import AuthView from "@/views/AuthView.vue"
import ProfileView from "@/views/ProfileView.vue"
import BookDetailsView from "@/views/BookDetailsView.vue"
import RoomView from "@/components/reading-rooms/RoomView.vue"
import RoomsView from "@/views/RoomsView.vue"
import AdminView from "@/views/admin/AdminView.vue"
import { rooms } from "@/mock/data"

const { current, selectedRoom, leaveRoom } = useNav()
</script>
