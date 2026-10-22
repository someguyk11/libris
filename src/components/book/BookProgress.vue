<template>
  <div class="book-progress">

    <span class="value">
      {{ progress }}%
    </span>

    <ProgressBar
      :progress="progress"
      compact
    />

  </div>
</template>

<script setup>
import ProgressBar from "../common/ProgressBar.vue"

defineProps({
  progress: {
    type: Number,
    default: 0
  },

  label: {
    type: String,
    default: ""
  }
})
</script>

<style scoped>
.book-progress {
  width: 100%;
}

.value {
  align-self: flex-start;

  font-family: var(--font-body);
  font-size: 0.78rem;
  font-weight: 700;
  letter-spacing: 0.02em;
  color: var(--accent);
}
</style>