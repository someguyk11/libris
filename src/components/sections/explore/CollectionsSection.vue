<template>

    <BaseSection
        title="Collections"
        subtitle="Browse books by genre"
        action-text="Explore"
        @action="$emit('view-all')"
    >

        <ScrollRail :gap="'var(--card-gap)'">

            <CollectionCard
                v-for="collection in collections"
                :key="collection.id"
                class="collections__item"
                :title="collection.title"
                :description="collection.description"
                :count="collection.bookCount"
                :type="collection.type"
                @click="$emit('open', collection)"
            />

        </ScrollRail>

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import CollectionCard from "@/components/collection/CollectionCard.vue"

defineProps({

    collections: {
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

.collections__item{

    flex:0 0 210px;

    scroll-snap-align:start;

}

@media (min-width: 768px){

    .collections__item{

        flex-basis:240px;

    }

}

</style>