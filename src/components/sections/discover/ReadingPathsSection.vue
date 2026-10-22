<template>

    <BaseSection
        title="Reading Paths"
        subtitle="Guided journeys through carefully ordered books"
        action-text="See All"
        @action="$emit('view-all')"
    >

        <ScrollRail :gap="'var(--card-gap)'">

            <ReadingPathCard
                v-for="path in paths"
                :key="path.id"
                class="paths__item"
                :title="path.title"
                :description="path.description"
                :steps="path.steps"
                :type="path.type"
                :preview-books="path.previewBooks"
                @click="$emit('open', path)"
            />

        </ScrollRail>

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import ReadingPathCard from "@/components/collection/ReadingPathCard.vue"

defineProps({

    paths: {
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

.paths__item{

    flex:0 0 210px;

    scroll-snap-align:start;

}

@media (min-width: 768px){

    .paths__item{

        flex-basis:240px;

    }

}

</style>