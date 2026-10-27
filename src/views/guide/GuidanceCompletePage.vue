<template>
  <section class="guide-state-page guide-complete">
    <div class="guide-orbit guide-orbit--small"><GuideIcon name="atlas" /></div>
    <h1>Session complete</h1><p>Thank you for using Atlas.</p>
    <GuideBookSummary :book="book" />
    <AppButton variant="secondary" full-width @click="$emit('search')"><GuideIcon name="search" />Search another book</AppButton>
  </section>
</template>
<script setup>
import GuideBookSummary from '@/components/guide/GuideBookSummary.vue'
import GuideIcon from '@/components/guide/GuideIcon.vue'
import AppButton from '@/components/common/AppButton.vue'
defineProps({ book: { type: Object, required: true } })
defineEmits(['search'])
</script>
