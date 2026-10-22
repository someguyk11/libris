<template>

    <BaseSection
        title="Curator Collections"
        subtitle="Thoughtfully selected books around meaningful themes"
        action-text="Explore"
        @action="$emit('view-all')"
    >

        <ScrollRail :gap="'var(--card-gap)'">

            <CuratorCard
                v-for="collection in collections"
                :key="collection.id"
                class="collections__item"
                :title="collection.title"
                :description="collection.description"
                :type="collection.type"
                @click="$emit('open', collection)"
            />

        </ScrollRail>

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import CuratorCard from "@/components/collection/CuratorCard.vue"

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

    flex:0 0 230px;

    scroll-snap-align:start;

}

@media (min-width: 768px){

    .collections__item{

        flex-basis:270px;

    }

}

</style>