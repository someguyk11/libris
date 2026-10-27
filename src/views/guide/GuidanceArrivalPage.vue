<template>
  <section class="guide-state-page">
    <div class="guide-state-icon is-success"><GuideIcon name="check" /></div>
    <h1>You've arrived!</h1><p>Atlas has reached the shelf.</p>
    <GuideBookSummary :book="book" />
    <div class="guide-actions"><AppButton full-width @click="$emit('location')">View book location <GuideIcon name="pin" /></AppButton><AppButton variant="secondary" full-width @click="$emit('end')">End session</AppButton></div>
  </section>
</template>
<script setup>
import GuideBookSummary from '@/components/guide/GuideBookSummary.vue'
import GuideIcon from '@/components/guide/GuideIcon.vue'
import AppButton from '@/components/common/AppButton.vue'
defineProps({ book: { type: Object, required: true } })
defineEmits(['location', 'end'])
</script>
