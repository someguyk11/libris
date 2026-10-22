<template>
  <article
    class="curator-card"
    :style="cardStyle"
    @click="$emit('click')"
  >
    <div
      class="curator-card__veil"
      aria-hidden="true"
    ></div>

    <div class="curator-card__content">

      <h3 class="curator-card__title">
        {{ title }}
      </h3>

      <p
        v-if="description"
        class="curator-card__desc"
      >
        {{ description }}
      </p>

      <span
        v-if="collectionCount"
        class="curator-card__count"
      >
        {{ collectionCount }} books
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

  collectionCount: {
    type: Number,
    default: 0
  }
})

defineEmits([
  "click"
])

const art = {
  history:
    "linear-gradient(160deg,#2C2520 0%,#201B18 55%,#17130F 100%)",

  literature:
    "linear-gradient(160deg,#2A2036 0%,#1E1928 55%,#16121F 100%)",

  philosophy:
    "linear-gradient(160deg,#202A2E 0%,#192227 55%,#141A1E 100%)",

  poetry:
    "linear-gradient(160deg,#2B2138 0%,#201A2E 55%,#171321 100%)",

  default:
    "linear-gradient(160deg,#251F37 0%,#1C1928 55%,#151422 100%)"
}

const cardStyle = computed(() => ({
  backgroundImage: props.image || art[props.type] || art.default
}))
</script>

<style scoped>
.curator-card {
  position: relative;
  overflow: hidden;

  display: flex;
  flex-direction: column;
  justify-content: flex-end;

  aspect-ratio: 4 / 5;

  border-radius: var(--radius-lg);

  background-size: cover;
  background-position: center;

  cursor: pointer;

  transition:
    transform var(--transition-base),
    filter var(--transition-base);
}

.curator-card:hover {
  transform: translateY(-4px);
  filter: brightness(1.06);
}

.curator-card::before {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  border: 1px solid var(--border-subtle);
  pointer-events: none;
}

.curator-card__veil {
  position: absolute;
  inset: 0;

  background: linear-gradient(
    180deg,
    rgba(13, 12, 16, 0.02) 30%,
    rgba(13, 12, 16, 0.44) 60%,
    rgba(13, 12, 16, 0.86) 100%
  );
}

.curator-card__content {
  position: relative;
  z-index: 1;

  display: flex;
  flex-direction: column;
  gap: 0.45rem;

  padding: 1.35rem;
}

.curator-card__title {
  margin: 0;
  font-family: var(--font-display);
  font-size: 1.5rem;
  font-weight: 500;
  line-height: 1.05;
  color: var(--text-primary);
}

.curator-card__desc {
  margin: 0.2rem 0 0;
  font-size: 0.85rem;
  line-height: 1.55;
  color: var(--text-secondary);

  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.curator-card__count {
  margin-top: 0.35rem;
  font-size: 0.75rem;
  color: var(--text-muted);
}
</style>