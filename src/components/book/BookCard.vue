<template>
  <article
    class="book-card"
    :class="{ 'book-card--cover-only': coverOnly }"
    @click="$emit('click')"
  >
    <div
      class="book-card__bg"
      :style="{ backgroundImage: cover ? `url(${cover})` : 'none' }"
      aria-hidden="true"
    ></div>

    <div class="book-card__media">
      <BookCover
        :cover="cover"
        :title="title"
        :author="author"
        :size="coverSize"
        class="book-card__cover"
      />

      <button
        v-if="showBookmark"
        class="book-card__bookmark"
        aria-label="Add to library"
        @click.stop="$emit('bookmark')"
      >
        <span class="material-symbols-rounded">bookmark_add</span>
      </button>
    </div>

    <div v-if="!coverOnly" class="book-card__body">
      <BookMeta
        :title="title"
        :author="author"
        :details="details"
        class="book-card__meta"
      />

      <BookProgress
        v-if="progress !== null"
        :progress="progress"
        :label="progressLabel"
        class="book-card__progress"
      />

      <button
        v-if="actionLabel"
        class="book-card__action"
        @click.stop="$emit('action')"
      >
        {{ actionLabel }}
      </button>

      <slot name="extra" />
    </div>
  </article>
</template>

<script setup>
import BookCover from "./BookCover.vue"
import BookMeta from "./BookMeta.vue"
import BookProgress from "./BookProgress.vue"

defineProps({
  coverOnly: { type: Boolean, default: false },
  cover: {
    type: String,
    default: ""
  },

  title: {
    type: String,
    required: true
  },

  author: {
    type: String,
    default: ""
  },

  details: {
    type: Array,
    default: () => []
  },

  progress: {
    type: Number,
    default: null
  },

  progressLabel: {
    type: String,
    default: "Reading progress"
  },

  coverSize: {
    type: String,
    default: "medium"
  },

  actionLabel: {
    type: String,
    default: ""
  },

  showBookmark: {
    type: Boolean,
    default: true
  }
})

defineEmits([
  "click",
  "bookmark",
  "action"
])
</script>

<style scoped>
.book-card {
  position: relative;

  display: flex;
  flex-direction: column;

  flex-shrink: 0;
  overflow: hidden;

  width: var(--book-card-width, 140px);
  height: var(--book-card-height, 216px);

  background: var(--surface-1);

  backdrop-filter: blur(18px) saturate(1.15);
  -webkit-backdrop-filter: blur(18px) saturate(1.15);

  border: 1px solid var(--border-soft);

  border-radius: 10px;

  cursor: pointer;

  transition:
    transform var(--transition-base),
    border-color var(--transition-base),
    box-shadow var(--transition-base);
}

/* Glassy full-card background: the same cover, blurred and
   faded so it melts into the dark page behind the card. */
.book-card__bg {
  position: absolute;
  inset: -24px;
  z-index: 0;

  background-size: cover;
  background-position: center;
  background-repeat: no-repeat;

  filter: blur(24px) saturate(1.2);
  opacity: 0.5;
}

.book-card__bg::after {
  content: "";
  position: absolute;
  inset: 0;

  background: linear-gradient(
    to top,
    rgba(13, 12, 16, 0.92) 0%,
    rgba(24, 23, 29, 0.55) 55%,
    rgba(24, 23, 29, 0.35) 100%
  );
}

/* Cover area — flexible: takes whatever room the text below needs */
.book-card__media {
  position: relative;
  z-index: 1;

  flex: 1;
  min-height: 0;
  overflow: hidden;

  margin-bottom: 0.2em;
}

.book-card--cover-only .book-card__media { margin-bottom: 0; }

.book-card__media :deep(.book-cover) {
  width: 100%;
  height: 100%;
  border-radius: 0;
  border: none;
  box-shadow: none;
  aspect-ratio: auto;
}

.book-card__bookmark {
  position: absolute;
  top: 0.5rem;
  right: 0.5rem;
  z-index: 2;

  display: grid;
  place-items: center;

  width: 28px;
  height: 28px;

  border: none;
  background: rgba(13, 12, 16, 0.7);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
  border-radius: 8px;

  color: var(--text-secondary);
  opacity: 0.8;

  cursor: pointer;

  transition:
    background var(--transition-fast),
    color var(--transition-fast),
    transform var(--transition-fast),
    opacity var(--transition-fast);
}

.book-card__bookmark:hover {
  background: var(--accent);
  color: var(--bg-primary);
  opacity: 1;
  transform: scale(1.1);
}

.book-card__bookmark:active {
  transform: scale(0.95);
  transition: transform 0.1s ease;
}

.book-card__bookmark .material-symbols-rounded {
  font-size: 1.1rem;
}

.book-card__body {
  position: relative;
  z-index: 1;

  flex: 0 0 auto;
  min-height: 0;

  display: flex;
  flex-direction: column;
  gap: 0.2rem;

  padding: 0 0.5rem 0.45rem;
}

.book-card__meta :deep(.book-meta) {
  gap: 0;
}

.book-card__meta :deep(.title) {
  font-family: var(--font-display);
  font-size: 0.9rem;
  font-weight: 600;
  -webkit-line-clamp: 2;
}

.book-card__meta :deep(.author) {
  font-size: 0.72rem;
}

.book-card__progress :deep(.book-progress) {
  gap: 0.2rem;
}

.book-card__progress :deep(.value) {
  font-size: 0.68rem;
}

.book-card__action {
  margin-top: 0.25rem;
  padding: 0.35rem 0.75rem;

  border: none;
  border-radius: var(--radius-sm);
  background: var(--accent);
  color: var(--bg-primary);

  font-family: var(--font-body);
  font-size: 0.68rem;
  font-weight: 600;
  letter-spacing: 0.02em;

  cursor: pointer;

  transition:
    background var(--transition-fast),
    transform var(--transition-fast),
    box-shadow var(--transition-fast);
}

.book-card__action:hover {
  background: var(--accent-hover);
  box-shadow: 0 0 12px var(--accent-glow);
  transform: translateY(-1px);
}

.book-card__action:active {
  transform: scale(0.98);
  transition: transform 0.1s ease;
}

.book-card:hover {
  transform: translateY(-4px);

  border-color: var(--border-strong);

  box-shadow: 0 10px 26px rgba(0, 0, 0, 0.32), 0 0 0 1px var(--accent-glow);

  transition:
    transform 0.24s cubic-bezier(0.22, 1, 0.36, 1),
    border-color 0.24s cubic-bezier(0.22, 1, 0.36, 1),
    box-shadow 0.24s cubic-bezier(0.22, 1, 0.36, 1);
}

.book-card:active {
  transform: scale(0.98);

  transition: transform 0.1s ease;
}
</style>
