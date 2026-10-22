<template>
  <div class="waitlist-status" :class="statusClass">
    <span class="status-dot" aria-hidden="true"></span>
    <span class="status-text">{{ status }}</span>
    <span v-if="position" class="status-position">#{{ position }} in queue</span>
  </div>
</template>

<script setup>
import { computed } from "vue"

defineProps({
  position: {
    type: [Number, String],
    default: null
  },
  status: {
    type: String,
    default: "Waiting for return"
  }
})

const statusClass = computed(() => {
  const s = (props.status || "").toLowerCase()
  if (s.includes("almost ready") || s.includes("ready")) return "almost-ready"
  if (s.includes("waiting")) return "waiting"
  return ""
})
</script>

<style scoped>
.waitlist-status {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;

  font-family: var(--font-body);
  font-size: 0.7rem;
  font-weight: 500;
  letter-spacing: 0.03em;
  text-transform: uppercase;
  color: var(--text-tertiary);
}

.status-dot {
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: currentColor;
  opacity: 0.6;
}

.status-position {
  margin-left: 0.2rem;
  opacity: 0.7;
}

.waitlist-status.waiting {
  color: var(--text-secondary);
}

.waitlist-status.waiting .status-dot {
  background: var(--text-secondary);
}

.waitlist-status.almost-ready {
  color: var(--accent);
}

.waitlist-status.almost-ready .status-dot {
  background: var(--accent);
  box-shadow: 0 0 6px var(--accent-glow), 0 0 12px var(--accent-glow);
  animation: pulse 2s ease-in-out infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 0.6; transform: scale(1); }
  50% { opacity: 1; transform: scale(1.2); }
}
</style>