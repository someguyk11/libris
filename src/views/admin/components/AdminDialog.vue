<template>
  <dialog ref="dialog" class="admin-dialog" :aria-labelledby="headingId" @cancel.prevent="$emit('close')" @click="backdropClose">
    <header><div><p class="admin-eyebrow">LIBRIS ADMINISTRATION</p><h2 :id="headingId">{{ title }}</h2></div><button class="admin-icon-button" aria-label="Close dialog" @click="$emit('close')"><span class="material-symbols-rounded">close</span></button></header>
    <slot />
  </dialog>
</template>
<script setup>
import { nextTick, ref, useId, watch } from 'vue'
const props = defineProps({ open: Boolean, title: String })
const emit = defineEmits(['close'])
const dialog = ref(null)
const headingId = useId()
watch(() => props.open, async open => { await nextTick(); if (open && !dialog.value.open) dialog.value.showModal(); else if (!open && dialog.value.open) dialog.value.close() }, { immediate: true })
function backdropClose(event) { if (event.target === dialog.value) { const bounds = dialog.value.getBoundingClientRect(); if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) emit('close') } }
</script>
