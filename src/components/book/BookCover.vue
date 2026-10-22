<template>
  <div
    class="book-cover"
    :class="size"
  >
    <img
      v-if="cover && !imageFailed"
      :src="cover"
      :alt="title"
      class="cover-image"
      loading="lazy"
      @error="imageFailed = true"
    />

    <div
      v-else
      class="cover-placeholder"
    >
      <span class="placeholder-title">
        {{ title }}
      </span>

      <span
        v-if="author"
        class="placeholder-author"
      >
        {{ author }}
      </span>
    </div>
  </div>
</template>

<script setup>
import { ref, watch } from "vue"

const props = defineProps({
  cover: {
    type: String,
    default: ""
  },

  title: {
    type: String,
    default: "Unknown Book"
  },

  author: {
    type: String,
    default: ""
  },

  size: {
    type: String,
    default: "medium"
  }
})

const imageFailed = ref(false)

watch(() => props.cover, () => {
  imageFailed.value = false
})
</script>

<style scoped>
.book-cover {
  position: relative;

  overflow: hidden;

  border-radius: 10px;

  background:
    linear-gradient(160deg, #2A2438 0%, #1D1A27 55%, #17151F 100%);

  border: 1px solid var(--border-subtle);

  box-shadow: 0 2px 14px rgba(0, 0, 0, 0.3);

  aspect-ratio: 2 / 3;

  flex-shrink: 0;
}

.book-cover.small {
  width: 72px;
}

.book-cover.medium {
  width: var(--cover-width, 120px);
}

.book-cover.large {
  width: 180px;
}

.cover-image {
  width: 100%;
  height: 100%;

  object-fit: cover;

  display: block;
}

.cover-placeholder {
  position: relative;
  width: 100%;
  height: 100%;

  display: flex;
  flex-direction: column;

  justify-content: center;
  align-items: center;

  padding: 1rem;

  gap: 0.35rem;

  text-align: center;

  border: 1px solid var(--border-subtle);
  border-radius: inherit;
  background:
    radial-gradient(circle at 75% 18%, rgba(204, 157, 255, .18) 0 2%, transparent 3%),
    radial-gradient(circle at 28% 30%, rgba(255, 194, 140, .12) 0 1%, transparent 2%),
    linear-gradient(155deg, rgba(104, 67, 139, .34), rgba(28, 21, 42, .78) 48%, rgba(11, 9, 17, .96));
}

.cover-placeholder::after {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  background:
    linear-gradient(90deg, transparent 8%, rgba(229, 208, 255, .12) 8.5%, transparent 9%),
    radial-gradient(120% 70% at 50% 0%, rgba(198, 172, 220, 0.11), transparent 60%);
  pointer-events: none;
}

.placeholder-title {
  position: relative;
  z-index: 1;
  font-family: var(--font-display);
  font-size: 0.95rem;
  font-weight: 500;
  line-height: 1.15;
  color: var(--text-primary);
}

.placeholder-author {
  position: relative;
  z-index: 1;
  font-size: 0.72rem;
  color: var(--text-muted);
}
</style>
