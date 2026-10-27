<template>
  <section class="guide-state-page">
    <div class="guide-state-icon"><GuideIcon name="alert" /></div>
    <h1>Assistance needed</h1><p>Atlas is waiting for your help.</p>
    <p class="guide-assistance-copy">An obstacle has paused this simulated journey. Guidance can continue once the path is clear.</p>
    <div class="guide-info guide-surface" role="status"><GuideIcon name="alert" /><p>{{ clear ? 'The simulated path is clear. You can continue.' : 'Atlas is currently waiting near the left side of the aisle.' }}</p></div>
    <p v-if="error" class="guide-feedback" role="alert">{{ error }}</p>
    <div class="guide-actions"><AppButton full-width @click="resume">Continue</AppButton><AppButton variant="secondary" full-width @click="$emit('end')">End session</AppButton></div>
  </section>
</template>
<script setup>
import { computed, ref } from 'vue'
import { useGuestGuide } from '@/composables/useGuestGuide.js'
import GuideIcon from '@/components/guide/GuideIcon.vue'
import AppButton from '@/components/common/AppButton.vue'
const props = defineProps({ now: { type: Number, required: true } })
defineEmits(['end'])
const guide = useGuestGuide(), error = ref('')
const clear = computed(() => props.now >= guide.state.session.obstacleClearAt)
function resume() { try { guide.resume(); error.value = '' } catch (cause) { error.value = cause.message } }
</script>
