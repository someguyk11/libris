<template>

    <article
        class="room-card"
        :style="cardStyle"
        @click="$emit('open')"
    >

        <div
            class="room-card__veil"
            aria-hidden="true"
        ></div>

        <div class="room-card__content">

            <h3 class="room-card__title">

                {{ title }}

            </h3>

            <p
                v-if="description"
                class="room-card__desc"
            >

                {{ description }}

            </p>

            <div class="room-card__meta">

                <span
                    v-if="readers"
                    class="room-card__readers"
                >
                    <span class="room-card__dot" aria-hidden="true"></span>
                    {{ readers }} reading
                </span>

                <span
                    v-if="timeLeft"
                    class="room-card__time"
                >
                    {{ timeLeft }} left
                </span>

            </div>

        </div>

    </article>

</template>

<script setup>

import { computed } from "vue"

const props = defineProps({

    title: {

        type: String,

        required: true

    },

    description: {

        type: String,

        default: ""

    },

    image: {

        type: String,

        default: ""

    },

    readers: {

        type: Number,

        default: 0

    },

    timeLeft: {

        type: String,

        default: ""

    }

})

defineEmits([

    "open"

])

const art = [

    "linear-gradient(165deg,#262033 0%,#1D1A28 55%,#16141F 100%)",
    "linear-gradient(165deg,#202940 0%,#1A1E2C 55%,#131826 100%)",
    "linear-gradient(165deg,#2C2528 0%,#211C20 55%,#181419 100%)"

]

const cardStyle = computed(() => {

    if (props.image) {

        return {

            backgroundImage: `url(${props.image})`,

            backgroundSize: "cover",

            backgroundPosition: "center"

        }

    }

    const index = (props.title?.length || 0) % art.length

    const base = art[index] || art[0]

    const glow = "radial-gradient(120% 70% at 78% 0%, rgba(198,172,220,0.10), transparent 55%)"

    return {

        backgroundImage: `${glow}, ${base}`,

        backgroundSize: "cover, cover",

        backgroundPosition: "center, center"

    }

})

</script>

<style scoped>

.room-card{

    position:relative;

    overflow:hidden;

    display:flex;

    flex-direction:column;

    justify-content:flex-end;

    aspect-ratio: 3 / 4;

    border-radius:var(--radius-lg);

    cursor:pointer;

    text-align:left;

    transition:

        transform var(--transition-base),

        filter var(--transition-base);

}

.room-card:hover{

    transform:translateY(-4px);

    filter:brightness(1.06);

}

.room-card::before{

    content:"";

    position:absolute;

    inset:0;

    border-radius:inherit;

    border:1px solid var(--border-subtle);

    pointer-events:none;

}

.room-card__veil{

    position:absolute;

    inset:0;

    background:linear-gradient(

        180deg,

        rgba(13,12,16,0.03) 30%,

        rgba(13,12,16,0.45) 60%,

        rgba(13,12,16,0.88) 100%

    );

}

.room-card__content{

    position:relative;

    z-index:1;

    display:flex;

    flex-direction:column;

    gap:.5rem;

    padding:1.35rem;

}

.room-card__title{

    margin:0;

    font-family:var(--font-display);

    font-size:1.55rem;

    font-weight:500;

    line-height:1.05;

    color:var(--text-primary);

}

.room-card__desc{

    margin:0;

    font-size:.85rem;

    line-height:1.5;

    color:var(--text-secondary);

    display:-webkit-box;

    -webkit-line-clamp:2;

    -webkit-box-orient:vertical;

    overflow:hidden;

}

.room-card__meta{

    margin-top:.55rem;

    display:flex;

    align-items:center;

    gap:1rem;

    font-size:.78rem;

    color:var(--text-muted);

}

.room-card__readers{

    display:inline-flex;

    align-items:center;

    gap:.4rem;

    color:var(--text-secondary);

}

.room-card__dot{

    width:7px;

    height:7px;

    border-radius:50%;

    background:var(--accent);

    opacity:.85;

}

</style>