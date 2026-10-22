<template>
  <nav class="bottom-nav" aria-label="Primary navigation">
    <div class="bottom-nav__dock">
      <button
        v-for="item in items"
        :key="item.name"
        class="bottom-nav__item"
        :class="{ 'is-active': modelValue === item.name }"
        :aria-current="modelValue === item.name ? 'page' : undefined"
        @click="$emit('update:modelValue', item.name)"
      >
        <span class="bottom-nav__icon-wrap">
          <span ref="icons" class="material-symbols-rounded bottom-nav__icon">{{ item.icon }}</span>
        </span>
        <span class="bottom-nav__label">{{ item.label }}</span>
      </button>
    </div>
  </nav>
</template>

<script setup>
import { onMounted, onUnmounted, ref, watch } from "vue"
import { gsap } from "@/animations/setup"
import useReducedMotion from "@/composables/useReducedMotion"

const props = defineProps({ modelValue: { type: String, required: true } })
defineEmits(["update:modelValue"])

const items = [
  { name: "explore", label: "Explore", icon: "search" },
  { name: "discover", label: "Discover", icon: "explore" },
  { name: "library", label: "My Library", icon: "menu_book" },
  { name: "rooms", label: "Rooms", icon: "group" },
  { name: "profile", label: "Profile", icon: "person" }
]
const icons = ref([])
const isReducedMotion = useReducedMotion()
let tween = null

function animateActive(name) {
  if (isReducedMotion.value) return
  const index = items.findIndex(item => item.name === name)
  const icon = icons.value[index]
  if (!icon) return
  tween?.kill()
  tween = gsap.fromTo(icon, { y: 3, scale: .78, rotate: -6 }, { y: 0, scale: 1, rotate: 0, duration: .42, ease: "back.out(2.2)" })
}
watch(() => props.modelValue, animateActive)
onMounted(() => animateActive(props.modelValue))
onUnmounted(() => tween?.kill())
</script>

<style scoped>
.bottom-nav {
  position:fixed;
  z-index:100;
  left:0;
  right:0;
  bottom:0;
  padding:0 .75rem calc(.65rem + env(safe-area-inset-bottom));
  pointer-events:none;
}
.bottom-nav::before {
  content:"";
  position:absolute;
  inset:-1.6rem 0 0;
  z-index:-1;
  background:linear-gradient(0deg,rgba(5,4,10,.98) 28%,rgba(5,4,10,.72) 66%,transparent);
  pointer-events:none;
}
.bottom-nav__dock {
  display:grid;
  grid-template-columns:repeat(5,1fr);
  width:min(100%,39rem);
  height:4.4rem;
  margin:auto;
  padding:.28rem;
  border:1px solid rgba(236,226,248,.1);
  border-radius:20px;
  background:rgba(12,10,19,.88);
  box-shadow:0 18px 50px rgba(0,0,0,.44),inset 0 1px rgba(255,255,255,.035);
  backdrop-filter:blur(24px) saturate(1.25);
  pointer-events:auto;
}
.bottom-nav__item {
  position:relative;
  display:flex;
  flex-direction:column;
  align-items:center;
  justify-content:center;
  gap:.17rem;
  min-width:0;
  padding:.3rem .15rem;
  border:0;
  border-radius:15px;
  color:#85818f;
  background:transparent;
  font-family:var(--font-body);
  cursor:pointer;
  transition:color .22s ease,background .22s ease,transform .16s ease;
}
.bottom-nav__item:hover { color:#c8c1cf; background:rgba(255,255,255,.025); }
.bottom-nav__item:active { transform:scale(.96); }
.bottom-nav__icon-wrap {
  position:relative;
  display:grid;
  place-items:center;
  width:2rem;
  height:2rem;
  border-radius:10px;
  transition:background .25s ease,box-shadow .25s ease,transform .25s ease;
}
.bottom-nav__icon { font-size:1.35rem; line-height:1; font-variation-settings:"wght" 330; }
.bottom-nav__label { overflow:hidden; max-width:100%; font-size:.62rem; font-weight:550; line-height:1; text-overflow:ellipsis; white-space:nowrap; }
.bottom-nav__item.is-active { color:#d6b5ff; background:linear-gradient(180deg,rgba(130,72,199,.14),rgba(130,72,199,.035)); }
.bottom-nav__item.is-active::after {
  content:"";
  position:absolute;
  left:50%;
  bottom:.14rem;
  width:1.25rem;
  height:2px;
  border-radius:999px;
  background:linear-gradient(90deg,transparent,#b878ff,transparent);
  transform:translateX(-50%);
  box-shadow:0 0 10px rgba(184,120,255,.85);
}
.bottom-nav__item.is-active .bottom-nav__icon-wrap {
  color:#d5b1ff;
  background:radial-gradient(circle at 45% 35%,rgba(188,131,255,.23),rgba(106,56,169,.09));
  box-shadow:0 0 18px rgba(132,75,202,.16);
  transform:translateY(-1px);
}
@media (min-width:760px) {
  .bottom-nav { padding-bottom:1rem; }
  .bottom-nav__dock { height:4.6rem; border-radius:23px; }
  .bottom-nav__label { font-size:.66rem; }
}
@media (max-width:420px) {
  .bottom-nav { padding-inline:.45rem; }
  .bottom-nav__dock { height:4.25rem; border-radius:18px; }
  .bottom-nav__label { font-size:.57rem; }
}
</style>
