<template>
  <main ref="root" class="guest-guide" tabindex="-1">
    <div class="guide-demo-label">UI preview <span aria-hidden="true">·</span> Simulated catalogue &amp; Atlas guidance</div>
    <div class="guide-shell">
      <header v-if="current !== 'guide' && status !== 'complete'" class="guide-step-header"><button class="guide-icon-button" :aria-label="sessionActive ? 'Close guidance' : 'Go back'" @click="back"><GuideIcon name="back" /></button><h1 v-if="headerTitle">{{ headerTitle }}</h1></header>
      <GuestDiscoveryPage v-if="current === 'guide'" />
      <section v-else-if="notFound" class="guide-empty"><GuideIcon name="book" /><h1>Book not found</h1><p>This preview book is not in the catalogue.</p><AppButton @click="current = 'guide'">Search books</AppButton></section>
      <div v-else class="guide-step-content">
        <GuestBookDetailsPage v-if="current === 'guide-book' && book" :book="book" @setup="startSetup" />
        <AtlasSelectionPage v-else-if="current === 'guide-setup' && book" @continue="startGuidance" />
        <template v-else-if="current === 'guide-session' && state.session && book">
          <div aria-live="polite" class="guide-sr-only">{{ status === 'moving' ? `Guidance: ${STAGES[state.session.stage]}` : status }}</div>
          <GuidanceProgressPage v-if="status === 'moving'" :book="book" :session="state.session" />
          <GuidanceAssistancePage v-else-if="status === 'paused'" :now="now" @end="confirmEnd" />
          <GuidanceArrivalPage v-else-if="status === 'arrived'" :book="book" @location="showDialog('location')" @end="confirmEnd" />
          <GuidanceCompletePage v-else-if="status === 'complete'" :book="book" @search="searchAgain" />
        </template>
      </div>
    </div>
    <dialog ref="dialog" class="guide-dialog" :aria-labelledby="dialogMode === 'end' ? 'guide-end-title' : 'guide-location-title'" @click="closeOnBackdrop">
      <template v-if="dialogMode === 'end'"><h2 id="guide-end-title">End this guidance session?</h2><p>This will end the simulated journey.</p><div class="guide-actions"><AppButton variant="secondary" @click="dialog.close()">Keep session</AppButton><AppButton @click="endSession">End session</AppButton></div></template>
      <template v-else-if="book"><h2 id="guide-location-title">Book location</h2><p class="guide-location"><GuideIcon name="pin" />{{ book.location }}</p><p>{{ book.directions }}</p><p class="guide-meta">Illustrative shelf directions, not a live position.</p><AppButton full-width @click="dialog.close()">Close location</AppButton></template>
    </dialog>
  </main>
</template>
<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { useNav } from '@/composables/useNav.js'
import { useGuestGuide } from '@/composables/useGuestGuide.js'
import { eligibleAtlas, STAGES } from '@/services/guestGuideModel.js'
import AppButton from '@/components/common/AppButton.vue'
import GuideIcon from '@/components/guide/GuideIcon.vue'
import GuestDiscoveryPage from './GuestDiscoveryPage.vue'
import GuestBookDetailsPage from './GuestBookDetailsPage.vue'
import AtlasSelectionPage from './AtlasSelectionPage.vue'
import GuidanceProgressPage from './GuidanceProgressPage.vue'
import GuidanceAssistancePage from './GuidanceAssistancePage.vue'
import GuidanceArrivalPage from './GuidanceArrivalPage.vue'
import GuidanceCompletePage from './GuidanceCompletePage.vue'
import '@/styles/guest-guide.css'

const { current, selectedGuideBookId, selectedGuideSessionId, openGuideBook, openGuideSession } = useNav()
const guide = useGuestGuide(), { state, book, atlas } = guide
const root = ref(null), dialog = ref(null), dialogMode = ref('end'), now = ref(Date.now()), notFound = ref(false)
const status = computed(() => current.value === 'guide-session' ? state.session?.status : null)
const sessionActive = computed(() => ['moving', 'paused', 'arrived'].includes(status.value))
const headerTitle = computed(() => current.value === 'guide-setup' ? 'Guidance Setup' : status.value === 'moving' ? 'Guidance in Progress' : '')
watch([current, selectedGuideBookId, selectedGuideSessionId], async () => {
  notFound.value = false
  if (current.value === 'guide-book') notFound.value = !guide.selectBook(selectedGuideBookId.value)
   if (current.value === 'guide-setup' && !book.value?.available) current.value = 'guide'
  if (current.value === 'guide-session' && (!state.session || selectedGuideSessionId.value !== state.session.id || !book.value)) current.value = 'guide'
  await nextTick(); root.value?.focus({ preventScroll: true })
}, { immediate: true })
watch(status, async () => { await nextTick(); root.value?.focus({ preventScroll: true }) })
let ticker
onMounted(() => { ticker = setInterval(() => { now.value = Date.now(); if (current.value === 'guide-session') guide.tick() }, 300) })
function beforeUnload(event) { if (sessionActive.value) { event.preventDefault(); event.returnValue = '' } }
if (typeof window !== 'undefined') window.addEventListener('beforeunload', beforeUnload)
onBeforeUnmount(() => { clearInterval(ticker); window.removeEventListener('beforeunload', beforeUnload) })
function startSetup() { state.session = null; guide.persist(); current.value = 'guide-setup' }
function startGuidance() {
  if (!book.value?.available || !eligibleAtlas(atlas.value)) return
  guide.start(); openGuideSession(state.session.id)
}
function back() {
  if (sessionActive.value) { confirmEnd(); return }
  if (current.value === 'guide-setup' && book.value) openGuideBook(book.value.id)
  else current.value = 'guide'
}
function showDialog(mode) { dialogMode.value = mode; dialog.value.showModal() }
function confirmEnd() { showDialog('end') }
function closeOnBackdrop(event) { if (event.target === dialog.value) { const rect = dialog.value.getBoundingClientRect(); if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) dialog.value.close() } }
function endSession() { guide.end(); dialog.value.close() }
function searchAgain() { guide.reset(); current.value = 'guide' }
</script>
