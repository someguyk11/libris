<template>
  <div class="hero">
    <p class="hero__greeting">Good evening,</p>

    <h1 class="hero__title">Find your next world.</h1>

    <div class="hero__search">
      <SearchBar placeholder="Search a book, author, or a feeling" />
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from "vue"

import SearchBar from "@/components/navigation/SearchBar.vue"
import { animateHero } from "@/animations/hero"
import { gsap } from "@/animations/setup"
import useReducedMotion from "@/composables/useReducedMotion"

const greeting = ref(null)
const title = ref(null)
const search = ref(null)

const isReducedMotion = useReducedMotion()

let timeline = null

onMounted(() => {
  if (isReducedMotion.value) return

  timeline = animateHero({
    greeting: greeting.value,
    title: title.value,
    search: search.value
  })
})

onUnmounted(() => {
  timeline?.scrollTrigger?.kill()
  timeline?.kill()
  gsap.killTweensOf([greeting.value, title.value, search.value])
})
</script>

<style scoped>
.hero {
  display: flex;
  flex-direction: column;
  gap: 0.1rem;
  max-width: 52rem;
}

.hero__greeting,
.hero__title {
  margin: 0;
  font-family: var(--font-display);
  font-weight: 500;
  line-height: .85;
  letter-spacing: -0.02em;
  color: var(--text-primary);
  font-size: clamp(2.4rem, 5.12vw, 2.48rem);
}

.hero__greeting {
  margin-top: 6rem;
}

.hero__title {
  font-style: italic;
  color: var(--accent);
  white-space: nowrap;
}

.hero__search {
  margin-top: var(--space-6);
  margin-bottom: 1.75em;
  max-width: 34rem;
}

@media (min-width: 768px) {
  .hero__greeting,
  .hero__title {
    font-size: clamp(2.08rem, 3.36vw, 2.72rem);
  }

  .hero__greeting {
    margin-top: 13.4rem;
  }
}

@media (min-width: 1024px) {
  .hero__greeting {
    margin-top: 15rem;
  }
}
</style>