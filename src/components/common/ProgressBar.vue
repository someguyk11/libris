<template>
  <div
    class="progress-wrapper"
    :class="{ compact }"
  >
    <div class="progress-track">
      <div
        class="progress-fill"
        :style="{ width: `${progress}%` }"
      ></div>
    </div>

    <span v-if="showLabel" class="progress-label">
      {{ progress }}%
    </span>
  </div>
</template>

<script setup>
defineProps({
  progress: {
    type: Number,
    default: 0,
    validator: (value) => value >= 0 && value <= 100
  },

  compact: {
    type: Boolean,
    default: false
  },

  showLabel: {
    type: Boolean,
    default: false
  }
})
</script>

<style scoped>
.progress-wrapper {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  width: 100%;
}

.progress-track {
  flex: 1;
  height: 8px;
  border-radius: 999px;
  overflow: hidden;
  background: rgba(255, 255, 255, 0.10);
}

.progress-fill {
  height: 100%;
  border-radius: inherit;
  background: var(--accent);
  box-shadow: 0 0 9px rgba(167, 139, 250, 0.45);

  transition: width 0.4s ease;
}

.progress-label {
  font-size: 0.85rem;
  opacity: 0.7;
}

.compact .progress-track {
  height: 4px;
}

.compact .progress-label {
  font-size: 0.75rem;
}
</style>