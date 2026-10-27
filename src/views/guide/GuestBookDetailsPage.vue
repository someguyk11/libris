<template>
  <section>
    <div class="guide-detail-heading"><BookCover :cover="book.cover" :title="book.title" :author="book.author" size="large" /><div><h1>{{ book.title }}</h1><p>{{ book.author }}</p><GuideAvailability :available="book.available" /><p class="guide-location"><GuideIcon name="pin" />{{ book.location }}</p></div></div>
    <dl class="guide-book-facts guide-surface"><div><GuideIcon name="book" /><dt>Edition</dt><dd>{{ book.edition }}</dd></div><div><GuideIcon name="info" /><dt>ISBN</dt><dd>{{ book.isbn }}</dd></div></dl>
    <p class="guide-description">{{ book.description }}</p>
    <p v-if="!book.available" class="guide-feedback" role="status">This book is currently unavailable. Guidance can start when a copy is available.</p>
    <AppButton full-width :disabled="!book.available" @click="$emit('setup')">Guide me there <GuideIcon name="arrow" /></AppButton>
  </section>
</template>
<script setup>
import BookCover from '@/components/book/BookCover.vue'
import AppButton from '@/components/common/AppButton.vue'
import GuideIcon from '@/components/guide/GuideIcon.vue'
import GuideAvailability from '@/components/guide/GuideAvailability.vue'
defineProps({ book: { type: Object, required: true } })
defineEmits(['setup'])
</script>
