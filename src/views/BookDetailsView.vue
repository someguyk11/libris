<template>
  <main ref="root" class="book-details">
    <div v-if="!book" class="detail-panel" role="status"><p>{{ loading ? 'Loading book…' : loadError || 'Book not found.' }}</p><button @click="closeBook">Back to catalogue</button><button v-if="loadError" @click="loadBook">Retry</button></div>
    <template v-else>
    <section class="book-hero" :style="{ '--hero-cover': `url(${book.cover || libraryArt})` }">
      <div class="book-hero__scene" :style="{ backgroundImage: `url(${libraryArt})` }" aria-hidden="true"></div>
      <header class="book-hero__actions">
        <button aria-label="Go back" @click="closeBook">
          <span class="material-symbols-rounded">arrow_back</span>
        </button>
        <div>
          <button aria-label="Share book" @click="shareBook">
            <span class="material-symbols-rounded">ios_share</span>
          </button>
          <button v-if="!apiMode" :aria-label="saved ? 'Remove bookmark' : 'Save book'" @click="saved = !saved; notify('Demo bookmark changed for this visit')">
            <span class="material-symbols-rounded" :class="{ filled: saved }">{{ saved ? 'bookmark' : 'bookmark_border' }}</span>
          </button>
        </div>
      </header>

      <div class="book-hero__content">
        <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="large" />
        <div class="book-hero__copy">
          <p>FEATURED EDITION</p>
          <h1>{{ book.title }}</h1>
          <h2>{{ book.author }}</h2>
          <div class="book-hero__chips">
            <span v-if="book.genre">{{ book.genre }}</span>
            <span v-if="!apiMode">{{ moodLabel }}</span>
          </div>
        </div>
      </div>
    </section>

    <div class="book-body">
      <section class="availability-panel detail-panel reveal-block">
        <div>
          <p class="detail-label">Availability</p>
          <strong class="availability"><i></i>{{ apiMode ? book.status || 'Not reported' : 'Available' }}</strong>
          <small>{{ apiMode ? 'Catalogue availability · not a reserved copy' : 'Ready to borrow' }}</small>
        </div>
        <span class="availability-panel__rule"></span>
        <div>
          <p class="detail-label">Location</p>
          <strong><span class="material-symbols-rounded">account_balance</span>{{ apiMode ? 'Registered shelf' : 'Central Library' }}</strong>
          <small>{{ apiMode ? book.shelf ? 'Shelf ' + book.shelf : 'Not mapped in the live catalogue' : 'Floor 2 · Aisle 7 · Shelf B' }}</small>
        </div>
      </section>

      <ReaderGuidance v-if="apiMode" :key="book.id" :book-id="book.backendId" />

      <section class="detail-panel reveal-block about-panel">
        <p class="detail-label">About the book</p>
        <p :class="{ 'is-clamped': !expanded }">{{ description }}</p>
        <button class="text-action" @click="expanded = !expanded">
          {{ expanded ? 'Show less' : 'Read more' }}
          <span class="material-symbols-rounded">{{ expanded ? 'expand_less' : 'expand_more' }}</span>
        </button>
      </section>

      <section class="detail-panel reveal-block facts-panel">
        <p class="detail-label">Edition details</p>
        <div class="facts">
          <span v-if="book.pages"><i class="material-symbols-rounded">menu_book</i>{{ book.pages }} pages</span>
          <span v-if="book.published"><i class="material-symbols-rounded">calendar_today</i>{{ book.published }}</span>
          <span v-if="book.language"><i class="material-symbols-rounded">translate</i>{{ book.language }}</span>
          <span v-if="book.publisher"><i class="material-symbols-rounded">corporate_fare</i>{{ book.publisher }}</span>
          <span v-if="book.isbn">ISBN {{ book.isbn }}</span>
        </div>
      </section>

      <section v-if="!apiMode" class="detail-panel reveal-block borrow-panel">
        <div>
          <span class="material-symbols-rounded">account_balance</span>
          <div>
            <p class="detail-label">Borrow from Central Library</p>
            <small>{{ borrowed ? 'Due in 14 days' : 'Free for 14 days' }}</small>
          </div>
        </div>
        <button :class="{ borrowed }" @click="borrowBook">
          <span class="material-symbols-rounded">{{ borrowed ? 'check' : 'local_library' }}</span>
          {{ borrowed ? 'Borrowed' : 'Get this book' }}
        </button>
      </section>

      <section v-if="!apiMode" class="lists-section reveal-block">
        <p class="detail-label">Add to your lists</p>
        <div>
          <button
            v-for="item in listOptions"
            :key="item.label"
            :class="{ active: lists.has(item.label) }"
            @click="toggleList(item.label)"
          >
            <span class="material-symbols-rounded">{{ item.icon }}</span>{{ item.label }}
          </button>
        </div>
      </section>
    </div>
    </template>

    <Transition name="toast"><div v-if="toast" class="book-toast" role="status">{{ toast }}</div></Transition>
  </main>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import BookCover from "@/components/book/BookCover.vue"
import ReaderGuidance from '@/components/book/ReaderGuidance.vue'
import { apiMode, backend } from '@/services/api.js'
import { bookFromApi } from '@/services/backendData.js'
import { useNav } from "@/composables/useNav"
import useReducedMotion from "@/composables/useReducedMotion"
import { gsap } from "@/animations/setup"
import libraryArt from "@/assets/library.png"
import { continueReading } from "@/mock/data"

const { selectedBook, selectedBookId, closeBook } = useNav()
const liveBook = ref(null), loading = ref(false), loadError = ref('')
const book = computed(() => apiMode ? liveBook.value : selectedBook.value || continueReading[0])
let loadVersion = 0
async function loadBook() {
  if (!apiMode) return
  const version = ++loadVersion
  liveBook.value = null; loadError.value = ''; loading.value = true
  try {
    if (!/^\d+$/.test(String(selectedBookId.value || ''))) throw new Error('This book link is invalid.')
    const result = await backend.book(selectedBookId.value)
    if (version === loadVersion) liveBook.value = bookFromApi(result)
  } catch (cause) { if (version === loadVersion) loadError.value = cause.message }
  finally { if (version === loadVersion) loading.value = false }
}
watch(selectedBookId, () => { expanded.value = false; loadBook() })
const isReducedMotion = useReducedMotion()
const root = ref(null)
const expanded = ref(false)
const saved = ref(false)
const borrowed = ref(false)
const lists = ref(new Set())
const toast = ref("")
let entrance = null
let toastTimer = null

const listOptions = [
  { label: "Want to Read", icon: "schedule" },
  { label: "Reading", icon: "favorite_border" },
  { label: "Save for Later", icon: "bookmark_border" }
]
const moodLabel = computed(() => {
  const genre = (book.value.genre || "").toLowerCase()
  if (genre.includes("self") || genre.includes("product")) return "Practical"
  if (genre.includes("fantasy") || genre.includes("fiction")) return "Immersive"
  if (genre.includes("history") || genre.includes("classic")) return "Reflective"
  return "Thoughtful"
})
const description = computed(() => {
  return book.value?.description || 'No description is available for this edition.'
})

function notify(message) {
  toast.value = message
  clearTimeout(toastTimer)
  toastTimer = setTimeout(() => { toast.value = "" }, 2200)
}
async function shareBook() {
  try { await navigator.clipboard.writeText(window.location.href); notify('Book link copied') }
  catch { notify('Could not copy the link. Copy it from your address bar.') }
}
function borrowBook() {
  borrowed.value = !borrowed.value
  notify(borrowed.value ? 'Demo borrowing preview only; no loan was created' : 'Demo preview cancelled')
}
function toggleList(label) {
  const next = new Set(lists.value)
  next.has(label) ? next.delete(label) : next.add(label)
  lists.value = next
  notify(next.has(label) ? `Added to ${label}` : `Removed from ${label}`)
}

onMounted(() => {
  loadBook()
  window.scrollTo({ top: 0 })
  if (apiMode || isReducedMotion.value) return
  entrance = gsap.timeline({ defaults: { ease: "power3.out" } })
    .fromTo(".book-hero__scene", { scale: 1.06, opacity: 0 }, { scale: 1, opacity: .72, duration: 1.05 })
    .fromTo(".book-hero__content > *", { y: 22, opacity: 0 }, { y: 0, opacity: 1, duration: .65, stagger: .1 }, "-=.65")
    .fromTo(root.value.querySelectorAll(".reveal-block"), { y: 16, opacity: 0 }, { y: 0, opacity: 1, duration: .48, stagger: .07 }, "-=.3")
})
onBeforeUnmount(() => {
  loadVersion++
  entrance?.kill()
  clearTimeout(toastTimer)
})
</script>

<style scoped>
.book-details {
  min-height: 100vh;
  color: var(--text-primary);
  background: radial-gradient(circle at 48% 0, rgba(80, 45, 142, .14), transparent 30rem), #07060d;
}
.book-hero { position:relative; isolation:isolate; overflow:hidden; min-height:26rem; border-bottom:1px solid var(--border-soft); }
.book-hero::after { content:""; position:absolute; inset:0; z-index:-1; background:linear-gradient(90deg,rgba(7,6,13,.95) 0%,rgba(7,6,13,.6) 55%,rgba(7,6,13,.32)),linear-gradient(0deg,#07060d 0%,transparent 55%); }
.book-hero__scene { position:absolute; inset:0; z-index:-2; opacity:.72; background-size:cover; background-position:65% 45%; filter:saturate(.9) contrast(1.04); }
.book-hero__actions { display:flex; align-items:center; justify-content:space-between; width:min(100%,68rem); margin:auto; padding:1.15rem var(--page-padding-mobile); }
.book-hero__actions > div { display:flex; gap:.65rem; }
.book-hero__actions button { display:grid; place-items:center; width:2.65rem; height:2.65rem; border:1px solid rgba(255,255,255,.1); border-radius:12px; color:var(--text-primary); background:rgba(10,8,18,.62); backdrop-filter:blur(14px); cursor:pointer; }
.book-hero__actions button:hover { border-color:rgba(181,125,255,.42); background:rgba(33,22,50,.78); }
.book-hero__actions .filled { color:var(--accent); font-variation-settings:"FILL" 1; }
.book-hero__content { display:flex; align-items:flex-end; gap:1.45rem; width:min(100%,68rem); margin:auto; padding:1.15rem var(--page-padding-mobile) 2rem; }
.book-hero__content :deep(.book-cover) { flex:0 0 9.2rem; width:9.2rem; height:13.8rem; border-radius:8px; box-shadow:0 20px 50px rgba(0,0,0,.55); }
.book-hero__copy { padding-bottom:.4rem; }
.book-hero__copy > p { margin:0 0 .55rem; color:var(--accent); font-size:.62rem; font-weight:700; letter-spacing:.16em; }
.book-hero__copy h1 { max-width:32rem; margin:0; font:500 clamp(2rem,8vw,3.55rem)/.98 var(--font-display); }
.book-hero__copy h2 { margin:.55rem 0 1rem; color:var(--text-secondary); font:500 1rem var(--font-body); }
.book-hero__chips { display:flex; flex-wrap:wrap; gap:.5rem; }
.book-hero__chips span { padding:.38rem .62rem; border:1px solid rgba(176,123,250,.18); border-radius:7px; color:#d7c4f6; background:rgba(111,65,176,.15); font-size:.7rem; }
.book-body { display:grid; gap:.75rem; width:min(100%,68rem); margin:auto; padding:1rem var(--page-padding-mobile) 7rem; }
.detail-panel { border:1px solid var(--border-soft); border-radius:14px; background:linear-gradient(145deg,rgba(20,17,30,.82),rgba(11,9,18,.72)); box-shadow:inset 0 1px rgba(255,255,255,.025); }
.detail-label { margin:0; color:var(--text-primary); font-size:.86rem; font-weight:600; }
.availability-panel { display:grid; grid-template-columns:1fr 1px 1.35fr; gap:1rem; padding:1rem 1.1rem; }
.availability-panel > div { display:grid; align-content:center; gap:.34rem; min-width:0; }
.availability-panel strong { display:flex; align-items:center; gap:.45rem; font-size:.82rem; font-weight:600; }
.availability-panel strong .material-symbols-rounded { display:grid; place-items:center; width:2rem; height:2rem; border-radius:8px; background:rgba(255,255,255,.04); font-size:1.1rem; }
.availability-panel small { padding-left:2.45rem; overflow:hidden; color:var(--text-tertiary); font-size:.7rem; text-overflow:ellipsis; white-space:nowrap; }
.availability-panel > div:first-child small { padding-left:1.25rem; }
.availability-panel__rule { width:1px; background:var(--border-soft); }
.availability { color:#e8e1ed; }
.availability i { width:.65rem; height:.65rem; border-radius:50%; background:#39cf73; box-shadow:0 0 0 5px rgba(57,207,115,.1),0 0 14px rgba(57,207,115,.5); }
.about-panel,.facts-panel { padding:1rem 1.1rem; }
.about-panel > p:nth-child(2) { margin:.65rem 0 0; color:var(--text-secondary); font-size:.81rem; line-height:1.68; }
.about-panel .is-clamped { display:-webkit-box; overflow:hidden; -webkit-box-orient:vertical; -webkit-line-clamp:3; }
.text-action { display:flex; align-items:center; gap:.2rem; margin-top:.55rem; padding:0; border:0; color:var(--accent); background:none; font:500 .8rem var(--font-body); cursor:pointer; }
.text-action span { font-size:1rem; }
.facts { display:flex; flex-wrap:wrap; gap:.65rem 1rem; margin-top:.7rem; color:var(--text-secondary); }
.facts span { display:flex; align-items:center; gap:.32rem; font-size:.72rem; }
.facts i { font-size:.9rem; color:var(--text-tertiary); }
.borrow-panel { display:flex; align-items:center; justify-content:space-between; gap:1rem; padding:.75rem; }
.borrow-panel > div { display:flex; align-items:center; gap:.7rem; min-width:0; }
.borrow-panel > div > .material-symbols-rounded { display:grid; place-items:center; flex:0 0 2.3rem; width:2.3rem; height:2.3rem; border:1px solid var(--border-soft); border-radius:10px; color:var(--text-secondary); }
.borrow-panel small { color:var(--text-tertiary); font-size:.68rem; }
.borrow-panel button { display:flex; align-items:center; justify-content:center; gap:.4rem; min-width:9.6rem; padding:.72rem 1rem; border:1px solid #9664da; border-radius:9px; color:#fff; background:linear-gradient(135deg,#7844be,#5d3295); font:600 .78rem var(--font-body); box-shadow:0 8px 24px rgba(112,58,176,.2); cursor:pointer; }
.borrow-panel button.borrowed { border-color:rgba(79,198,120,.4); background:rgba(46,135,78,.2); color:#9ee8b6; }
.borrow-panel button span { font-size:1rem; }
.lists-section { padding:1rem 0 0; }
.lists-section > div { display:flex; flex-wrap:wrap; gap:.55rem; margin-top:.65rem; }
.lists-section button { display:flex; align-items:center; gap:.4rem; padding:.6rem .75rem; border:1px solid var(--border-soft); border-radius:9px; color:var(--text-secondary); background:rgba(18,15,28,.72); font:.72rem var(--font-body); cursor:pointer; }
.lists-section button.active { border-color:rgba(179,121,255,.42); color:#d3b3ff; background:rgba(127,72,194,.16); }
.lists-section button span { font-size:.95rem; }
.book-toast { position:fixed; z-index:120; left:50%; bottom:1.4rem; transform:translateX(-50%); padding:.72rem 1rem; border:1px solid rgba(179,121,255,.28); border-radius:999px; color:var(--text-primary); background:rgba(20,15,31,.94); box-shadow:0 15px 40px rgba(0,0,0,.4); font-size:.75rem; white-space:nowrap; }
.toast-enter-active,.toast-leave-active { transition:all .2s ease; }
.toast-enter-from,.toast-leave-to { opacity:0; transform:translate(-50%,8px); }
@media (min-width:760px) {
  .book-hero { min-height:32rem; }
  .book-hero__actions,.book-hero__content,.book-body { padding-inline:2rem; }
  .book-hero__content { gap:2.5rem; padding-top:3.4rem; }
  .book-hero__content :deep(.book-cover) { flex-basis:12.2rem; width:12.2rem; height:18.2rem; }
  .book-body { grid-template-columns:1.05fr .95fr; gap:1rem; padding-top:1.35rem; }
  .availability-panel,.about-panel { grid-column:1 / -1; }
  .facts-panel,.borrow-panel { min-height:8.5rem; }
  .lists-section { grid-column:1 / -1; }
}
@media (max-width:520px) {
  .book-hero { min-height:23.5rem; }
  .book-hero__actions { padding-top:calc(.75rem + env(safe-area-inset-top)); }
  .book-hero__content { gap:1rem; padding-top:.7rem; }
  .book-hero__content :deep(.book-cover) { flex-basis:7.7rem; width:7.7rem; height:11.55rem; }
  .book-hero__copy > p { display:none; }
  .book-hero__copy h1 { font-size:2.05rem; }
  .availability-panel { grid-template-columns:1fr; gap:.9rem; }
  .availability-panel__rule { width:100%; height:1px; }
  .borrow-panel { align-items:stretch; flex-direction:column; }
  .borrow-panel button { width:100%; }
}
@media (prefers-reduced-motion:reduce) { *,*::before,*::after { transition:none!important; animation:none!important; } }
</style>
