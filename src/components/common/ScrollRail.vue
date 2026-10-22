<template>
  <div class="rail">
    <div
      ref="strip"
      class="rail__strip"
      :style="{ gap }"
      @scroll.passive="track"
    >
      <slot />
    </div>

    <div
      v-if="dotCount > 1"
      class="rail__dots"
      aria-hidden="true"
    >
      <span
        v-for="index in dotCount"
        :key="index"
        class="rail__dot"
        :class="{ 'rail__dot--active': index === active + 1 }"
      ></span>
    </div>
  </div>
</template>

<script setup>
import {
  ref,
  onMounted,
  onBeforeUnmount
} from "vue"

import useReveal from "@/composables/useReveal"

defineProps({
  gap: {
    type: String,
    default: "1.25rem"
  }
})

const strip = ref(null)

const active = ref(0)

const dotCount = ref(0)

const { stagger } = useReveal()

let cardWidth = 0
let visibleCount = 0

function measure() {

  const el = strip.value

  if (!el) return

  const child = el.firstElementChild

  if (!child) return

  const gap = parseFloat(getComputedStyle(el).gap) || 16

  const style = getComputedStyle(child)

  const margin = parseFloat(style.marginInlineEnd) || 0

  cardWidth = child.getBoundingClientRect().width + gap + margin

  const available = el.clientWidth + gap

  visibleCount = Math.max(1, Math.round(available / cardWidth))

  const slides = Math.max(1, el.children.length - visibleCount)

  dotCount.value = Math.ceil(slides / visibleCount) + 1

  active.value = Math.min(
    dotCount.value - 1,
    Math.round(el.scrollLeft / (visibleCount * cardWidth))
  )

}

function track() {

  const el = strip.value

  if (!el || !cardWidth) return

  active.value = Math.min(
    dotCount.value - 1,
    Math.round(el.scrollLeft / (visibleCount * cardWidth))
  )

}

onMounted(() => {

  measure()

  stagger(strip.value, null, { y: 12, duration: 0.4, stagger: 0.06 })

  window.addEventListener("resize", measure)

})

onBeforeUnmount(() => {

  window.removeEventListener("resize", measure)

})

</script>

<style scoped>

.rail {

  display: flex;

  flex-direction: column;

  gap: 1rem;

}

.rail__strip {

  display: flex;

  overflow-x: auto;

  scroll-snap-type: x mandatory;

  scrollbar-width: none;

  padding-bottom: 0.5rem;

}

.rail__strip::-webkit-scrollbar {

  display: none;

}

.rail__strip > * {

  flex-shrink: 0;

  scroll-snap-align: start;

  margin-inline-end: 0.3em;

}

.rail__dots {

  display: flex;

  justify-content: center;

  align-items: center;

  gap: 7px;

}

.rail__dot {

  width: 4px;

  height: 4px;

  border-radius: 999px;

  background: rgba(243, 240, 234, 0.45);

  opacity: 0.5;

  transition: width var(--motion-normal) var(--ease-standard),
    opacity var(--motion-normal) var(--ease-standard),
    background-color var(--motion-normal) var(--ease-standard);

}

.rail__dot--active {

  width: 15px;

  background: var(--accent-glow);

  opacity: 1;

}

</style>