<template>

    <BaseSection
        title="Most Borrowed"
        subtitle="Popular choices among readers this week"
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
            title="Nothing on this shelf yet"
            description="Try a different filter or search term."
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