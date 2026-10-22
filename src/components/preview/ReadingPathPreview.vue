<template>
  <BottomSheet :open="open" variant="editorial" :label="title" @close="$emit('close')">
    <div class="editorial-preview editorial-preview--path">
      <PreviewHero class="editorial-hero" immersive category="Reading Path" :title="title" :background="background || libraryArt" height="350px">
        <p v-if="tags.length" class="editorial-tags">{{ tags.join(' · ') }}</p>
      </PreviewHero>
      <div class="editorial-body">
        <blockquote v-if="quote" class="editorial-quote editorial-quote--path">
          <span class="editorial-quote__star material-symbols-rounded" aria-hidden="true">auto_awesome</span>
          <p>{{ quote }}</p>
        </blockquote>
        <section v-if="books.length" class="editorial-books" :class="{ 'editorial-books--expanded': showAllBooks }">
          <SectionHeader title="Included Books" :action="showAllBooks ? 'Show less' : 'See all'" @action="showAllBooks = !showAllBooks" />
          <BookCarousel :books="books" cover-only :show-bookmarks="false" @select="selectBook" />
        </section>
        <dl class="editorial-path-meta">
          <div><span class="material-symbols-rounded" aria-hidden="true">menu_book</span><dt>Books</dt><dd>{{ books.length }}</dd></div>
          <div><span class="material-symbols-rounded" aria-hidden="true">schedule</span><dt>Estimated</dt><dd>{{ duration || 'At your pace' }}</dd></div>
          <div><span class="material-symbols-rounded" aria-hidden="true">signal_cellular_alt</span><dt>Level</dt><dd>{{ level || 'All readers' }}</dd></div>
        </dl>
      </div>
      <footer class="editorial-footer">
        <AppButton full-width @click="$emit('start')"><span class="material-symbols-rounded" aria-hidden="true">auto_awesome</span>Start Path<span class="material-symbols-rounded" aria-hidden="true">east</span></AppButton>
      </footer>
    </div>
  </BottomSheet>
</template>

<script setup>
import { ref } from "vue"
import BottomSheet from "../common/BottomSheet.vue"
import PreviewHero from "./PreviewHero.vue"
import BookCarousel from "../book/BookCarousel.vue"
import AppButton from "../common/AppButton.vue"
import SectionHeader from "../common/SectionHeader.vue"
import { useNav } from "@/composables/useNav"
import libraryArt from "@/assets/library.png"
import "@/styles/editorial-preview.css"

defineProps({
  open: Boolean,
  title: { type: String, default: "Reading Path" },
  background: String,
  duration: String,
  level: String,
  quote: String,
  books: { type: Array, default: () => [] },
  tags: { type: Array, default: () => [] }
})
const emit = defineEmits(["close", "start"])
const showAllBooks = ref(false)
const { openBook } = useNav()
function selectBook(book) { emit("close"); openBook(book) }
</script>
