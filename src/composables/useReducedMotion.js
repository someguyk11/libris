import { computed, ref, watch } from "vue"
const query = typeof window !== "undefined" ? window.matchMedia("(prefers-reduced-motion: reduce)") : null
const systemReduced = ref(Boolean(query?.matches))
let saved = true
try { saved = localStorage.getItem("libris-animations") !== "off" } catch { /* Use an in-memory preference. */ }
export const animationsEnabled = ref(saved)
export const animationStorageError = ref(false)
const reduced = computed(() => systemReduced.value || !animationsEnabled.value)
query?.addEventListener("change", event => { systemReduced.value = event.matches })
export function setAnimationsEnabled(value) { animationsEnabled.value = Boolean(value) }
watch(reduced, value => {
  if (typeof document !== "undefined") document.documentElement.dataset.motion = value ? "off" : "on"
}, { immediate: true })
watch(animationsEnabled, value => {
  try { localStorage.setItem("libris-animations", value ? "on" : "off"); animationStorageError.value = false } catch { animationStorageError.value = true }
})
export default function useReducedMotion() { return reduced }
