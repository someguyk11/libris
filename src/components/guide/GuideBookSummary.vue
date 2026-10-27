<template>
  <div class="guide-book-summary guide-surface">
    <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
    <div><h2>{{ book.title }}</h2><p>{{ book.author }}</p><p class="guide-location"><GuideIcon name="pin" />{{ book.aisle }} → {{ book.shelf }}</p></div>
  </div>
</template>
<script setup>
import BookCover from '@/components/book/BookCover.vue'
import GuideIcon from './GuideIcon.vue'
defineProps({ book: { type: Object, required: true } })
</script>
