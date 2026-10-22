<template>
  <span class="chip"><slot /></span>
</template>

<script setup>
</script>

<style scoped>
.chip {
  display: inline-flex;
  align-items: center;
  padding: .4rem .75rem;
  border-radius: 999px;
  background: var(--accent-soft);
  border: 1px solid rgba(167, 139, 250, 0.22);
  color: var(--accent);
  font-family: var(--font-body);
  font-size: .68rem;
  font-weight: 700;
  letter-spacing: .08em;
  text-transform: uppercase;
  white-space: nowrap;
}
</style>