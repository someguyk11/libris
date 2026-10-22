<template>

    <BaseSection
        title="Most Borrowed"
        subtitle="Popular choices among readers this week."
        action-text="Explore"
        @action="$emit('view-all')"
    >

        <BookCarousel
            :books="books"
            @select="$emit('open', $event)"
        />

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import BookCarousel from "@/components/book/BookCarousel.vue"

defineProps({

    books: {
        type: Array,
        default: () => []
    }

})

defineEmits([

    "open",

    "view-all"

])

</script>