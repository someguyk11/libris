<template>
  <main class="reading-room" :class="{ 'is-paused': !session.running }" :style="{ '--room-art': `url(${lakesideArt})` }">
    <div class="reading-room__layout">
      <header class="room-header">
        <span class="room-readers"><span class="material-symbols-rounded">group</span>{{ room.readers || 18 }} <small>reading</small></span>
        <div><time>{{ localTime }}</time><button class="room-icon" :aria-pressed="animationsEnabled" aria-label="Toggle animations" :title="animationsEnabled ? 'Turn animations off' : 'Turn animations on'" @click="setAnimationsEnabled(!animationsEnabled)"><span class="material-symbols-rounded">{{ animationsEnabled ? 'motion_photos_on' : 'motion_photos_off' }}</span></button><button class="room-icon" aria-label="Leave reading room" @click="leave"><span class="material-symbols-rounded">logout</span></button></div>
      </header>
      <section class="room-focus" aria-labelledby="room-title">
        <p id="room-title">{{ room.title || 'Twilight reading room' }}</p>
        <div class="room-countdown" role="timer" aria-label="Time remaining" aria-live="off">{{ countdown }}</div>
        <div class="room-divider" aria-hidden="true"><i></i><span class="material-symbols-rounded">auto_awesome</span><i></i></div>
        <p class="room-timer-status" role="status">{{ remaining === 0 ? 'A little time, well spent.' : session.running ? 'A quiet moment, just for reading.' : 'Paused. Your place is safe.' }}</p>
        <div class="room-controls">
          <button class="room-pause" @click="toggleTimer"><span class="material-symbols-rounded">{{ session.running ? 'pause' : 'play_arrow' }}</span>{{ session.running ? 'Pause' : remaining === 0 ? 'Read again' : 'Resume' }}</button>
          <button class="room-icon" aria-label="Reset timer" title="Reset and pause timer" @click="resetTimer"><span class="material-symbols-rounded">restart_alt</span></button>
          <label class="room-duration"><span class="sr-only">Session duration</span><select v-model.number="session.duration" @change="resetTimer"><option v-for="minutes in durationChoices" :key="minutes" :value="minutes * 60">{{ minutes }} min</option></select></label>
        </div>
      </section>
      <footer class="room-bottom">
        <section class="room-panel reading-now">
          <div><p class="room-label">YOU’RE READING</p><h1>{{ currentBook.title }}</h1><button v-if="!editingPage" class="room-page" @click="editPage">Page {{ session.page }} <span class="material-symbols-rounded">edit</span></button><form v-else class="room-page-form" @submit.prevent="savePage"><label>Page <input ref="pageInput" v-model.number="draftPage" type="number" min="1" :max="currentBook.pages || 5000" required aria-label="Current page" /></label><button type="submit">Save</button><button type="button" @click="editingPage = false">Cancel</button></form></div>
          <span class="room-book-icon material-symbols-rounded" aria-hidden="true">menu_book</span>
        </section>
        <section class="room-panel room-shelf">
          <div class="room-shelf__heading"><h2 class="room-label">CURRENTLY READING</h2><button :aria-expanded="expanded" @click="expanded = !expanded">{{ expanded ? 'Show less' : `All ${books.length} books` }}<span class="material-symbols-rounded">{{ expanded ? 'expand_less' : 'expand_more' }}</span></button></div>
          <div class="room-book-list">
            <button v-for="book in visibleBooks" :key="book.id" class="room-book" :aria-pressed="book.id === currentBook.id" @click="chooseBook(book)"><BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" /><span>{{ book.title }}</span><small v-if="book.id === currentBook.id">Your current book</small></button>
          </div>
        </section>
      </footer>
    </div>
  </main>
</template>
<script setup>
import { computed, nextTick, onBeforeUnmount, reactive, ref, watch } from 'vue'
import BookCover from '../book/BookCover.vue'
import lakesideArt from '@/assets/twilight-lakeside.png'
import { continueReading } from '@/mock/data'
import { animationsEnabled, setAnimationsEnabled } from '@/composables/useReducedMotion'
import { durationInSeconds, formatCountdown, secondsRemaining } from '@/composables/readingSession'
const props = defineProps({ room: { type: Object, default: () => ({}) } })
const emit = defineEmits(['leave'])
const books = computed(() => props.room.books?.length ? props.room.books : continueReading)
const storageKey = `libris-reading-session-${props.room.id || 1}`
const initialDuration = durationInSeconds(props.room.timeLeft)
let saved = null
try { saved = JSON.parse(localStorage.getItem(storageKey)) } catch { /* Start a fresh session. */ }
const validSaved = saved && Number.isFinite(saved.duration) && Number.isFinite(saved.seconds) && Number.isFinite(saved.deadline) && saved.duration >= 60 && saved.duration <= 7200
const session = reactive(validSaved ? saved : { duration: initialDuration, seconds: initialDuration, deadline: Date.now() + initialDuration * 1000, running: true, bookId: books.value[0]?.id, page: 1, pages: {} })
session.pages ||= {}
const now = ref(Date.now())
const remaining = computed(() => secondsRemaining(session, now.value))
const countdown = computed(() => formatCountdown(remaining.value))
const currentBook = computed(() => books.value.find(book => book.id === session.bookId) || books.value[0] || { title: 'Choose your next book', pages: 5000 })
const localTime = computed(() => new Date(now.value).toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' }))
const durationChoices = computed(() => [...new Set([25, 42, 50, 60, initialDuration / 60])].sort((a, b) => a - b))
const expanded = ref(false)
const visibleBooks = computed(() => expanded.value ? books.value : books.value.slice(0, 5))
const editingPage = ref(false)
const pageInput = ref(null)
const draftPage = ref(session.page)
function persist() { try { localStorage.setItem(storageKey, JSON.stringify(session)) } catch { /* Reading continues for this visit. */ } }
function toggleTimer() {
  now.value = Date.now()
  if (session.running) { session.seconds = remaining.value; session.running = false }
  else { if (session.seconds === 0) session.seconds = session.duration; session.deadline = now.value + session.seconds * 1000; session.running = true }
}
function resetTimer() { session.running = false; session.seconds = session.duration; now.value = Date.now() }
function chooseBook(book) { session.pages[currentBook.value.id] = session.page; session.bookId = book.id; session.page = session.pages[book.id] || 1; editingPage.value = false }
async function editPage() { draftPage.value = session.page; editingPage.value = true; await nextTick(); pageInput.value?.focus() }
function savePage() { session.page = Math.min(currentBook.value.pages || 5000, Math.max(1, Math.floor(draftPage.value))); editingPage.value = false }
function leave() { if (session.running) toggleTimer(); persist(); emit('leave') }
watch(session, persist, { deep: true })
const interval = setInterval(() => { now.value = Date.now(); if (session.running && remaining.value === 0) { session.seconds = 0; session.running = false } }, 1000)
onBeforeUnmount(() => { clearInterval(interval); persist() })
</script>
<style scoped>
.reading-room{position:relative;isolation:isolate;min-height:100dvh;color:#f1eaf9;background:#080711}.reading-room::before{content:'';position:fixed;inset:0;z-index:-2;background:var(--room-art) center 48%/cover}.reading-room::after{content:'';position:fixed;inset:0;z-index:-1;background:linear-gradient(0deg,rgba(6,5,13,.97),rgba(6,5,13,.2) 65%,rgba(6,5,13,.25))}.reading-room__layout{width:min(100%,960px);margin:auto;padding:0 clamp(1.1rem,5vw,3rem) 2rem}.room-header{display:flex;align-items:center;justify-content:space-between;gap:1rem;padding:calc(1.4rem + env(safe-area-inset-top)) 0 0}.room-readers,.room-header>div{display:flex;align-items:center;gap:.65rem}.room-readers{font-size:1rem}.room-readers>.material-symbols-rounded{color:#c095f5;font-size:1.5rem}.room-readers small{color:#bab0ca;font-size:.7rem}.room-header time{font-size:.8rem;color:#c59af9}.room-icon{display:inline-grid;place-items:center;width:2.35rem;height:2.35rem;border:1px solid rgba(194,151,242,.12);border-radius:10px;color:#d3b4fb;background:rgba(10,8,20,.4);cursor:pointer}.room-icon:hover{border-color:#ae7bea;background:rgba(106,64,156,.2)}.room-focus{display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:53dvh;padding:3rem 0;text-align:center}.room-focus>p:first-child{margin:0 0 1.2rem;font-size:.65rem;letter-spacing:.16em;text-transform:uppercase;color:#c0a9db}.room-countdown{font:300 clamp(5rem,18vw,8rem)/1 var(--font-body);letter-spacing:-.055em;font-variant-numeric:tabular-nums;text-shadow:0 0 45px rgba(159,115,220,.28)}.room-divider{display:flex;align-items:center;gap:.8rem;margin:1.3rem 0 .3rem;color:#b689ed}.room-divider i{width:3rem;height:1px;background:linear-gradient(90deg,transparent,#a573df)}.room-divider i:last-child{rotate:180deg}.room-divider span{font-size:.85rem}.room-timer-status{font-size:.75rem;color:#c6adE8;margin:.7rem 0 1rem}.room-controls{display:flex;align-items:center;gap:.7rem}.room-pause{display:flex;align-items:center;gap:.45rem;min-width:6.5rem;justify-content:center;padding:.6rem .9rem;border:1px solid rgba(188,140,246,.32);border-radius:10px;color:#e9d6ff;background:rgba(49,28,77,.45);font:.75rem var(--font-body);cursor:pointer}.room-pause:hover{background:rgba(97,55,151,.55)}.room-duration select{padding:.6rem .3rem;border:0;border-radius:8px;color:#c2a8e2;background:#171020;font:.7rem var(--font-body)}.room-bottom{display:grid;gap:1rem}.room-panel{min-width:0;padding:1.3rem;border:1px solid rgba(191,162,225,.17);border-radius:16px;background:linear-gradient(125deg,rgba(14,12,24,.82),rgba(10,8,18,.88));backdrop-filter:blur(14px)}.reading-now{display:flex;align-items:center;justify-content:space-between;gap:1rem}.room-label{margin:0;color:#b98be9;font:500 .63rem var(--font-body);letter-spacing:.12em}.reading-now h1{margin:.7rem 0 .45rem;font:400 clamp(1.8rem,5vw,2.7rem)/1.1 var(--font-display)}.room-page{display:flex;align-items:center;gap:.4rem;border:0;color:#be91f0;background:none;font:.85rem var(--font-body);cursor:pointer}.room-page span{font-size:.9rem;opacity:.6}.room-book-icon{display:grid;place-items:center;flex-shrink:0;width:3.8rem;height:3.8rem;border:1px solid rgba(180,134,236,.13);border-radius:50%;color:#b883f5;font-size:1.7rem}.room-shelf__heading{display:flex;justify-content:space-between;align-items:center;gap:.5rem;margin-bottom:1rem}.room-shelf__heading>button{display:flex;align-items:center;gap:.2rem;border:0;background:none;color:#c2a3e9;font:.65rem var(--font-body);cursor:pointer}.room-shelf__heading span{font-size:1rem}.room-book-list{display:flex;gap:1rem;overflow-x:auto;padding:2px 2px .4rem;scrollbar-width:thin;scrollbar-color:#4c326d transparent}.room-book{display:flex;flex:0 0 5.2rem;min-width:0;flex-direction:column;align-items:center;gap:.5rem;padding:0;border:0;color:#e0d9e8;background:none;cursor:pointer}.room-book :deep(.book-cover.small){width:5.2rem;height:7.6rem;border:1px solid rgba(195,155,241,.15);transition:border-color .2s}.room-book[aria-pressed=true] :deep(.book-cover){border-color:#bb85ff;box-shadow:0 0 0 1px #bb85ff}.room-book>span{font:.67rem/1.5 var(--font-body);text-align:center}.room-book>small{color:#b686ec;font-size:.55rem}.room-page-form{display:flex;align-items:center;flex-wrap:wrap;gap:.4rem;font-size:.75rem;color:#c7ade9}.room-page-form input{width:4.4rem;padding:.4rem;border:1px solid #675183;border-radius:6px;color:white;background:#130e20}.room-page-form button{padding:.4rem;border:0;border-radius:6px;color:#d6bbfa;background:#35204f;cursor:pointer}.sr-only{position:absolute;width:1px;height:1px;overflow:hidden;clip-path:inset(50%)}@media(min-width:800px){.room-focus{min-height:55dvh}.room-bottom{grid-template-columns:.9fr 1.4fr;align-items:stretch}.room-shelf{min-width:0}.reading-now{flex-direction:column;align-items:flex-start;justify-content:center}.room-book-icon{display:none}}@media(max-width:400px){.room-readers small{display:none}.room-header>div{gap:.35rem}.room-panel{padding:1.1rem}.room-focus{min-height:49dvh}}[data-motion=on] .room-book:hover{translate:0 -2px}
</style>
