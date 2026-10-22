<template>

    <section
        ref="root"
        class="base-section"
        :style="{ '--section-gap': gap }"
    >

        <!-- Header -->

        <slot name="header">

            <SectionHeader
                v-if="showHeader"
                :title="title"
                :subtitle="subtitle"
                :action="showAction ? actionText : ''"
                @action="$emit('action')"
            />

        </slot>

        <!-- Content -->

        <div class="section-content">

            <slot />

        </div>

        <!-- Optional Footer -->

        <slot name="footer" />

    </section>

</template>

<script setup>

import { ref, onMounted } from "vue"

import SectionHeader from "../common/SectionHeader.vue"
import useReveal from "@/composables/useReveal"

defineProps({

    title: {
        type: String,
        default: ""
    },

    subtitle: {
        type: String,
        default: ""
    },

    actionText: {
        type: String,
        default: "See All"
    },

    showHeader: {
        type: Boolean,
        default: true
    },

    showAction: {
        type: Boolean,
        default: true
    },

    gap: {
        type: String,
        default: "1.5rem"
    }

})

defineEmits([
    "action"
])

const root = ref(null)

const { reveal } = useReveal()

onMounted(() => {
    reveal(root.value, { y: 16, duration: 0.5 })
})

</script>

<style scoped>

.base-section{

    display:flex;
    flex-direction:column;

    gap:var(--section-gap);

    margin-bottom:4rem;

}

.section-content{

    display:flex;
    flex-direction:column;

    gap:1rem;

    min-width:0;

}

</style>