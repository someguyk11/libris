<template>
  <div class="collection-carousel">

    <div
      ref="track"
      class="carousel-track"
    >
      <BookCover
        v-for="(book, index) in books"
        :key="book.id || index"
        :cover="book.cover"
        :title="book.title"
        :author="book.author"
        size="small"
      />
    </div>


    <button
      v-if="showControls"
      class="next-button"
      @click="scrollNext"
    >
      →
    </button>

  </div>
</template>


<script setup>
import { ref } from "vue"

import BookCover from "../book/BookCover.vue"


defineProps({

  books: {
    type: Array,
    default: () => []
  },

  showControls: {
    type: Boolean,
    default: true
  }

})


const track = ref(null)


function scrollNext(){

  track.value?.scrollBy({
    left: 120,
    behavior: "smooth"
  })

}

</script>


<style scoped>

.collection-carousel {

  position: relative;

  width: 100%;

}


.carousel-track {

  display: flex;

  gap: 0.6rem;

  overflow-x: auto;

  scroll-behavior: smooth;

  scrollbar-width: none;

  padding: 0.25rem;

}

.carousel-track > * {

  margin-inline-end: 0.3em;

}


.carousel-track::-webkit-scrollbar {

  display: none;

}



.next-button {

  position: absolute;

  right: 0;

  top: 50%;

  transform: translateY(-50%);


  width: 32px;

  height: 32px;


  border-radius: 50%;

  border: none;


  cursor: pointer;


  background: rgba(0,0,0,0.45);

  backdrop-filter: blur(10px);

}


</style>