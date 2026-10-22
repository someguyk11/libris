<template>
  <article
    class="collection-card"
    :style="cardStyle"
    @click="$emit('click')"
  >
    <div
      class="collection-card__veil"
      aria-hidden="true"
    ></div>

    <div class="collection-card__content">
      <div class="collection-card__info">
        <h3 class="collection-card__title">
          {{ title }}
        </h3>

        <p
          v-if="description"
          class="collection-card__desc"
        >
          {{ description }}
        </p>
      </div>

      <span
        v-if="count !== undefined && count !== null"
        class="collection-card__meta"
      >
        {{ count }} books
      </span>
    </div>
  </article>
</template>

<script setup>
import { computed } from "vue"

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

  count: {
    type: Number,
    default: 0
  }
})

defineEmits([
  "click"
])

const art = {
  fantasy:
    "linear-gradient(160deg,#28203A 0%,#1C1826 55%,#161320 100%)",

  science:
    "linear-gradient(160deg,#1F2433 0%,#191C26 55%,#141620 100%)",

  mystery:
    "linear-gradient(160deg,#292036 0%,#1D1A28 55%,#16141F 100%)",

  history:
    "linear-gradient(160deg,#26251D 0%,#1C1B16 55%,#161510 100%)",

  philosophy:
    "linear-gradient(160deg,#22272C 0%,#1B1F24 55%,#15181D 100%)",

  classics:
    "linear-gradient(160deg,#2B2633 0%,#211D2A 55%,#171420 100%)",

  biography:
    "linear-gradient(160deg,#273026 0%,#1D241B 55%,#151A11 100%)",

  default:
    "linear-gradient(160deg,#26203A 0%,#1C1826 55%,#15141C 100%)"
}

const cardStyle = computed(() => ({
  backgroundImage: props.image || art[props.type] || art.default
}))
</script>

<style scoped>
.collection-card {
  position: relative;
  overflow: hidden;

  display: flex;
  flex-direction: column;
  justify-content: space-between;

  aspect-ratio: 3 / 4;

  border-radius: var(--radius-lg);

  background-size: cover;
  background-position: center;

  cursor: pointer;

  transition:
    transform var(--transition-base),
    filter var(--transition-base);
}

.collection-card:hover {
  transform: translateY(-4px);
  filter: brightness(1.06);
}

.collection-card::before {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  border: 1px solid var(--border-subtle);
  pointer-events: none;
}

.collection-card__veil {
  position: absolute;
  inset: 0;

  background: linear-gradient(
    180deg,
    rgba(13, 12, 16, 0.05) 0%,
    rgba(13, 12, 16, 0.25) 40%,
    rgba(13, 12, 16, 0.65) 70%,
    rgba(13, 12, 16, 0.92) 100%
  );
}

.collection-card__content {
  position: relative;
  z-index: 1;

  display: flex;
  flex-direction: column;
  justify-content: space-between;

  height: 100%;
  padding: 1.35rem;
}

.collection-card__info {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;

  min-height: 0;
}

.collection-card__title {
  margin: 0;
  font-family: var(--font-display);
  font-size: 1.5rem;
  font-weight: 500;
  line-height: 1.05;
  letter-spacing: -0.01em;
  color: var(--text-primary);
}

.collection-card__desc {
  margin: 0;
  font-size: 0.85rem;
  line-height: 1.5;
  color: var(--text-secondary);

  display: -webkit-box;
  -webkit-line-clamp: 3;
  -webkit-box-orient: vertical;
  overflow: hidden;

  flex: 1;
}

.collection-card__meta {
  font-size: 0.75rem;
  font-weight: 500;
  letter-spacing: 0.03em;
  color: var(--text-tertiary);

  margin-top: auto;
  padding-top: 0.5rem;
}
</style>