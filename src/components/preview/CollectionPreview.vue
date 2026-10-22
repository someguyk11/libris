<template>
  <CuratorCollectionPreview v-bind="$props" category="Collection" @close="$emit('close')" @open="$emit('open')" />
</template>

<script setup>
import CuratorCollectionPreview from "./CuratorCollectionPreview.vue"

defineProps({
  open: Boolean,
  title: { type: String, required: true },
  description: { type: String, default: "" },
  background: { type: String, default: "" },
  curator: { type: String, default: "" },
  tags: { type: Array, default: () => [] },
  books: { type: Array, default: () => [] }
})
defineEmits(["close", "open"])
</script>
