<template>

    <BaseSection
        title="Recommended For You"
        subtitle="Picked based on your reading"
        action-text="See All"
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