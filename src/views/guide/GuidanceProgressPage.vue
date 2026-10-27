<template>
  <section class="guide-progress">
    <div class="guide-orbit"><GuideIcon name="atlas" /></div>
    <h2>Atlas is on the move</h2><p>Heading to {{ book.aisle }} → {{ book.shelf }}</p>
    <ol class="guide-stages" aria-label="Guidance progress"><li v-for="(stage, index) in STAGES" :key="stage" :class="{ complete: index < session.stage, active: index === session.stage }" :aria-current="index === session.stage ? 'step' : undefined"><span><GuideIcon :name="index < session.stage ? 'check' : index === session.stage ? 'atlas' : 'pin'" /></span><p>{{ stage }}</p></li></ol>
    <div class="guide-info guide-surface"><GuideIcon name="info" /><p>Please stay on the designated path and allow Atlas to pass.</p></div>
  </section>
</template>
<script setup>
import { STAGES } from '@/services/guestGuideModel.js'
import GuideIcon from '@/components/guide/GuideIcon.vue'
defineProps({ book: { type: Object, required: true }, session: { type: Object, required: true } })
</script>
