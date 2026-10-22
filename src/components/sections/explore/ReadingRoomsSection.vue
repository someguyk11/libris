<template>

    <BaseSection
        title="Reading Rooms"
        subtitle="Step into a quiet space and read alongside others."
        action-text="Explore"
        @action="$emit('view-all')"
    >

        <ScrollRail :gap="'0'">

            <RoomCard
                v-for="room in rooms"
                :key="room.id"
                class="rooms__item"
                :title="room.title"
                :description="room.description"
                :image="room.background"
                :readers="room.readers"
                :time-left="room.timeLeft"
                @open="$emit('open', room)"
            />

        </ScrollRail>

    </BaseSection>

</template>

<script setup>

import BaseSection from "@/components/sections/BaseSection.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import RoomCard from "@/components/reading-rooms/RoomCard.vue"

defineProps({

    rooms: {
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

.rooms__item{

    flex:0 0 calc(100% - 0.3em);

    scroll-snap-align:start;

    aspect-ratio:5 / 4;

}

@media (min-width: 768px){

    .rooms__item{

        flex-basis:calc(50% - 0.3em);

    }

}

</style>