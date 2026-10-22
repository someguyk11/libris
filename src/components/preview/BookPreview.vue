<template>
  <BottomSheet
    :open="open"
    @close="$emit('close')"
  >

    <div class="book-preview">

      <!-- Hero -->
      <PreviewHero
        category="Book"
        :title="title"
        :subtitle="author"
        :background="cover"
        height="360px"
      >
        <template #meta>

          <span v-if="pages">
            {{ pages }} Pages
          </span>

          <span v-if="published">
            {{ published }}
          </span>

          <span v-if="language">
            {{ language }}
          </span>

        </template>

        <AppChip
          v-if="genre"
        >
          {{ genre }}
        </AppChip>

      </PreviewHero>


      <!-- Description -->

      <section
        v-if="description"
        class="section"
      >

        <SectionHeader
          title="Description"
        />

        <p>
          {{ description }}
        </p>

      </section>


      <!-- Reading Progress -->

      <section
        v-if="progress !== null"
        class="section"
      >

        <SectionHeader
          title="Reading Progress"
        />

        <BookProgress
          :progress="progress"
        />

      </section>


      <!-- Actions -->

      <footer class="footer">

        <AppButton
          full-width
          @click="$emit('read')"
        >

          {{ progress > 0
              ? "Continue Reading"
              : "Start Reading"
          }}

        </AppButton>

      </footer>

    </div>

  </BottomSheet>
</template>

<script setup>

import BottomSheet from "../common/BottomSheet.vue"
import PreviewHero from "./PreviewHero.vue"

import AppButton from "../common/AppButton.vue"
import AppChip from "../common/AppChip.vue"
import SectionHeader from "../common/SectionHeader.vue"

import BookProgress from "../book/BookProgress.vue"

defineProps({

  open:{
    type:Boolean,
    default:false
  },

  title:{
    type:String,
    required:true
  },

  author:{
    type:String,
    default:"Unknown Author"
  },

  cover:{
    type:String,
    default:""
  },

  genre:{
    type:String,
    default:""
  },

  description:{
    type:String,
    default:""
  },

  pages:{
    type:Number,
    default:null
  },

  published:{
    type:[String,Number],
    default:null
  },

  language:{
    type:String,
    default:""
  },

  progress:{
    type:Number,
    default:null
  }

})

defineEmits([
  "close",
  "read"
])

</script>

<style scoped>

.book-preview{

  display:flex;

  flex-direction:column;

  gap:2.25rem;

}



.section{

  display:flex;

  flex-direction:column;

  gap:.85rem;

}



.section p{

  margin:0;

  line-height:1.85;

  color:var(--text-secondary);

  font-size:.97rem;

}



.footer{

  position:sticky;

  bottom:0;

  z-index:2;

  margin:0 -1.5rem;

  padding:1.25rem 1.5rem calc(1.25rem + env(safe-area-inset-bottom));

  background:linear-gradient(180deg, rgba(24,23,29,0) 0%, var(--surface) 42%);

}

</style>