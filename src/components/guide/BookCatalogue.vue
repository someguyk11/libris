<template>
  <section aria-labelledby="guide-books-heading">
    <div class="guide-section-heading"><h2 id="guide-books-heading">Books</h2><div class="guide-layout" role="group" aria-label="Catalogue layout"><button v-for="mode in ['grid', 'list']" :key="mode" :aria-label="mode === 'grid' ? 'Grid view' : 'List view'" :aria-pressed="layout === mode" @click="$emit('update:layout', mode)"><GuideIcon :name="mode" /></button></div></div>
    <p class="guide-sr-only" role="status">{{ books.length }} {{ books.length === 1 ? 'book' : 'books' }} found</p>
    <div v-if="books.length" class="guide-catalogue" :class="{ 'is-list': layout === 'list' }">
      <button v-for="book in books" :key="book.id" class="guide-book-card guide-surface" :aria-label="`${book.title}, ${book.available ? 'Available' : 'Unavailable'}`" @click="$emit('select', book.id)">
        <BookCover :cover="book.cover" :title="book.title" :author="book.author" />
        <span class="guide-book-card__copy"><span class="guide-book-card__title">{{ book.title }}</span><GuideAvailability :available="book.available" /></span>
      </button>
    </div>
    <div v-else class="guide-empty" role="status"><GuideIcon name="search" /><h3>No books found</h3><p>Try a different title, author, or ISBN.</p><button class="guide-text-button" @click="$emit('clear')">Clear search</button></div>
  </section>
</template>
<script setup>
import BookCover from '@/components/book/BookCover.vue'
import GuideAvailability from './GuideAvailability.vue'
import GuideIcon from './GuideIcon.vue'
defineProps({ books: { type: Array, required: true }, layout: { type: String, default: 'grid' } })
defineEmits(['update:layout', 'select', 'clear'])
</script>
