<template>

    <BaseSection
        title="Curated for you"
        subtitle="Picked to match your reading taste"
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
            title="No matches here yet"
            description="Try a different filter or search term to rediscover your shelf."
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