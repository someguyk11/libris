<template>
  <article
    class="reading-path-card"
    :style="cardStyle"
    @click="$emit('click')"
  >
    <div
      class="reading-path-card__veil"
      aria-hidden="true"
    ></div>

    <div class="reading-path-card__content">

      <div
        v-if="previewBooks.length"
        class="reading-path-card__preview"
        aria-hidden="true"
      >
        <BookCover
          v-for="(book, index) in previewBooks.slice(0, 3)"
          :key="book.id || index"
          :cover="book.cover"
          :title="book.title"
          size="small"
        />
      </div>

      <div class="reading-path-card__info">
        <span
          v-if="steps"
          class="reading-path-card__steps"
        >
          {{ steps }} steps
        </span>

        <h3 class="reading-path-card__title">
          {{ title }}
        </h3>

        <p
          v-if="description"
          class="reading-path-card__desc"
        >
          {{ description }}
        </p>

        <div
          v-if="bookCount"
          class="reading-path-card__meta"
        >
          {{ bookCount }} books
        </div>
      </div>
    </div>
  </article>
</template>

<script setup>
import { computed } from "vue"

import BookCover from "../book/BookCover.vue"

const props = defineProps({
  title: {
    type: String,
    required: true
  },

  description: {
    type: String,
    default: ""
  },

  type: {
    type: String,
    default: "default"
  },

  image: {
    type: String,
    default: ""
  },

  previewBooks: {
    type: Array,
    default: () => []
  },

  steps: {
    type: Number,
    default: 0
  },

  bookCount: {
    type: Number,
    default: 0
  }
})

defineEmits([
  "click"
])

const art = {
  philosophy:
    "linear-gradient(160deg,#23283B 0%,#1B1E2B 55%,#141823 100%)",

  science:
    "linear-gradient(160deg,#202741 0%,#191D2E 55%,#131726 100%)",

  history:
    "linear-gradient(160deg,#2B2620 0%,#201C18 55%,#16130F 100%)",

  default:
    "linear-gradient(160deg,#26203A 0%,#1C1928 55%,#16151F 100%)"
}

const cardStyle = computed(() => ({
  backgroundImage: props.image || art[props.type] || art.default
}))
</script>

<style scoped>
.reading-path-card {
  position: relative;
  overflow: hidden;

  display: flex;
  flex-direction: column;
  justify-content: flex-end;

  aspect-ratio: 3 / 4;

  border-radius: var(--radius-lg);

  background-size: cover;
  background-position: center;

  cursor: pointer;

  transition:
    transform var(--transition-base),
    filter var(--transition-base);
}

.reading-path-card:hover {
  transform: translateY(-4px);
  filter: brightness(1.06);
}

.reading-path-card::before {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  border: 1px solid var(--border-subtle);
  pointer-events: none;
}

.reading-path-card__veil {
  position: absolute;
  inset: 0;

  background: linear-gradient(
    180deg,
    rgba(13, 12, 16, 0.04) 26%,
    rgba(13, 12, 16, 0.42) 62%,
    rgba(13, 12, 16, 0.85) 100%
  );
}

.reading-path-card__content {
  position: relative;
  z-index: 1;

  display: flex;
  flex-direction: column;
  gap: 1.1rem;

  padding: 1.35rem;
}

.reading-path-card__preview {
  display: flex;
  gap: 0.55rem;
}

.reading-path-card__preview > * {
  width: 84px;
}

.reading-path-card__info {
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
}

.reading-path-card__steps {
  font-size: 0.72rem;
  font-weight: 500;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--accent);
  opacity: 0.9;
}

.reading-path-card__title {
  margin: 0;
  font-family: var(--font-display);
  font-size: 1.5rem;
  font-weight: 500;
  line-height: 1.05;
  color: var(--text-primary);
}

.reading-path-card__desc {
  margin: 0;
  font-size: 0.85rem;
  line-height: 1.5;
  color: var(--text-secondary);

  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.reading-path-card__meta {
  font-size: 0.75rem;
  color: var(--text-muted);
}
</style>