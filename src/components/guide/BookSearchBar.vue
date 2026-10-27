<template>
  <form class="guide-search" role="search" @submit.prevent="$emit('search')">
    <label class="guide-sr-only" for="guest-search">Search by book title, author, or ISBN</label>
    <input id="guest-search" :value="modelValue" type="search" placeholder="Search by book title, author, or ISBN" maxlength="200" @input="$emit('update:modelValue', $event.target.value)" />
    <button v-if="modelValue" type="button" class="guide-search__clear" aria-label="Clear search" @click="$emit('update:modelValue', '')"><GuideIcon name="close" /></button>
    <button type="submit" class="guide-search__submit" aria-label="Search books"><GuideIcon name="search" /></button>
  </form>
</template>
<script setup>
import GuideIcon from './GuideIcon.vue'
defineProps({ modelValue: { type: String, default: '' } })
defineEmits(['update:modelValue', 'search'])
</script>
