<template>
  <BottomSheet :open="open" variant="editorial" :label="title" @close="$emit('close')">
    <div class="editorial-preview editorial-preview--collection">
      <PreviewHero class="editorial-hero" immersive :category="category" :title="title" :subtitle="curator ? `Curated by ${curator}` : ''" :background="background || libraryArt" height="320px">
        <template #subtitle>Curated by <span class="editorial-curator">{{ curator }}</span></template>
      </PreviewHero>
      <div class="editorial-body">
        <blockquote class="editorial-quote editorial-quote--collection">
          <p>{{ quote || 'Some books entertain us. Others quietly become part of who we are.' }}</p>
          <cite v-if="curator">— {{ curator }}</cite>
        </blockquote>
        <section v-if="description" class="editorial-description">
          <h2>Why this collection</h2>
          <p>{{ description }}</p>
          <span class="editorial-description__art material-symbols-rounded" aria-hidden="true">history_edu</span>
        </section>
        <section v-if="books.length" class="editorial-books" :class="{ 'editorial-books--expanded': showAllBooks }">
          <SectionHeader title="Books you’ll discover" :action="showAllBooks ? 'Show less' : 'See all'" @action="showAllBooks = !showAllBooks" />
          <BookCarousel :books="books" cover-only :show-bookmarks="false" @select="selectBook" />
        </section>
        <section class="editorial-experience">
          <h2>Reading experience</h2>
          <div>
            <AppChip v-for="(experience, index) in readingExperience" :key="experience"><span class="material-symbols-rounded" aria-hidden="true">{{ experienceIcons[index % experienceIcons.length] }}</span>{{ experience }}</AppChip>
          </div>
        </section>
      </div>
      <footer class="editorial-footer">
        <AppButton full-width @click="$emit('open')"><span class="material-symbols-rounded" aria-hidden="true">auto_awesome</span>Explore Collection<span class="material-symbols-rounded" aria-hidden="true">east</span></AppButton>
      </footer>
    </div>
  </BottomSheet>
</template>

<script setup>
import { computed, ref } from "vue"
import BottomSheet from "../common/BottomSheet.vue"
import PreviewHero from "./PreviewHero.vue"
import BookCarousel from "../book/BookCarousel.vue"
import SectionHeader from "../common/SectionHeader.vue"
import AppButton from "../common/AppButton.vue"
import AppChip from "../common/AppChip.vue"
import { useNav } from "@/composables/useNav"
import libraryArt from "@/assets/library.png"
import "@/styles/editorial-preview.css"

const props = defineProps({
  open: Boolean,
  title: { type: String, required: true },
  category: { type: String, default: "Curator Collection" },
  curator: { type: String, default: "" },
  background: { type: String, default: "" },
  description: { type: String, default: "" },
  quote: { type: String, default: "" },
  books: { type: Array, default: () => [] },
  tags: { type: Array, default: () => [] }
})
const emit = defineEmits(["close", "open"])
const showAllBooks = ref(false)
const readingExperience = computed(() => props.tags.length ? props.tags : ["Reflective", "Slow-paced", "Thought-provoking"])
const experienceIcons = ["eco", "schedule", "auto_awesome"]
const { openBook } = useNav()
function selectBook(book) { emit("close"); openBook(book) }
</script>
