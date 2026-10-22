<template>
  <div class="filter-chips" ref="chipsContainer">
    <button
      v-for="item in items"
      :key="item.value"
      class="filter-chip"
      :class="{ active: modelValue === item.value }"
      type="button"
      @click="$emit('update:modelValue', item.value)"
    >
      {{ item.label }}
    </button>
  </div>
</template>

<script setup>
defineProps({
  items: {
    type: Array,
    default: () => []
  },

  modelValue: {
    type: String,
    default: "all"
  }
})

defineEmits([
  "update:modelValue"
])
</script>

<style scoped>
.filter-chips {
  display: flex;
  gap: 0.5rem;
  overflow-x: auto;
  scrollbar-width: none;
  -ms-overflow-style: none;
  padding-bottom: 0.25rem;
}

.filter-chips::-webkit-scrollbar {
  display: none;
}

.filter-chip {
  flex-shrink: 0;
  height: 34px;
  padding: 0 13px;
  border-radius: 8px;
  border: 1px solid var(--border-soft);
  background: var(--surface-1);
  color: var(--text-secondary);
  font-family: var(--font-body);
  font-size: 0.8rem;
  font-weight: 600;
  letter-spacing: 0.02em;
  cursor: pointer;
  transition:
    background var(--transition-fast),
    border-color var(--transition-fast),
    color var(--transition-fast),
    box-shadow var(--transition-fast);
}

.filter-chip:hover {
  background: var(--surface-2);
  border-color: var(--border-default);
  color: var(--text-primary);
}

.filter-chip.active {
  background: var(--accent);
  border-color: var(--accent);
  color: var(--bg-primary);
  box-shadow: 0 0 0 1px var(--accent), 0 4px 16px rgba(179, 136, 255, 0.3);
}

.filter-chip:active {
  transform: scale(0.98);
  transition: transform 0.1s ease;
}
</style>