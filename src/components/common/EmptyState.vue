<template>
  <div class="empty-state">
    <div v-if="icon" class="empty-icon">
      {{ icon }}
    </div>

    <div v-else-if="$slots.illustration" class="illustration">
      <slot name="illustration"></slot>
    </div>

    <div class="content">
      <h3 class="title">
        {{ title }}
      </h3>

      <p v-if="description" class="description">
        {{ description }}
      </p>
    </div>

    <div v-if="$slots.action" class="action">
      <slot name="action"></slot>
    </div>
  </div>
</template>

<script setup>
defineProps({
  icon: {
    type: String,
    default: ""
  },

  title: {
    type: String,
    required: true
  },

  description: {
    type: String,
    default: ""
  },

  variant: {
    type: String,
    default: "default"
  }
})
</script>

<style scoped>
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;

  padding: 3rem 1.5rem;
  gap: 1.25rem;
}

.empty-state.library {
  padding: 2rem 1rem;
  gap: 0.75rem;
}

.empty-icon {
  font-size: 3rem;
  opacity: 0.8;
}

.empty-state.library .empty-icon {
  font-size: 2.5rem;
  opacity: 0.5;
}

.illustration {
  display: flex;
  justify-content: center;
  align-items: center;
}

.content {
  max-width: 320px;
}

.title {
  margin: 0;
  font-size: 1.25rem;
  font-weight: 600;
}

.empty-state.library .title {
  font-size: 1rem;
  font-weight: 500;
}

.description {
  margin: 0.5rem 0 0;
  font-size: 0.95rem;
  opacity: 0.65;
  line-height: 1.5;
}

.empty-state.library .description {
  font-size: 0.85rem;
  opacity: 0.55;
  margin: 0.25rem 0 0;
}

.action {
  margin-top: 0.5rem;
}

.empty-state.library .action {
  margin-top: 0.25rem;
}
</style>