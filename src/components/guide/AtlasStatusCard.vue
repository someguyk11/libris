<template>
  <button class="guide-atlas-card guide-surface" :class="{ selected }" role="radio" :aria-checked="selected" :disabled="!eligibleAtlas(atlas)" @click="$emit('select', atlas.id)">
    <GuideIcon name="atlas" class="guide-atlas-card__icon" />
    <span class="guide-atlas-card__copy"><strong>{{ atlas.id }}</strong><GuideAvailability :available="eligibleAtlas(atlas)" /><span><GuideIcon name="battery" />{{ atlas.battery }}% battery{{ !atlas.available ? ' · Offline' : '' }}</span></span>
    <span class="guide-radio" aria-hidden="true"><GuideIcon v-if="selected" name="check" /></span>
  </button>
</template>
<script setup>
import GuideIcon from './GuideIcon.vue'
import GuideAvailability from './GuideAvailability.vue'
import { eligibleAtlas } from '@/services/guestGuideModel.js'
defineProps({ atlas: { type: Object, required: true }, selected: Boolean })
defineEmits(['select'])
</script>
