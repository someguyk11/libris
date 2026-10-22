<template>
  <div class="section-header">
    <div class="section-content">
      <h2 class="title">
        {{ title }}
      </h2>

      <p v-if="subtitle" class="subtitle">
        {{ subtitle }}
      </p>
    </div>

    <button
      v-if="action"
      class="action-button"
      @click="$emit('action')"
    >
      {{ action }}
    </button>
  </div>
</template>

<script setup>
defineProps({
  title: {
    type: String,
    required: true
  },

  subtitle: {
    type: String,
    default: ""
  },

  action: {
    type: String,
    default: ""
  }
})

defineEmits([
  "action"
])
</script>

<style scoped>
.section-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 1rem;
  width: 100%;
}

.section-content {
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
}

.title {
  margin: 0;
  font-family: var(--font-body);
  font-size: 1.05rem;
  font-weight: 700;
  letter-spacing: -0.01em;
  line-height: 1.3;
  color: var(--text-primary);
  white-space: nowrap;
}

.subtitle {
  margin: 0;
  font-family: var(--font-body);
  font-size: 0.9rem;
  color: var(--text-muted);
  line-height: 1.5;
}

.action-button {
  display: inline-flex;
  align-items: center;
  align-self: flex-start;
  flex-shrink: 0;
  gap: 0.4rem;
  margin-top: 0.12rem;
  border: none;
  background: transparent;
  cursor: pointer;
  font-family: var(--font-body);
  font-size: 0.86rem;
  font-weight: 600;
  color: var(--accent);
  white-space: nowrap;
  padding: 0.15rem 0;

  transition:
    transform 0.18s cubic-bezier(0.22, 1, 0.36, 1),
    color 0.18s cubic-bezier(0.22, 1, 0.36, 1),
    text-shadow 0.18s cubic-bezier(0.22, 1, 0.36, 1);
}

.action-button:hover {
  color: var(--accent-hover);
  transform: translateX(2px);
  text-shadow: 0 0 8px var(--accent-glow);
}
</style>