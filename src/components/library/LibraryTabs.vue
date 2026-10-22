<template>
  <div ref="tabsContainer" class="library-tabs" role="tablist" aria-label="My Library sections">
    <button
      v-for="tab in tabs"
      :key="tab.id"
      ref="tabButtons"
      class="library-tab"
      :class="{ active: modelValue === tab.id }"
      :aria-selected="modelValue === tab.id"
      role="tab"
      @click="$emit('update:modelValue', tab.id)"
    >
      <span class="material-symbols-rounded">{{ tab.icon }}</span>
      <small>{{ tab.label }}</small>
    </button>
  </div>
</template>

<script setup>
import { ref, watch, nextTick, onMounted, onUnmounted } from "vue"
import { gsap } from "@/animations/setup"
import useReducedMotion from "@/composables/useReducedMotion"

const props = defineProps({
  modelValue: { type: String, required: true }
})
defineEmits(["update:modelValue"])

const tabs = [
  { id: "waitlists", label: "Wait Lists", icon: "hourglass_empty" },
  { id: "borrowed", label: "Borrowed", icon: "local_mall" },
  { id: "reading", label: "Reading", icon: "menu_book" },
  { id: "collections", label: "Collections", icon: "layers" },
  { id: "saved", label: "Saved", icon: "bookmark" }
]
const tabsContainer = ref(null)
const tabButtons = ref([])
const isReducedMotion = useReducedMotion()
let activeTween = null

async function focusActive() {
  await nextTick()
  const index = tabs.findIndex((tab) => tab.id === props.modelValue)
  const button = tabButtons.value[index]
  if (!button) return
  button.scrollIntoView({
    behavior: isReducedMotion.value ? "auto" : "smooth",
    block: "nearest",
    inline: "center"
  })
  if (isReducedMotion.value) return
  activeTween?.kill()
  activeTween = gsap.fromTo(button, { scale: .97 }, { scale: 1, duration: .28, ease: "back.out(2)" })
}

watch(() => props.modelValue, focusActive)
onMounted(focusActive)
onUnmounted(() => activeTween?.kill())
</script>

<style scoped>
.library-tabs {
  display: grid;
  grid-template-columns: repeat(5, minmax(4.5rem, 1fr));
  gap: .2rem;
  width: 100%;
  overflow-x: auto;
  padding: .28rem;
  border: 1px solid var(--border-subtle);
  border-radius: 13px;
  background: rgba(17,14,27,.76);
  scrollbar-width: none;
}
.library-tabs::-webkit-scrollbar { display: none; }
.library-tab {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: .28rem;
  min-width: 4.5rem;
  min-height: 4rem;
  padding: .45rem .2rem;
  border: 0;
  border-radius: 10px;
  color: var(--text-tertiary);
  background: transparent;
  cursor: pointer;
  transition: color .2s ease, background .2s ease, box-shadow .2s ease;
}
.library-tab::after {
  content: "";
  position: absolute;
  left: 28%;
  right: 28%;
  bottom: .18rem;
  height: 2px;
  border-radius: 99px;
  background: var(--accent);
  opacity: 0;
  transform: scaleX(.35);
  transition: opacity .2s ease, transform .25s ease;
}
.library-tab .material-symbols-rounded { font-size: 1.35rem; font-variation-settings: "FILL" 0, "wght" 350; }
.library-tab small { font: 500 .67rem var(--font-body); white-space: nowrap; }
.library-tab:hover { color: var(--text-primary); background: rgba(255,255,255,.025); }
.library-tab.active {
  color: #c899ff;
  background: radial-gradient(circle at 50% 18%, rgba(160,86,255,.24), rgba(91,46,155,.22));
  box-shadow: inset 0 0 0 1px rgba(176,113,255,.12), 0 5px 18px rgba(68,31,120,.16);
}
.library-tab.active .material-symbols-rounded { font-variation-settings: "FILL" 1, "wght" 450; text-shadow: 0 0 12px rgba(177,110,255,.48); }
.library-tab.active::after { opacity: 1; transform: scaleX(1); }
@media (max-width: 390px) {
  .library-tabs { grid-template-columns: repeat(5, 4.4rem); }
}
</style>
