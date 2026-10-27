<template>
  <div>
    <section class="guide-hero">
      <header class="guide-brand-row"><span class="guide-wordmark">Libris</span><a href="#/auth" @click="openSignup">Sign up</a></header>
      <h1>Find your next <em>world.</em></h1>
      <p>Search for books, get directions, explore more.</p>
      <BookSearchBar v-model="state.query" @search="focusBooks" />
    </section>
    <div ref="catalogue" class="guide-discovery-content" tabindex="-1">
      <BookCatalogue :books="filteredBooks" v-model:layout="state.layout" @select="nav.openGuideBook" @clear="state.query = ''" />
    </div>
  </div>
</template>
<script setup>
import { computed, ref, watch } from 'vue'
import { useGuestGuide } from '@/composables/useGuestGuide.js'
import { useNav } from '@/composables/useNav.js'
import { filterCatalogue } from '@/services/guestGuideModel.js'
import BookSearchBar from '@/components/guide/BookSearchBar.vue'
import BookCatalogue from '@/components/guide/BookCatalogue.vue'
const { state, books, persist } = useGuestGuide()
const nav = useNav(), catalogue = ref(null)
const filteredBooks = computed(() => filterCatalogue(books, state.query))
watch(() => [state.query, state.layout], persist)
function focusBooks() { catalogue.value?.focus({ preventScroll: true }) }
function openSignup() { nav.authMode.value = 'signup' }
</script>
