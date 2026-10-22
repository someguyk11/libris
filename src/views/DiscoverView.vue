<template>
  <main ref="root" class="discover-view">
    <div class="discover-shell">
      <header ref="header" class="discover-header">
        <div class="discover-heading">
          <p>DISCOVER</p>
          <h1>What are you<br><em>in the mood for?</em></h1>
          <span>Search precisely, or wander on purpose.</span>
        </div>

        <div class="discover-search">
          <SearchBar placeholder="Search books, authors, or genres..." @search="onSearch" @clear="onSearch('')" />
          <FilterChips v-if="!apiMode" v-model="activeFilter" :items="filterItems" />
        </div>

        <button v-if="!apiMode" class="spotlight" @click="openBook(featuredBook)">
          <div class="spotlight__halo" aria-hidden="true"></div>
          <BookCover :cover="featuredBook.cover" :title="featuredBook.title" :author="featuredBook.author" size="medium" />
          <div class="spotlight__copy">
            <span>TONIGHT'S EDITOR PICK</span>
            <h2>{{ featuredBook.title }}</h2>
            <p>{{ featuredBook.author }}</p>
            <small>One bold idea, beautifully argued.</small>
          </div>
          <i class="material-symbols-rounded">arrow_outward</i>
        </button>
      </header>

      <section v-if="apiMode" class="discover-content discover-section book-shelf" aria-label="Live catalogue search">
        <SectionHeader title="Library catalogue" subtitle="Search registered books. Open a book to request Atlas shelf guidance." />
        <p v-if="catalogueLoading" role="status">Searching the catalogue…</p>
        <p v-if="catalogueError" role="alert">{{ catalogueError }}</p>
        <ScrollRail gap=".85rem"><BookCard v-for="book in catalogue" :key="book.id" v-bind="book" :show-bookmark="false" @click="openBook(book)" /></ScrollRail>
        <p v-if="!catalogueLoading && searchQuery && !catalogue.length && !catalogueError">No registered books match your search.</p>
      </section>
      <div v-else class="discover-content">
        <section class="discover-section book-shelf">
          <SectionHeader title="Curated for you" subtitle="Chosen from the ideas and stories you keep returning to." action="Refresh" @action="rotateFeatured" />
          <ScrollRail gap=".85rem">
            <BookCard v-for="book in curatedList" :key="book.id" v-bind="book" :show-bookmark="false" @click="openBook(book)" />
          </ScrollRail>
          <div v-if="!curatedList.length" class="no-results">
            <span class="material-symbols-rounded">search_off</span>
            <p>No books match that search yet.</p>
            <button @click="resetSearch">Clear filters</button>
          </div>
        </section>

        <section class="discover-section genre-section">
          <SectionHeader title="Browse by atmosphere" subtitle="Sometimes a feeling is a better starting point than a genre." />
          <div class="genre-grid">
            <button
              v-for="(genre, index) in genres.slice(0, 6)"
              :key="genre.id"
              class="genre-tile"
              :class="`genre-tile--${index + 1}`"
              @click="openGenre(genre)"
            >
              <span class="genre-tile__number">0{{ index + 1 }}</span>
              <span class="genre-tile__icon material-symbols-rounded">{{ genreIcons[index] }}</span>
              <strong>{{ genreMood[index] }}</strong>
              <small>{{ shortGenreName(genre.title) }} · {{ genreCounts[index] }} books</small>
              <i class="material-symbols-rounded">east</i>
            </button>
          </div>
        </section>

        <section class="discover-section journey-section">
          <SectionHeader title="Reading paths" subtitle="Curated journeys through ideas, stories, and disciplines." action="See all" />
          <ScrollRail gap="1rem">
            <article
              v-for="(path, index) in readingPaths"
              :key="path.id"
              class="journey-card"
              :class="`journey-card--${(index % 3) + 1}`"
              @click="openReadingPath(path)"
            >
              <div class="journey-card__art" aria-hidden="true">
                <div class="journey-card__orbit journey-card__orbit--one"></div>
                <div class="journey-card__orbit journey-card__orbit--two"></div>
                <div class="journey-card__core"></div>
                <i class="journey-card__body journey-card__body--one"></i>
                <i class="journey-card__body journey-card__body--two"></i>
              </div>
              <div class="journey-card__content">
                <p>{{ path.tags.join('  ·  ') }}</p>
                <h3>{{ pathDisplayNames[index] || path.title }}</h3>
                <span>{{ pathDescriptions[index] || path.description }}</span>
                <div class="journey-card__meta">
                  <small><i class="material-symbols-rounded">menu_book</i><b>{{ path.bookCount }}</b>Books</small>
                  <small><i class="material-symbols-rounded">schedule</i><b>{{ pathHours[index] || 12 }} hrs</b>Estimated</small>
                  <small><i class="material-symbols-rounded">signal_cellular_alt</i><b>{{ path.level }}</b>Level</small>
                </div>
                <button>Start path <span>→</span></button>
              </div>
            </article>
          </ScrollRail>
        </section>

        <section class="discover-section curator-section">
          <SectionHeader title="Curator collections" subtitle="Distinct voices. Unexpected connections." action="See all" />
          <div class="curator-grid">
            <button
              v-for="(collection, index) in curatorCollections"
              :key="collection.id"
              class="curator-card"
              :style="collectionStyle(index)"
              @click="openCurated(collection)"
            >
              <span>{{ collection.curator || (index % 2 ? 'Aarav' : 'Mira') }}</span>
              <h3>{{ curatorNames[index] || collection.title }}</h3>
              <p>{{ collection.description }}</p>
              <small>{{ collection.bookCount }} books <i class="material-symbols-rounded">arrow_forward</i></small>
            </button>
          </div>
        </section>

        <section class="discover-section book-shelf arrivals-section">
          <SectionHeader title="New & noteworthy" subtitle="Freshly added, already worth a place on your shelf." action="See all" />
          <ScrollRail gap=".85rem">
            <BookCard v-for="book in newArrivals" :key="book.id" v-bind="book" :show-bookmark="false" @click="openBook(book)" />
          </ScrollRail>
        </section>
      </div>
    </div>

    <CollectionPreview v-if="previewKind === 'collection' && previewItem" :open="previewOpen" v-bind="previewItem" @close="closePreview" @open="closePreview" />
    <ReadingPathPreview v-else-if="previewKind === 'path' && previewItem" :open="previewOpen" v-bind="previewItem" @close="closePreview" @start="closePreview" />
    <CuratorCollectionPreview v-else-if="previewKind === 'curated' && previewItem" :open="previewOpen" v-bind="previewItem" @close="closePreview" @open="closePreview" />
    <BottomNav :model-value="activeNav" @update:model-value="activeNav = $event" />
  </main>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from "vue"
import { gsap } from "@/animations/setup"
import { useNav } from "@/composables/useNav"
import useReducedMotion from "@/composables/useReducedMotion"
import SearchBar from "@/components/navigation/SearchBar.vue"
import FilterChips from "@/components/common/FilterChips.vue"
import BottomNav from "@/components/navigation/BottomNav.vue"
import SectionHeader from "@/components/common/SectionHeader.vue"
import ScrollRail from "@/components/common/ScrollRail.vue"
import BookCard from "@/components/book/BookCard.vue"
import BookCover from "@/components/book/BookCover.vue"
import CollectionPreview from "@/components/preview/CollectionPreview.vue"
import ReadingPathPreview from "@/components/preview/ReadingPathPreview.vue"
import CuratorCollectionPreview from "@/components/preview/CuratorCollectionPreview.vue"
import libraryArt from "@/assets/library.png"
import { apiMode, backend } from '@/services/api.js'
import { bookFromApi } from '@/services/backendData.js'
import { curatedForYou, discoverGenres, discoverReadingPaths, discoverCuratedCollections, newArrivals } from "@/mock/data"

const { current: activeNav, openBook: goToBook } = useNav()
const isReducedMotion = useReducedMotion()
const root = ref(null)
const header = ref(null)
const activeFilter = ref("all")
const searchQuery = ref("")
const catalogue = ref([]), catalogueLoading = ref(false), catalogueError = ref('')
let searchVersion = 0
const featuredIndex = ref(0)
const previewOpen = ref(false)
const previewKind = ref("")
const previewItem = ref(null)
const genres = discoverGenres
const readingPaths = discoverReadingPaths
const curatorCollections = discoverCuratedCollections
const genreIcons = ["landscape", "visibility", "rocket_launch", "history_edu", "person", "psychology"]
const genreMood = ["Other worlds", "Keep me guessing", "Beyond the known", "Stories that last", "A life, closely told", "Questions worth keeping"]
const genreCounts = [142, 98, 128, 110, 96, 74]
const pathDisplayNames = ["Understanding Human Nature", "Entering Philosophy", "Worlds That Changed Us", "The Art of Slow Reading"]
const pathDescriptions = ["Psychology, behavior, and the stories we tell ourselves.", "A clear, generous way into life's enduring questions.", "The ideas that redrew our place in the universe.", "Gentle fiction for attention without urgency."]
const pathHours = [14, 18, 22, 9]
const curatorNames = ["The Quiet Revolution", "Letters from Another Time", "Ideas That Stay", "Lives in Full"]
const filterItems = [
  { value: "all", label: "All" }, { value: "fiction", label: "Fiction" }, { value: "non-fiction", label: "Non-fiction" },
  { value: "fantasy", label: "Fantasy" }, { value: "mystery", label: "Mystery" }, { value: "sci-fi", label: "Sci-Fi" }, { value: "biography", label: "Biography" }
]
const filterBuckets = {
  fiction: ["fiction", "classics", "literature", "fantasy", "science fiction", "mystery"],
  "non-fiction": ["non-fiction", "productivity", "history", "psychology", "finance", "wellbeing", "self development"],
  fantasy: ["fantasy"], mystery: ["mystery"], "sci-fi": ["science fiction"], biography: ["biography", "memoir"]
}
let entrance = null
let sectionTweens = []

const featuredBook = computed(() => curatedForYou[featuredIndex.value % curatedForYou.length])
const curatedList = computed(() => curatedForYou.filter(book => {
  const genre = (book.genre || "").toLowerCase()
  const bucket = filterBuckets[activeFilter.value] || []
  if (activeFilter.value !== "all" && !bucket.some(key => genre.includes(key))) return false
  const query = searchQuery.value.trim().toLowerCase()
  return !query || `${book.title} ${book.author} ${book.genre}`.toLowerCase().includes(query)
}))

async function onSearch(query) {
  searchQuery.value = query
  if (!apiMode) return
  const version = ++searchVersion
  catalogueError.value = ''; catalogue.value = []
  if (!query.trim()) { catalogueLoading.value = false; return }
  catalogueLoading.value = true
  try { const books = await backend.books(query.trim()); if (version === searchVersion) catalogue.value = books.map(bookFromApi) }
  catch (cause) { if (version === searchVersion) catalogueError.value = cause.message }
  finally { if (version === searchVersion) catalogueLoading.value = false }
}
function resetSearch() { searchQuery.value = ""; activeFilter.value = "all" }
function rotateFeatured() { featuredIndex.value = (featuredIndex.value + 1) % curatedForYou.length }
function shortGenreName(name) { return name === "Science Fiction" ? "Sci-Fi" : name }
function collectionStyle(index) {
  const positions = ["72% 42%", "32% 56%", "82% 66%", "27% 43%"]
  const tones = ["rgba(102,55,134,.16)", "rgba(157,91,46,.14)", "rgba(52,73,131,.15)", "rgba(104,47,67,.14)"]
  return { backgroundImage: `linear-gradient(0deg,rgba(7,6,13,.97),${tones[index] || tones[0]} 82%),url(${libraryArt})`, backgroundPosition: positions[index] || "center" }
}
function showPreview(kind, item) { previewKind.value = kind; previewItem.value = item; previewOpen.value = true }
function closePreview() { previewOpen.value = false }
const openBook = book => goToBook(book, "discover")
const openGenre = item => showPreview("collection", item)
function openReadingPath(item) {
  const index = readingPaths.findIndex(path => path.id === item.id)
  showPreview("path", {
    ...item,
    title: pathDisplayNames[index] || item.title,
    duration: pathHours[index] ? `${pathHours[index]} hrs` : item.duration,
    ...(index === 0 ? {
      tags: ["Psychology", "Philosophy"],
      quote: "The stories and ideas that explain why humans think, love, fear, and believe."
    } : {})
  })
}
function openCurated(item) {
  const index = curatorCollections.findIndex(collection => collection.id === item.id)
  showPreview("curated", { ...item, title: curatorNames[index] || item.title })
}

onMounted(() => {
  if (isReducedMotion.value) return
  entrance = gsap.timeline({ defaults: { ease: "power3.out" } })
    .fromTo(header.value.querySelectorAll(".discover-heading > *, .discover-search, .spotlight"), { y: 18, opacity: 0 }, { y: 0, opacity: 1, duration: .55, stagger: .07 })
  sectionTweens = gsap.utils.toArray(root.value.querySelectorAll(".discover-section")).map(section =>
    gsap.fromTo(section, { y: 22, opacity: 0 }, { y: 0, opacity: 1, duration: .62, ease: "power3.out", scrollTrigger: { trigger: section, start: "top 90%", once: true } })
  )
})
onBeforeUnmount(() => { entrance?.kill(); sectionTweens.forEach(tween => { tween.scrollTrigger?.kill(); tween.kill() }) })
</script>

<style scoped>
.discover-view { min-height:100vh; padding-bottom:6.5rem; overflow:hidden; background:var(--page-gradient); }
.discover-shell { width:min(100%,var(--content-width)); margin:auto; padding-inline:var(--page-padding-mobile); }
.discover-header { display:grid; gap:1.2rem; padding-top:calc(2rem + env(safe-area-inset-top)); }
.discover-heading > p { margin:0 0 .55rem; color:var(--accent); font-size:.62rem; font-weight:700; letter-spacing:.2em; }
.discover-heading h1 { margin:0; font:500 clamp(2.6rem,10vw,4.8rem)/.88 var(--font-display); }
.discover-heading h1 em { color:#b784ee; font-weight:400; }
.discover-heading > span { display:block; margin-top:1rem; color:var(--text-tertiary); font-size:.82rem; }
.discover-search { display:grid; gap:.75rem; margin-top:.45rem; }
.discover-search :deep(.search-wrapper) { height:3.2rem; border-color:rgba(228,212,246,.15); background:rgba(16,13,24,.82); }
.discover-search :deep(.filter-chip) { border-radius:999px; background:rgba(18,15,27,.72); }
.discover-search :deep(.filter-chip.active) { border-color:#9764d4; color:#fff; background:linear-gradient(135deg,#7645b4,#56307f); box-shadow:0 0 18px rgba(142,82,205,.22); }
.spotlight { position:relative; display:grid; grid-template-columns:5.2rem 1fr auto; align-items:center; gap:1rem; width:100%; min-height:8rem; padding:.85rem; overflow:hidden; border:1px solid rgba(188,132,243,.16); border-radius:16px; color:var(--text-primary); background:linear-gradient(120deg,rgba(41,22,60,.78),rgba(13,10,21,.82)); text-align:left; cursor:pointer; }
.spotlight__halo { position:absolute; right:-5%; top:-80%; width:15rem; height:15rem; border-radius:50%; background:radial-gradient(circle,rgba(162,97,223,.22),transparent 68%); }
.spotlight :deep(.book-cover) { width:5.2rem; height:7.4rem; border:0; border-radius:6px; box-shadow:0 10px 22px rgba(0,0,0,.45); }
.spotlight__copy { position:relative; min-width:0; }.spotlight__copy > span { color:#c395ef; font-size:.52rem; font-weight:700; letter-spacing:.13em; }.spotlight h2 { overflow:hidden; margin:.28rem 0 0; font:500 1.35rem/1 var(--font-display); text-overflow:ellipsis; white-space:nowrap; }.spotlight p { margin:.2rem 0; color:var(--text-secondary); font-size:.72rem; }.spotlight small { color:var(--text-tertiary); font-size:.62rem; }.spotlight > i { position:relative; color:var(--text-secondary); }
.discover-content { padding-top:3.2rem; }.discover-section { margin-bottom:4rem; }.discover-section :deep(.section-header) { margin-bottom:1rem; }.discover-section :deep(.section-header .title) { font:500 clamp(1.75rem,6vw,2.4rem)/1 var(--font-display); }.discover-section :deep(.section-header .subtitle) { max-width:31rem; font-size:.82rem; }
.book-shelf :deep(.book-card) { --book-card-width:7.4rem; --book-card-height:12.4rem; }.no-results { display:grid; justify-items:center; gap:.45rem; padding:2rem; color:var(--text-tertiary); }.no-results button { border:0; color:var(--accent); background:none; }
.genre-grid { display:grid; grid-template-columns:repeat(2,1fr); gap:.7rem; }
.genre-tile { position:relative; display:grid; min-height:9.5rem; padding:1rem; overflow:hidden; border:1px solid var(--border-soft); border-radius:14px; color:var(--text-primary); background:radial-gradient(circle at 85% 10%,rgba(140,89,205,.22),transparent 55%),linear-gradient(145deg,#171022,#0c0a12); text-align:left; cursor:pointer; transition:transform .22s ease,border-color .22s ease; }
.genre-tile:hover { transform:translateY(-3px); border-color:rgba(190,139,244,.26); }.genre-tile--2 { background:radial-gradient(circle at 85% 10%,rgba(209,137,78,.18),transparent 55%),linear-gradient(145deg,#20150f,#0d0a0a); }.genre-tile--3 { background:radial-gradient(circle at 85% 10%,rgba(68,123,178,.22),transparent 55%),linear-gradient(145deg,#0e1a28,#090b11); }.genre-tile--4 { background:radial-gradient(circle at 85% 10%,rgba(117,99,157,.2),transparent 55%),linear-gradient(145deg,#17131f,#0b0a10); }.genre-tile--5 { background:radial-gradient(circle at 85% 10%,rgba(151,88,103,.2),transparent 55%),linear-gradient(145deg,#211218,#0d090d); }.genre-tile--6 { background:radial-gradient(circle at 85% 10%,rgba(78,145,143,.18),transparent 55%),linear-gradient(145deg,#0e1d1c,#090c0d); }
.genre-tile__number { color:rgba(238,230,244,.35); font:.68rem var(--font-display); }.genre-tile__icon { position:absolute; right:.9rem; top:.8rem; color:rgba(220,196,240,.55); font-size:1.25rem; }.genre-tile strong { align-self:end; max-width:8.5rem; margin-top:2.4rem; font:500 1.1rem/1.05 var(--font-display); }.genre-tile small { margin-top:.3rem; color:var(--text-tertiary); font-size:.6rem; }.genre-tile > i { position:absolute; right:.8rem; bottom:.8rem; color:rgba(238,230,244,.35); font-size:1rem; }
.journey-card { position:relative; flex:0 0 min(87vw,28rem); min-height:38rem; overflow:hidden; border:1px solid rgba(187,118,236,.3); border-radius:24px; background:linear-gradient(155deg,#24102f,#110a1c 55%,#08070e); cursor:pointer; }.journey-card--2 { border-color:rgba(76,167,219,.28); background:linear-gradient(155deg,#0a2134,#09101b 55%,#07090e); }.journey-card--3 { border-color:rgba(205,132,74,.24); background:linear-gradient(155deg,#2a170f,#130c0b 55%,#080707); }
.journey-card__art { position:relative; height:20rem; overflow:hidden; background:radial-gradient(circle at 51% 57%,rgba(238,180,237,.95) 0 2%,rgba(177,92,181,.38) 3%,transparent 8%),radial-gradient(circle at 50% 58%,rgba(126,56,151,.46),transparent 43%); }.journey-card--2 .journey-card__art { background:linear-gradient(90deg,transparent 42%,rgba(139,209,239,.22) 47%,rgba(250,238,191,.8) 50%,rgba(100,188,228,.16) 54%,transparent 60%),radial-gradient(ellipse at 50% 76%,rgba(52,139,182,.38),transparent 45%); }.journey-card--3 .journey-card__art { background:radial-gradient(circle at 50% 58%,rgba(241,178,104,.75) 0 3%,rgba(185,91,48,.25) 5%,transparent 9%),radial-gradient(circle at 50% 56%,rgba(137,73,40,.38),transparent 44%); }
.journey-card__orbit { position:absolute; left:12%; right:12%; top:28%; height:45%; border:1px solid rgba(227,151,230,.4); border-radius:50%; transform:rotate(-13deg); }.journey-card__orbit--two { top:38%; transform:rotate(15deg) scale(.84); }.journey-card--2 .journey-card__orbit { left:29%; right:29%; top:15%; height:77%; border-color:rgba(158,215,239,.38); border-radius:50% 50% 8% 8%; transform:none; }.journey-card--3 .journey-card__orbit { border-color:rgba(235,166,99,.34); }.journey-card__core { position:absolute; left:50%; top:57%; width:1.2rem; height:1.2rem; border-radius:50%; background:#ecc4eb; box-shadow:0 0 24px #d18bd3; transform:translate(-50%,-50%); }.journey-card__body { position:absolute; border-radius:50%; background:radial-gradient(circle at 35% 30%,#e0a0d9,#64357c 65%,#1c0c29); box-shadow:0 0 18px rgba(208,116,218,.32); }.journey-card__body--one { left:22%; top:56%; width:2.7rem; height:2.7rem; }.journey-card__body--two { right:18%; top:32%; width:3.3rem; height:3.3rem; }.journey-card--2 .journey-card__body { display:none; }.journey-card--3 .journey-card__body { background:radial-gradient(circle at 35% 30%,#e4ad75,#773e28 65%,#20100b); }
.journey-card__content { padding:.3rem 1.35rem 1.35rem; }.journey-card__content > p { margin:0; color:var(--text-tertiary); font-size:.66rem; font-weight:650; letter-spacing:.15em; text-transform:uppercase; }.journey-card h3 { margin:.65rem 0 .55rem; font:500 2.35rem/.92 var(--font-display); }.journey-card__content > span { display:-webkit-box; overflow:hidden; color:var(--text-secondary); font-size:.74rem; line-height:1.5; -webkit-box-orient:vertical; -webkit-line-clamp:2; }.journey-card__meta { display:grid; grid-template-columns:repeat(3,1fr); margin-top:1rem; padding-top:1rem; border-top:1px solid var(--border-soft); }.journey-card__meta small { display:grid; justify-items:center; gap:.18rem; color:var(--text-tertiary); font-size:.58rem; }.journey-card__meta small+small { border-left:1px solid var(--border-soft); }.journey-card__meta i { font-size:1.05rem; }.journey-card__meta b { color:var(--text-primary); font-size:.7rem; font-weight:600; }.journey-card button { width:100%; margin-top:1.1rem; padding:.82rem; border:1px solid rgba(178,111,236,.15); border-radius:11px; color:#bf82fb; background:rgba(144,75,205,.055); font:600 .84rem var(--font-body); }.journey-card--2 button { color:#6bc6ef; }.journey-card--3 button { color:#e1a46f; }
.curator-grid { display:grid; gap:.8rem; }.curator-card { position:relative; display:flex; flex-direction:column; align-items:flex-start; justify-content:flex-end; min-height:17rem; padding:1.25rem; overflow:hidden; border:1px solid var(--border-soft); border-radius:17px; color:var(--text-primary); background-size:cover; text-align:left; cursor:pointer; }.curator-card::after { content:""; position:absolute; inset:0; background:linear-gradient(0deg,rgba(6,5,11,.98),rgba(6,5,11,.08)); }.curator-card > * { position:relative; z-index:1; }.curator-card > span { color:#bd8af0; font-size:.57rem; font-weight:700; letter-spacing:.14em; text-transform:uppercase; }.curator-card h3 { margin:.55rem 0 .35rem; font:500 1.6rem/1 var(--font-display); }.curator-card p { display:-webkit-box; overflow:hidden; max-width:24rem; color:var(--text-secondary); font-size:.68rem; line-height:1.45; -webkit-box-orient:vertical; -webkit-line-clamp:2; }.curator-card small { display:flex; align-items:center; gap:.45rem; margin-top:.7rem; color:var(--text-tertiary); font-size:.64rem; }.curator-card small i { font-size:1rem; }
@media (min-width:700px) { .discover-shell { padding-inline:var(--page-padding-tablet); }.discover-header { grid-template-columns:1fr 1.05fr; align-items:end; gap:1.5rem 2rem; padding-top:4rem; }.discover-search { grid-column:1 / -1; grid-row:2; }.spotlight { grid-column:2; grid-row:1; }.genre-grid { grid-template-columns:repeat(3,1fr); }.curator-grid { grid-template-columns:repeat(2,1fr); }.book-shelf :deep(.book-card) { --book-card-width:8rem; --book-card-height:13rem; } }
@media (min-width:1024px) { .discover-shell { padding-inline:var(--page-padding-desktop); }.discover-header { min-height:26rem; }.discover-search { max-width:54rem; }.spotlight { min-height:10rem; }.journey-card { flex-basis:29rem; }.curator-card { min-height:19rem; } }
@media (max-width:520px) { .discover-header { padding-top:calc(1.45rem + env(safe-area-inset-top)); }.discover-heading h1 { font-size:2.9rem; }.spotlight { grid-template-columns:4.7rem 1fr auto; }.spotlight :deep(.book-cover) { width:4.7rem; height:6.7rem; }.spotlight small { display:none; }.discover-content { padding-top:2.4rem; }.discover-section { margin-bottom:3.35rem; }.discover-section :deep(.section-header .subtitle) { display:none; }.journey-card { min-height:37rem; }.journey-card__art { height:19.3rem; }.curator-card { min-height:15.5rem; } }
</style>
