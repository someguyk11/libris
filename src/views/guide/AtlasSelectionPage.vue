<template>
  <section>
    <GuideBookSummary :book="book" />
    <div class="guide-section-copy"><h2>Select an Atlas</h2><p>Choose an available Atlas to assist you.</p></div>
    <div class="guide-atlas-list" role="radiogroup" aria-label="Select an Atlas" @keydown="navigateRadios">
      <AtlasStatusCard v-for="unit in atlases" :key="unit.id" :atlas="unit" :selected="state.atlasId === unit.id" @select="selectAtlas" />
    </div>
    <AppButton class="guide-bottom-action" full-width :disabled="!eligibleAtlas(atlas)" @click="$emit('continue')">Continue <GuideIcon name="arrow" /></AppButton>
  </section>
</template>
<script setup>
import { useGuestGuide } from '@/composables/useGuestGuide.js'
import { eligibleAtlas } from '@/services/guestGuideModel.js'
import GuideBookSummary from '@/components/guide/GuideBookSummary.vue'
import AtlasStatusCard from '@/components/guide/AtlasStatusCard.vue'
import AppButton from '@/components/common/AppButton.vue'
import GuideIcon from '@/components/guide/GuideIcon.vue'
const { book, state, atlas, atlases, selectAtlas } = useGuestGuide()
defineEmits(['continue'])
function navigateRadios(event) {
  if (!['ArrowDown', 'ArrowUp', 'ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return
  event.preventDefault()
  const options = atlases.filter(eligibleAtlas), index = options.findIndex(unit => unit.id === state.atlasId)
  const next = event.key === 'Home' ? 0 : event.key === 'End' ? options.length - 1 : (index + (['ArrowLeft', 'ArrowUp'].includes(event.key) ? -1 : 1) + options.length) % options.length
  selectAtlas(options[next].id)
  event.currentTarget.querySelectorAll('button:not(:disabled)')[next]?.focus()
}
</script>
