<template>

    <BottomSheet
        :open="open"
        variant="editorial"
        :label="title"
        @close="$emit('close')"
    >

        <div class="room-preview">

            <!-- Hero -->

            <PreviewHero
                category="Reading Room"
                :title="title"
                :background="background || lakesideArt"
                height="340px"
            >

                <template #meta>

                    <span>{{ readers }} reading</span>

                    <span>{{ timeLeft }} left</span>

                </template>

            </PreviewHero>

            <!-- Description -->

            <section
                class="description"
            >

                <p>

                    {{ description }}

                </p>

            </section>

            <!-- Books -->

            <section
                v-if="books.length"
                class="books"
            >

                <SectionHeader
                    title="Books in this room"
                />

                <BookCarousel
                    :books="books"
                />

            </section>

            <!-- CTA -->

            <footer class="footer">

                <AppButton
                    full-width
                    @click="$emit('enter')"
                >

                    Enter Room

                </AppButton>

            </footer>

        </div>

    </BottomSheet>

</template>

<script setup>

import BottomSheet from "../common/BottomSheet.vue"
import PreviewHero from "../preview/PreviewHero.vue"

import BookCarousel from "../book/BookCarousel.vue"

import SectionHeader from "../common/SectionHeader.vue"
import AppButton from "../common/AppButton.vue"
import lakesideArt from "@/assets/twilight-lakeside.png"

defineProps({

    open:Boolean,

    title:String,

    background:String,

    description:String,

    readers:Number,

    timeLeft:String,

    books:{
        type:Array,
        default:()=>[]
    }

})

defineEmits([

    "close",

    "enter"

])

</script>

<style scoped>

.room-preview{

    display:flex;

    flex-direction:column;

    gap:2.25rem;

}

.description p{

    margin:0;

    max-width:38rem;

    line-height:1.85;

    color:var(--text-secondary);

    font-size:.98rem;

}

.books{

    display:flex;

    flex-direction:column;

    gap:1rem;

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
