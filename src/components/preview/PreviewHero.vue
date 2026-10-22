<template>
  <section
    ref="hero"
    class="preview-hero"
    :style="heroStyle"
  >
    <div
      class="veil"
      aria-hidden="true"
    ></div>

    <div class="content">

      <!-- Category -->
      <AppChip
        v-if="category"
        ref="chip"
        class="category"
      >
        <span v-if="immersive" class="material-symbols-rounded" aria-hidden="true">auto_awesome</span>
        {{ category }}
      </AppChip>

      <!-- Title -->
      <h1
        ref="title"
        class="title"
      >
        {{ title }}
      </h1>

      <!-- Subtitle -->
      <p
        v-if="subtitle"
        ref="subtitle"
        class="subtitle"
      >
        <slot name="subtitle">{{ subtitle }}</slot>
      </p>

      <!-- Metadata -->
      <div
        v-if="$slots.meta"
        ref="meta"
        class="meta"
      >
        <slot name="meta" />
      </div>

      <!-- Extra Content -->
      <div
        v-if="$slots.default"
        ref="extra"
        class="extra"
      >
        <slot />
      </div>

    </div>
  </section>
</template>

<script setup>

import { computed } from "vue"
import AppChip from "../common/AppChip.vue"

const props = defineProps({
  immersive: { type: Boolean, default: false },

  title: {
    type: String,
    required: true
  },

  subtitle: {
    type: String,
    default: ""
  },

  category: {
    type: String,
    default: ""
  },

  background: {
    type: String,
    default: ""
  },

  height: {
    type: String,
    default: "280px"
  }

})

const heroStyle = computed(() => {

  const fallback =
    "radial-gradient(120% 80% at 85% 0%, rgba(198, 172, 220, .10), transparent 55%), " +
    "linear-gradient(165deg, var(--bg-secondary) 0%, var(--bg-tertiary) 100%)"

  return {

    backgroundImage: props.background
      ? `url(${props.background}), ${fallback}`
      : fallback,

    minHeight: props.height

  }

})

</script>

<style scoped>

.preview-hero{

  position:relative;

  overflow:hidden;

  border-radius:var(--radius-xl);

  background-size:cover;

  background-position:center;

  border:1px solid var(--border-subtle);

}



.veil{

  position:absolute;

  inset:0;

  background:linear-gradient(

    180deg,

    rgba(13,12,16,.05) 0%,

    rgba(13,12,16,.38) 52%,

    rgba(13,12,16,.84) 100%

  );

}



.content{

  position:relative;

  z-index:2;

  min-height:inherit;

  display:flex;

  flex-direction:column;

  justify-content:flex-end;

  gap:1.05rem;

  padding:2rem;

}



.category{

  align-self:flex-start;

}



.title{

  margin:0;

  max-width:30ch;

  font-family:var(--font-display);

  font-size:clamp(2rem,5vw,2.85rem);

  font-weight:500;

  line-height:1.08;

  letter-spacing:-0.015em;

  color:var(--text-primary);

  text-wrap:balance;

  text-shadow:0 2px 24px rgba(0,0,0,.35);

}



.subtitle{

  margin:0;

  max-width:36rem;

  font-size:1rem;

  line-height:1.7;

  color:rgba(243,240,234,.85);

}



.meta{

  display:flex;

  flex-wrap:wrap;

  align-items:center;

  gap:.6rem;

  color:rgba(243,240,234,.68);

  font-size:.86rem;

}



.meta span + span::before{

  content:"·";

  margin-right:.6rem;

  opacity:.5;

}



.extra{

  display:flex;

  flex-wrap:wrap;

  gap:.5rem;

}



@media (max-width: 640px){

  .preview-hero{

    border-radius:22px;

  }

  .content{

    padding:1.5rem;

  }

  .title{

    font-size:1.95rem;

  }

}

</style>
