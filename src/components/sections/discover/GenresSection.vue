<template>

    <BaseSection
        title="Explore by genre"
        subtitle="Browse the shelves by subject"
        action-text="See All"
        @action="$emit('view-all')"
    >

        <ScrollRail :gap="'var(--card-gap)'">

            <CollectionCard
                v-for="genre in genres"
                :key="genre.id"
                class="genres__item"
                :title="genre.title"
                :description="genre.description"
                :count="genre.bookCount"
                :type="genre.type"
                @click="$emit('open', genre)"
            />

        </ScrollRail>

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import CollectionCard from "@/components/collection/CollectionCard.vue"

defineProps({

    genres: {
        type: Array,
        default: () => []
    }

})

defineEmits([
    "open",
    "view-all"
])

</script>

<style scoped>

.genres__item{

    flex:0 0 210px;

    scroll-snap-align:start;

}

@media (min-width: 768px){

    .genres__item{

        flex-basis:240px;

    }

}

</style>