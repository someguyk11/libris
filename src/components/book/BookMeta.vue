<template>
  <div class="book-meta">
    <h3 class="title">
      {{ title }}
    </h3>

    <p
      v-if="author"
      class="author"
    >
      {{ author }}
    </p>

    <div
      v-if="details.length"
      class="details"
    >
      <span
        v-for="item in details"
        :key="item"
        class="detail-chip"
      >
        {{ item }}
      </span>
    </div>
  </div>
</template>

<script setup>
defineProps({
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
  }
})
</script>

<style scoped>


.title {
  margin: 0;

  font-family: var(--font-display);
  font-size: 1.05rem;
  font-weight: 600;
  margin-top: .5em;

  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.author {
  margin: 0;

  font-size: 0.78rem;
  color: var(--text-tertiary);
}

.details {
  display: flex;

  flex-wrap: wrap;

  gap: 0.4rem;
}

.detail-chip {
  padding: 0.25rem 0.55rem;

  border-radius: 999px;

  font-size: 0.75rem;

  background: rgba(255,255,255,0.08);

  opacity: 0.8;
}
</style>