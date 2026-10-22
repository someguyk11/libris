<template>

    <BaseSection
        title="New Arrivals"
        subtitle="Fresh on the shelves this month"
        action-text="See All"
        @action="$emit('view-all')"
    >

        <BookCarousel
            v-if="books.length"
            :books="books"
            @select="$emit('open', $event)"
        />

        <EmptyState
            v-else
            title="Nothing here yet"
            description="New books land here as soon as they arrive."
        />

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import BookCarousel from "@/components/book/BookCarousel.vue"
import EmptyState from "@/components/common/EmptyState.vue"

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