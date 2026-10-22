<template>
  <button
    :class="['app-button', variant, size, { 'full-width': fullWidth }]"
    :disabled="disabled"
  >
    <slot />
  </button>
</template>

<script setup>
defineProps({
  variant: { default: "primary" },
  size: { default: "md" },
  fullWidth: Boolean,
  disabled: Boolean
})
</script>

<style scoped>
.app-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: .5rem;
  border: none;
  border-radius: var(--radius-md);
  padding: .85rem 1.25rem;
  cursor: pointer;
  font-family: var(--font-body);
  font-weight: 600;
  letter-spacing: .01em;
  line-height: 1;
  transition:
    background var(--transition-fast),
    color var(--transition-fast),
    transform var(--transition-fast),
    filter var(--transition-fast);
}

.app-button.full-width {
  width: 100%;
}

.app-button:disabled {
  opacity: .5;
  cursor: not-allowed;
}

.primary {
  background: var(--accent);
  color: var(--brand-950);
}

.primary:hover {
  background: var(--brand-200);
}

.primary:active {
  transform: scale(.985);
}

.secondary {
  background: transparent;
  color: var(--text-primary);
  border: 1px solid var(--border-strong);
}

.secondary:hover {
  background: var(--bg-secondary);
}

.secondary:active {
  transform: scale(.985);
}

.sm {
  font-size: .82rem;
}

.md {
  font-size: .92rem;
}

.lg {
  font-size: 1rem;
  padding: 1rem 1.4rem;
}
</style>