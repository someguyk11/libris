<template>
  <section class="book-carousel">

    <SectionHeader
      v-if="title"
      :title="title"
      :subtitle="subtitle"
      :action="action"
      @action="$emit('action')"
    />

    <div class="carousel-wrapper">
      <button
        class="scroll-button left"
        v-if="showControls"
        @click="scrollLeft"
      >
        ‹
      </button>

      <div
        ref="carousel"
        class="carousel"
        @scroll.passive="track"
      >
        <BookCard
          v-for="book in books"
          :key="book.id || book.title"
          v-bind="book"
          :cover-only="coverOnly"
          :show-bookmark="showBookmarks"
          @click="$emit('select', book)"
        />
      </div>

      <button
        class="scroll-button right"
        v-if="showControls"
        @click="scrollRight"
      >
        ›
      </button>
    </div>

    <div
      v-if="dotCount > 1"
      class="carousel__dots"
      aria-hidden="true"
    >
      <span
        v-for="index in dotCount"
        :key="index"
        class="carousel__dot"
        :class="{ 'carousel__dot--active': index === active + 1 }"
      ></span>
    </div>

  </section>
</template>


<script setup>
import {
  ref,
  onMounted,
  onBeforeUnmount
} from "vue"

import BookCard from "./BookCard.vue"
import SectionHeader from "../common/SectionHeader.vue"
import useReveal from "@/composables/useReveal"


defineProps({
  coverOnly: { type: Boolean, default: false },
  showBookmarks: { type: Boolean, default: true },

  title: {
    type: String,
    default: ""
  },

  subtitle: {
    type: String,
    default: ""
  },

  action: {
    type: String,
    default: ""
  },

  books: {
    type: Array,
    default: () => []
  },

  showControls: {
    type: Boolean,
    default: false
  }

})


defineEmits([
  "action",
  "select"
])


const carousel = ref(null)

const active = ref(0)

const dotCount = ref(0)

const { stagger } = useReveal()

let cardWidth = 0
let visibleCount = 0


function measurePages() {

  const el = carousel.value

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

  const el = carousel.value

  if (!el || !cardWidth) return

  active.value = Math.min(
    dotCount.value - 1,
    Math.round(el.scrollLeft / (visibleCount * cardWidth))
  )

}


function scrollLeft(){

  carousel.value?.scrollBy({
    left: -300,
    behavior: "smooth"
  })

}


function scrollRight(){

  carousel.value?.scrollBy({
    left: 300,
    behavior: "smooth"
  })

}


onMounted(() => {

  measurePages()

  stagger(carousel.value, null, { y: 12, duration: 0.4, stagger: 0.06 })

  window.addEventListener("resize", measurePages)

})

onBeforeUnmount(() => {

  window.removeEventListener("resize", measurePages)

})

</script>


<style scoped>

.book-carousel {

  display: flex;

  flex-direction: column;

  gap: 1rem;

}


.carousel-wrapper {

  position: relative;

}


.carousel {

  display: flex;

  gap: 1rem;

  overflow-x: auto;

  scroll-behavior: smooth;

  scrollbar-width: none;

  padding-bottom: 0.5rem;

}

.carousel > * {

  margin-inline-end: 0.3em;

}


.carousel::-webkit-scrollbar {

  display: none;

}


.carousel__dots {

  display: flex;

  justify-content: center;

  align-items: center;

  gap: 7px;

  padding-top: 4px;

}


.carousel__dot {

  width: 4px;

  height: 4px;

  border-radius: 999px;

  background: rgba(243, 240, 234, 0.45);

  opacity: 0.5;

  transition:
    width 240ms cubic-bezier(0.22, 1, 0.36, 1),
    opacity 240ms cubic-bezier(0.22, 1, 0.36, 1),
    background-color 240ms cubic-bezier(0.22, 1, 0.36, 1);

}


.carousel__dot--active {

  width: 15px;

  background: var(--accent-glow);

  opacity: 1;

}


.scroll-button {

  position: absolute;

  top: 45%;

  transform: translateY(-50%);

  z-index: 2;


  width: 36px;
  height: 36px;


  border-radius: 50%;

  border: none;

  cursor: pointer;

}


.scroll-button.left {

  left: -18px;

}


.scroll-button.right {

  right: -18px;

}


</style>
