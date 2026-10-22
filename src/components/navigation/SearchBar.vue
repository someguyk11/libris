<template>
  <div class="search-wrapper">
    <span class="material-symbols-rounded search-icon">
      search
    </span>

    <input
      v-model="query"
      class="search-input"
      :placeholder="focused || query ? basePlaceholder : ''"
      @input="handleInput"
      @focus="onFocus"
      @blur="onBlur"
    />

    <span
      v-if="!focused && !query"
      class="search-placeholder"
      :class="{ 'is-fading': fading }"
      aria-hidden="true"
    >
      {{ suggestion }}
    </span>

    <button
      v-if="query"
      class="clear-button"
      aria-label="Clear search"
      @click="clearSearch"
    >
      <span class="material-symbols-rounded">close</span>
    </button>

    <button
      v-if="showFilter"
      class="filter-button"
      aria-label="Filters and settings"
      @click="$emit('filter-click')"
    >
      <span class="material-symbols-rounded">tune</span>
    </button>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from "vue"
import useReducedMotion from "@/composables/useReducedMotion"

const props = defineProps({
  placeholder: {
    type: String,
    default: "Search books, authors, genres..."
  },
  showFilter: {
    type: Boolean,
    default: true
  }
})

const emit = defineEmits([
  "search",
  "clear",
  "filter-click"
])

const query = ref("")
const focused = ref(false)
const fading = ref(false)
const suggestionIndex = ref(0)

const isReducedMotion = useReducedMotion()

const extraSuggestions = [
  "books like atomic habits",
  "books by J K Rowling",
  "quiet fantasy for slow evenings",
  "classic sci-fi that feels modern",
  "a book to finish in one sitting"
]

const suggestions = [props.placeholder, ...extraSuggestions]

const basePlaceholder = computed(() => props.placeholder)

const suggestion = computed(() => suggestions[suggestionIndex.value])

let cycleTimer = null
let swapTimer = null

function pickNext() {
  let next = suggestionIndex.value

  while (suggestions.length > 1 && next === suggestionIndex.value) {
    next = Math.floor(Math.random() * suggestions.length)
  }

  suggestionIndex.value = next
}

function schedule() {
  clearTimeout(cycleTimer)

  cycleTimer = setTimeout(() => {
    fading.value = true

    clearTimeout(swapTimer)
    swapTimer = setTimeout(() => {
      pickNext()
      fading.value = false
      schedule()
    }, 450)
  }, 3200)
}

function stopCycling() {
  clearTimeout(cycleTimer)
  clearTimeout(swapTimer)
  fading.value = false
}

function onFocus() {
  focused.value = true
  stopCycling()
}

function onBlur() {
  focused.value = false
  if (!isReducedMotion.value) schedule()
}

function handleInput() {
  emit("search", query.value)
}

function clearSearch() {
  query.value = ""
  emit("clear")
  emit("search", "")
}

onMounted(() => {
  if (!isReducedMotion.value) schedule()
})

onBeforeUnmount(stopCycling)
</script>

<style scoped>
.search-wrapper {
  position: relative;
  display: flex;
  align-items: center;
  gap: 0.6rem;

  width: 100%;

  height: calc(2.7rem + 10px);

  padding: 0 0.85rem;

  border-radius: 14px;

  background: rgba(13, 12, 16, 0.42);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);

  border: 1px solid var(--border-soft);

  transition:
    border-color 0.2s ease,
    background 0.2s ease,
    box-shadow 0.2s ease,
    transform 0.2s ease;
}

.search-wrapper:focus-within {
  background: rgba(13, 12, 16, 0.55);
  border-color: var(--accent-dim);
  transform: translateY(-1px);
  box-shadow: 0 0 0 1px rgba(179, 136, 255, 0.25), 0 8px 24px rgba(107, 83, 201, 0.14);
}

.search-icon {
  font-size: 1.2rem;
  color: var(--text-secondary);
}

.search-input {
  flex: 1;

  min-width: 0;

  border: none;
  outline: none;

  background: transparent;

  font-family: var(--font-body);
  font-size: 0.88rem;
  color: var(--text-primary);
}

.search-input::placeholder {
  color: rgba(243, 240, 234, 0.62);
  opacity: 1;
}

.search-placeholder {
  position: absolute;
  left: 2.65rem;
  right: 3rem;
  top: 50%;
  transform: translateY(-50%);

  pointer-events: none;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;

  font-family: var(--font-body);
  font-size: 0.88rem;
  color: rgba(243, 240, 234, 0.62);

  opacity: 1;
  transition: opacity 0.45s ease;
}

.search-placeholder.is-fading {
  opacity: 0;
}

.clear-button {
  display: grid;
  place-items: center;

  border: none;
  background: transparent;

  cursor: pointer;

  color: var(--text-secondary);
  opacity: 0.65;
}

.clear-button:hover {
  opacity: 1;
}

.clear-button .material-symbols-rounded {
  font-size: 1.15rem;
}

.filter-button {
  display: grid;
  place-items: center;

  width: 36px;
  height: 36px;

  border: none;
  background: transparent;
  border-radius: 10px;

  cursor: pointer;

  color: var(--text-secondary);
  opacity: 0.7;

  transition:
    background var(--transition-fast),
    color var(--transition-fast),
    opacity var(--transition-fast);
}

.filter-button:hover {
  background: var(--surface-2);
  color: var(--text-primary);
  opacity: 1;
}

.filter-button .material-symbols-rounded {
  font-size: 1.25rem;
}
</style>
