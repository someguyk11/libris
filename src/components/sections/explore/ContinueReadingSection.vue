<template>

    <BaseSection
        title="Continue Reading"
        action-text="See All"
        @action="$emit('see-all')"
    >

        <div
            v-if="books.length"
            ref="rail"
            class="continue-rail"
            @scroll.passive="onScroll"
        >

            <BookCard
                v-for="book in books"
                :key="book.id || book.title"
                v-bind="book"
                class="continue-rail__item"
                @click="$emit('open', book)"
            />

        </div>

        <div
            v-if="books.length && dotCount > 1"
            class="continue-rail__dots"
            aria-hidden="true"
        >

            <span
                v-for="index in dotCount"
                :key="index"
                class="dot"
                :class="{ 'dot--active': index === activeDot + 1 }"
            ></span>

        </div>

        <EmptyState
            v-else
            title="Nothing in progress"
            description="Borrow a book to begin your next reading journey."
        />

    </BaseSection>

</template>

<script setup>

import {
    ref,
    onMounted,
    onBeforeUnmount
} from "vue"

import BaseSection from "@/components/sections/BaseSection.vue";
import BookCard from "@/components/book/BookCard.vue";
import EmptyState from "@/components/common/EmptyState.vue";
import useReveal from "@/composables/useReveal";
import { gsap } from "@/animations/setup";

const props = defineProps({

    books: {
        type: Array,
        default: () => []
    }

});

defineEmits([
    "open",
    "see-all"
]);

const rail = ref(null)

const activeDot = ref(0)

const dotCount = ref(0)

const { stagger } = useReveal()

let gap = 12
let cardWidth = 0
let visibleCount = 0

function animateProgress() {

    const fills = rail.value?.querySelectorAll(".progress-fill")

    if (!fills || !fills.length) return

    fills.forEach((fill) => {

        gsap.fromTo(
            fill,
            { scaleX: 0 },
            {
                scaleX: 1,
                duration: 0.6,
                ease: "power3.out",
                delay: 0.15,
                transformOrigin: "left center"
            }
        )

    })

}

function measurePages() {

    const railEl = rail.value

    if (!railEl) return

    const child = railEl.firstElementChild

    if (!child) return

    gap = parseFloat(getComputedStyle(railEl).gap) || 12

    const style = getComputedStyle(child)

    const margin = parseFloat(style.marginInlineEnd) || 0

    cardWidth = child.getBoundingClientRect().width + gap + margin

    const available = railEl.clientWidth + gap

    visibleCount = Math.max(1, Math.round(available / cardWidth))

    const slides = Math.max(1, props.books.length - visibleCount)

    dotCount.value = Math.ceil(slides / visibleCount) + 1

    activeDot.value = Math.min(
        dotCount.value - 1,
        Math.round(railEl.scrollLeft / (visibleCount * cardWidth))
    )

}

function onScroll() {

    const railEl = rail.value

    if (!railEl || !cardWidth) return

    activeDot.value = Math.min(
        dotCount.value - 1,
        Math.round(railEl.scrollLeft / (visibleCount * cardWidth))
    )

}

onMounted(() => {

    measurePages()

    stagger(rail.value, null, {
        y: 12,
        duration: 0.4,
        stagger: 0.06,
        onComplete: animateProgress
    })

    window.addEventListener("resize", measurePages)

})

onBeforeUnmount(() => {

    window.removeEventListener("resize", measurePages)

})

</script>

<style scoped>

.continue-rail{

    display:flex;

    gap:12px;

    overflow-x:auto;

    scroll-snap-type:x mandatory;

    padding: 0.25rem 0.25rem 0.5rem;

    scrollbar-width:none;

}

.continue-rail::-webkit-scrollbar{

    display:none;

}

.continue-rail__item{

    flex:0 0 auto;

    scroll-snap-align:start;

    margin-inline-end:0.3em;

    --book-card-width:128px;

    --cover-width:112px;

}

.continue-rail__dots{

    display:flex;

    justify-content:center;

    align-items:center;

    gap:7px;

    padding-top:1rem;

}

.dot{

    width:4px;

    height:4px;

    border-radius:999px;

    background:rgba(243,240,234,.45);

    opacity:.5;

    transition: width .24s cubic-bezier(.22,1,.36,1),
        opacity .24s cubic-bezier(.22,1,.36,1),
        background-color .24s cubic-bezier(.22,1,.36,1);

}

.dot--active{

    width:15px;

    background:var(--accent);

    opacity:1;

}

@media (min-width: 768px){

    .continue-rail__item{

        --book-card-width:156px;

        --cover-width:138px;
    }

    .continue-rail{

        gap:16px;
    }

}

</style>