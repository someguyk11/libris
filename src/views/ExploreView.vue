<template>
  <main ref="root" class="explore-view">
    <section class="explore-hero">
      <div ref="sceneArt" class="explore-hero__art" :style="{ backgroundImage: `url(${libraryArt})` }" aria-hidden="true"></div>
      <div class="page-shell explore-hero__inner">
        <button ref="brand" class="wordmark" aria-label="Libris">Libris</button>
        <div class="explore-hero__copy">
          <p ref="greeting">Good evening, Rohit.</p>
          <h1 ref="title">Find your next world.</h1>
          <span>Stories, ideas, and quiet places worth getting lost in.</span>
          <div ref="search" class="explore-search">
            <SearchBar :show-filter="false" placeholder="Search a book, author, or a feeling..." />
          </div>
        </div>
        <section id="continue-reading" class="explore-section continue-section">
          <SectionHeader title="Continue reading" subtitle="Pick up exactly where you left off." action="View library" @action="activeNav = 'library'" />
          <ScrollRail gap=".85rem">
            <BookCard
              v-for="book in continueReading.slice(0, 7)"
              :key="book.id"
              v-bind="book"
              :show-bookmark="false"
              @click="openBook(book)"
            />
          </ScrollRail>
        </section>
      </div>
    </section>

    <div class="page-shell explore-content">
      <section class="explore-section path-section">
        <SectionHeader title="Reading paths" subtitle="Curated journeys with a beginning, a rhythm, and somewhere new to arrive." action="See all" @action="activeNav = 'discover'" />
        <ScrollRail gap="1rem">
          <article
            v-for="(path, index) in featuredPaths"
            :key="path.id"
            class="path-card"
            :class="`path-card--${index + 1}`"
            @click="openReadingPath(path)"
          >
            <div class="path-card__art" aria-hidden="true">
              <div class="orbit orbit--one"></div>
              <div class="orbit orbit--two"></div>
              <i class="planet planet--one"></i>
              <i class="planet planet--two"></i>
              <i class="planet planet--three"></i>
            </div>
            <div class="path-card__copy">
              <h3>{{ pathTitles[index] }}</h3>
              <div>
                <small><i class="material-symbols-rounded">menu_book</i>{{ path.bookCount }} books</small>
                <small><i class="material-symbols-rounded">schedule</i>{{ pathHours[index] }} hrs</small>
              </div>
              <button>Begin path <span>→</span></button>
            </div>
          </article>
        </ScrollRail>
      </section>

      <section class="explore-section collection-section">
        <SectionHeader title="Curator collections" subtitle="Small shelves shaped by a point of view." action="Discover more" @action="activeNav = 'discover'" />
        <div class="collection-grid">
          <article
            v-for="(collection, index) in curatorList"
            :key="collection.id"
            class="collection-card"
            :style="curatorStyle(index)"
            @click="openCuratedCollection(collection)"
          >
            <span>CURATED BY {{ curatorNames[index].toUpperCase() }}</span>
            <h3>{{ curatorTitles[index] }}</h3>
            <p>{{ collection.books.length }} books for {{ index ? 'slow, reflective hours' : 'a quieter kind of change' }}</p>
            <i class="material-symbols-rounded">arrow_outward</i>
          </article>
        </div>
      </section>

      <section class="explore-section popular-section">
        <SectionHeader title="Most borrowed" subtitle="What readers near you are reaching for this week." action="See all" @action="activeNav = 'discover'" />
        <ScrollRail gap=".85rem">
          <BookCard
            v-for="book in mostBorrowed.slice(0, 8)"
            :key="book.id"
            v-bind="book"
            :show-bookmark="false"
            @click="openBook(book)"
          />
        </ScrollRail>
      </section>

      <section class="explore-section rooms-banner" :style="{ backgroundImage: `url(${roomArt})` }">
        <div class="rooms-banner__copy">
          <span>READ TOGETHER · LIVE NOW</span>
          <h2>Quiet company,<br>one page at a time.</h2>
          <p>Join a low-pressure room, set your reading timer, and share the silence with other readers.</p>
          <div class="rooms-banner__meta"><i></i>{{ rooms[0]?.readers || 18 }} readers inside</div>
          <button @click="openRoom(rooms[0])">Enter the pavilion <span>→</span></button>
        </div>
      </section>

      <footer class="explore-footer">
        <span>Libris</span>
        <p>A quieter way to find what stays with you.</p>
      </footer>
    </div>
  </main>

  <ReadingPathPreview v-if="previewKind === 'path' && previewItem" :open="previewOpen" v-bind="previewItem" @close="closePreview" @start="closePreview" />
  <CuratorCollectionPreview v-else-if="previewKind === 'curated' && previewItem" :open="previewOpen" v-bind="previewItem" @close="closePreview" @open="closePreview" />
  <RoomPreview v-else-if="previewKind === 'room' && previewItem" :open="previewOpen" v-bind="previewItem" @close="closePreview" @enter="joinRoom" />
  <BottomNav :model-value="activeNav" @update:model-value="activeNav = $event" />
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from "vue"
import BottomNav from "@/components/navigation/BottomNav.vue"
import SearchBar from "@/components/navigation/SearchBar.vue"
import SectionHeader from "@/components/common/SectionHeader.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import BookCard from "@/components/book/BookCard.vue"
import ReadingPathPreview from "@/components/preview/ReadingPathPreview.vue"
import CuratorCollectionPreview from "@/components/preview/CuratorCollectionPreview.vue"
import RoomPreview from "@/components/reading-rooms/RoomPreview.vue"
import libraryArt from "@/assets/library.png"
import roomArt from "@/assets/reading-room-pavilion.png"
import useReducedMotion from "@/composables/useReducedMotion"
import { useNav } from "@/composables/useNav"
import { createHeroParallax } from "@/animations/hero"
import { gsap } from "@/animations/setup"
import { continueReading, readingPaths, curatedCollections, mostBorrowed, rooms } from "@/mock/data"

const { current: activeNav, openBook: goToBook, enterRoom } = useNav()
const isReducedMotion = useReducedMotion()
const root = ref(null)
const sceneArt = ref(null)
const brand = ref(null)
const greeting = ref(null)
const title = ref(null)
const search = ref(null)
const previewOpen = ref(false)
const previewKind = ref("")
const previewItem = ref(null)
const featuredPaths = readingPaths
const curatorList = curatedCollections
const pathTitles = ["Understanding Human Nature", "Entering Philosophy"]
const pathHours = [14, 18]
const curatorTitles = ["The Quiet Revolution", "Letters from Another Time"]
const curatorNames = ["Mira", "Aarav"]
let entrance = null
let parallax = null
let sectionTweens = []

onMounted(() => {
  if (isReducedMotion.value) return
  entrance = gsap.timeline({ defaults: { ease: "power3.out" } })
    .fromTo(sceneArt.value, { scale: 1.08, opacity: 0 }, { scale: 1, opacity: .78, duration: 1.2 })
    .fromTo(brand.value, { opacity: 0, y: -10 }, { opacity: 1, y: 0, duration: .45 }, "-=.9")
    .fromTo([greeting.value, title.value], { opacity: 0, y: 22 }, { opacity: 1, y: 0, duration: .65, stagger: .08 }, "-=.65")
    .fromTo(search.value, { opacity: 0, y: 14 }, { opacity: 1, y: 0, duration: .5 }, "-=.35")
  parallax = createHeroParallax(sceneArt.value, { start: "top top", end: 620 })
  sectionTweens = gsap.utils.toArray(root.value.querySelectorAll(".explore-section")).map(section =>
    gsap.fromTo(section, { opacity: 0, y: 24 }, { opacity: 1, y: 0, duration: .65, ease: "power3.out", scrollTrigger: { trigger: section, start: "top 90%", once: true } })
  )
})
onBeforeUnmount(() => {
  entrance?.kill()
  parallax?.scrollTrigger?.kill()
  parallax?.kill()
  sectionTweens.forEach(tween => { tween.scrollTrigger?.kill(); tween.kill() })
})

function showPreview(kind, item) { previewKind.value = kind; previewItem.value = item; previewOpen.value = true }
function closePreview() { previewOpen.value = false }
function curatorStyle(index) {
  return { backgroundImage: `linear-gradient(90deg,rgba(8,6,14,.3),rgba(8,6,14,.9)),url(${libraryArt})`, backgroundPosition: index ? "35% 58%" : "72% 42%" }
}
const openBook = book => goToBook(book, "explore")
function openReadingPath(path) {
  const index = featuredPaths.findIndex(item => item.id === path.id)
  showPreview("path", {
    ...path,
    title: pathTitles[index] || path.title,
    duration: pathHours[index] ? `${pathHours[index]} hrs` : path.duration,
    ...(index === 0 ? {
      tags: ["Psychology", "Philosophy"],
      quote: "The stories and ideas that explain why humans think, love, fear, and believe."
    } : {})
  })
}
function openCuratedCollection(item) {
  const index = curatorList.findIndex(collection => collection.id === item.id)
  showPreview("curated", { ...item, title: curatorTitles[index] || item.title, curator: curatorNames[index] || item.curator })
}
const openRoom = item => item && showPreview("room", item)
function joinRoom() { const room = previewItem.value; closePreview(); enterRoom(room) }
</script>

<style scoped>
.explore-view { min-height:100vh; padding-bottom:6rem; overflow:hidden; background:radial-gradient(circle at 80% 18%,rgba(91,52,151,.09),transparent 30rem),#07060d; }
.page-shell { width:min(100%,var(--content-width)); margin-inline:auto; padding-inline:var(--page-padding-mobile); }
.explore-hero { position:relative; isolation:isolate; min-height:37rem; overflow:hidden; }
.explore-hero::before { content:""; position:absolute; inset:0; z-index:-1; background:linear-gradient(90deg,#07060d 0%,rgba(7,6,13,.73) 48%,rgba(7,6,13,.18)),linear-gradient(0deg,#07060d 0%,transparent 48%); }
.explore-hero__art { position:absolute; inset:-2%; z-index:-2; opacity:.78; background-size:cover; background-position:68% 45%; filter:saturate(.95) contrast(1.05); }
.explore-hero__inner { position:relative; display:flex; flex-direction:column; min-height:37rem; }
.wordmark { align-self:flex-start; margin-top:1.3rem; padding:0; border:0; color:#f1ece7; background:transparent; font:500 1.8rem var(--font-display); cursor:pointer; }
.explore-hero__copy { display:flex; flex:1; flex-direction:column; justify-content:center; max-width:39rem; padding:3rem 0 0; }
.explore-hero__copy > p { margin:0; font:500 clamp(2rem,7.6vw,3.45rem)/.95 var(--font-display); }
.explore-hero__copy h1 { margin:0; color:#b988f5; font:400 italic clamp(2.45rem,9vw,4.35rem)/.96 var(--font-display); text-shadow:0 0 32px rgba(154,101,222,.2); }
.explore-hero__copy > span { margin-top:1.1rem; color:var(--text-secondary); font-size:.88rem; }
.explore-search { width:min(100%,35rem); margin-block:1.5em; }
.explore-search :deep(.search-wrapper) { height:3.25rem; border-color:rgba(225,210,246,.17); background:rgba(10,8,17,.68); }
.explore-search :deep(.search-placeholder) { right:1rem; }
.explore-content { padding-top:2.2rem; }
.explore-section { margin-bottom:4.35rem; }
.explore-section :deep(.section-header) { margin-bottom:1.1rem; }
.explore-section :deep(.section-header .title) { font:500 clamp(1.65rem,6vw,2.25rem)/1 var(--font-display); }
.explore-section :deep(.section-header .subtitle) { max-width:32rem; font-size:.82rem; }
.continue-section { min-width:0; margin-bottom:1.5rem; scroll-margin-top:1rem; }
.continue-section :deep(.book-card),.popular-section :deep(.book-card) { --book-card-width:8rem; --book-card-height:13rem; }
.path-card { position:relative; flex:0 0 min(82vw,23rem); min-height:28.5rem; overflow:hidden; border:1px solid rgba(184,118,238,.27); border-radius:22px; background:linear-gradient(155deg,#23102e,#100a1b 60%,#090711); cursor:pointer; }
.path-card--2 { border-color:rgba(84,172,224,.23); background:linear-gradient(155deg,#0a2031,#0a101c 60%,#07090f); }
.path-card__art { position:relative; height:15.2rem; overflow:hidden; background:radial-gradient(circle at 52% 54%,rgba(216,154,230,.9) 0 2%,rgba(164,86,183,.35) 3%,transparent 8%),radial-gradient(circle at 50% 55%,rgba(109,55,143,.45),transparent 46%); }
.path-card--2 .path-card__art { background:linear-gradient(90deg,transparent 42%,rgba(169,220,244,.16) 48%,rgba(255,239,191,.7) 50%,rgba(94,179,220,.12) 54%,transparent 61%),radial-gradient(ellipse at 50% 78%,rgba(41,116,157,.32),transparent 48%); }
.orbit { position:absolute; left:12%; right:12%; top:28%; height:48%; border:1px solid rgba(225,145,231,.34); border-radius:50%; transform:rotate(-12deg); }
.orbit--two { top:37%; transform:rotate(16deg) scale(.82); }
.path-card--2 .orbit { left:29%; right:29%; top:16%; height:75%; border-color:rgba(166,217,242,.33); border-radius:50% 50% 9% 9%; transform:none; }
.path-card--2 .orbit--two { top:26%; opacity:.45; transform:scale(.78); }
.planet { position:absolute; border-radius:50%; background:radial-gradient(circle at 35% 30%,#dfa3d9,#62357c 63%,#1b0d29); box-shadow:0 0 18px rgba(204,116,221,.3); }
.planet--one { left:22%; top:54%; width:2.2rem; height:2.2rem; }.planet--two { right:17%; top:28%; width:2.8rem; height:2.8rem; }.planet--three { right:26%; top:65%; width:1.15rem; height:1.15rem; }
.path-card--2 .planet { display:none; }
.path-card__copy { padding:.35rem 1.25rem 1.2rem; }
.path-card__copy > span { color:var(--text-tertiary); font-size:.62rem; font-weight:650; letter-spacing:.14em; text-transform:uppercase; }
.path-card h3 { min-height:4.2rem; margin:.55rem 0 .9rem; font:500 2rem/1 var(--font-display); }
.path-card__copy > div { display:flex; gap:1rem; padding-top:.85rem; border-top:1px solid var(--border-soft); }
.path-card small { display:flex; align-items:center; gap:.35rem; color:var(--text-secondary); font-size:.7rem; }.path-card small i { font-size:.95rem; }
.path-card button { display:flex; justify-content:center; gap:.75rem; width:100%; margin-top:1rem; padding:.75rem; border:1px solid rgba(179,120,244,.13); border-radius:10px; color:#bd83ff; background:rgba(142,77,208,.05); font:600 .8rem var(--font-body); }
.path-card--2 button { color:#6fc8f4; border-color:rgba(91,183,225,.13); }
.collection-grid { display:grid; gap:.85rem; }
.collection-card { position:relative; min-height:14.5rem; padding:1.35rem; overflow:hidden; border:1px solid var(--border-soft); border-radius:18px; background-size:cover; cursor:pointer; }
.collection-card::after { content:""; position:absolute; inset:0; background:linear-gradient(0deg,rgba(7,6,13,.97),rgba(7,6,13,.08)); }
.collection-card > * { position:relative; z-index:1; }.collection-card > span { color:var(--accent); font-size:.56rem; font-weight:700; letter-spacing:.16em; }.collection-card h3 { max-width:15rem; margin:7rem 0 .3rem; font:500 1.65rem/1 var(--font-display); }.collection-card p { max-width:18rem; color:var(--text-secondary); font-size:.7rem; }.collection-card > i { position:absolute; z-index:2; right:1rem; bottom:1rem; color:var(--text-secondary); }
.rooms-banner { position:relative; min-height:25rem; padding:2rem; overflow:hidden; border:1px solid rgba(218,176,133,.18); border-radius:22px; background-size:cover; background-position:center; }
.rooms-banner::before { content:""; position:absolute; inset:0; background:linear-gradient(90deg,rgba(6,7,13,.98) 0%,rgba(6,7,13,.83) 44%,rgba(6,7,13,.08)); }
.rooms-banner__copy { position:relative; z-index:1; display:flex; flex-direction:column; align-items:flex-start; justify-content:center; width:min(76%,28rem); min-height:21rem; }
.rooms-banner__copy > span { color:#d7a677; font-size:.6rem; font-weight:700; letter-spacing:.17em; }.rooms-banner h2 { margin:.7rem 0; font:500 2.35rem/.98 var(--font-display); }.rooms-banner p { color:var(--text-secondary); font-size:.8rem; line-height:1.55; }.rooms-banner__meta { display:flex; align-items:center; gap:.45rem; margin-top:1rem; color:var(--text-tertiary); font-size:.72rem; }.rooms-banner__meta i { width:.45rem; height:.45rem; border-radius:50%; background:#6fd795; box-shadow:0 0 10px #6fd795; }.rooms-banner button { margin-top:1.25rem; padding:.7rem 1rem; border:1px solid rgba(222,179,132,.28); border-radius:10px; color:#f2e9df; background:rgba(40,25,22,.58); font:600 .75rem var(--font-body); }
.explore-footer { display:flex; align-items:flex-end; justify-content:space-between; gap:1rem; margin:1rem 0 1.5rem; padding-top:1.5rem; border-top:1px solid var(--border-soft); color:var(--text-tertiary); }.explore-footer span { color:var(--text-secondary); font:500 1.4rem var(--font-display); }.explore-footer p { font-size:.68rem; }
@media (min-width:700px) { .page-shell { padding-inline:var(--page-padding-tablet); }.explore-hero,.explore-hero__inner { min-height:43rem; }.explore-hero__copy { padding-top:5rem; }.collection-grid { grid-template-columns:repeat(2,1fr); }.rooms-banner { padding:3rem; background-position:center 56%; } }
@media (min-width:1024px) { .page-shell { padding-inline:var(--page-padding-desktop); }.explore-hero__art { background-position:76% 43%; }.explore-content { padding-top:3.2rem; }.continue-section :deep(.book-card),.popular-section :deep(.book-card) { --book-card-width:8.7rem; --book-card-height:13.7rem; }.rooms-banner { min-height:28rem; } }
@media (max-width:520px) { .explore-hero,.explore-hero__inner { min-height:34rem; }.wordmark { margin-top:calc(1rem + env(safe-area-inset-top)); font-size:1.55rem; }.explore-hero__copy { padding:2.6rem 0 0; }.explore-hero__copy > span { max-width:19rem; font-size:.76rem; }.explore-content { padding-top:1.6rem; }.explore-section { margin-bottom:3.4rem; }.continue-section { margin-bottom:1.5rem; }.explore-section :deep(.section-header .subtitle) { display:none; }.path-card { min-height:27rem; }.rooms-banner { min-height:27rem; padding:1.35rem; background-position:61% center; }.rooms-banner::before { background:linear-gradient(90deg,rgba(6,7,13,.97),rgba(6,7,13,.68) 63%,rgba(6,7,13,.1)); }.rooms-banner__copy { width:88%; }.rooms-banner h2 { font-size:2.05rem; }.explore-footer { align-items:flex-start; flex-direction:column; } }
</style>
