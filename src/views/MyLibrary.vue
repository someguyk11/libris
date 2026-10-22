<template>
  <main ref="root" class="library-view">
    <div class="library-shell">
      <header class="library-header">
        <h1>My Library</h1>
        <button aria-label="Notifications" @click="showToast('You are all caught up.')">
          <span class="material-symbols-rounded">notifications</span><i></i>
        </button>
      </header>

      <LibraryTabs v-model="activeTab" />

      <div ref="content" class="library-content">
        <Transition name="library-panel" mode="out-in">
          <section :key="activeTab" class="library-panel">
            <div class="panel-heading">
              <h2>{{ panelTitle }} <span>{{ currentItems.length }}</span></h2>
              <button @click="sortAscending = !sortAscending">
                Sort: {{ sortAscending ? "Oldest" : sortLabel }}
                <span class="material-symbols-rounded">expand_more</span>
              </button>
            </div>

            <template v-if="activeTab === 'waitlists'">
              <div class="library-list">
                <article v-for="book in sortedItems" :key="book.id" class="library-row waitlist-row">
                  <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
                  <div class="library-row__body">
                    <h3>{{ book.title }}</h3><p>{{ book.author }}</p>
                    <div class="waitlist-copy">
                      <span><i class="material-symbols-rounded">group</i>You’re #{{ book.waitlistPosition }} in line</span>
                      <small>{{ waitDate(book.waitlistPosition) }}</small>
                    </div>
                  </div>
                  <button class="outline-action" @click="showToast('Opening ' + book.title)">View</button>
                </article>
              </div>
              <aside class="library-note">
                <div><h3>How wait lists work</h3><p>We’ll notify you when your turn is next and the book is ready.</p></div>
                <span class="hourglass material-symbols-rounded">hourglass_top</span>
              </aside>
              <section class="book-shelf">
                <div class="book-shelf__heading"><h2>Recently completed <span>4</span></h2><button aria-label="See all"><span class="material-symbols-rounded">chevron_right</span></button></div>
                <div class="book-shelf__rail">
                  <article v-for="book in savedBooks.slice(0, 4)" :key="book.id" class="returned-card">
                    <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
                    <h3>{{ book.title }}</h3><p>{{ book.author }}</p>
                  </article>
                </div>
              </section>
            </template>

            <template v-else-if="activeTab === 'borrowed'">
              <div class="library-list">
                <article v-for="book in sortedItems" :key="book.id" class="library-row borrowed-row">
                  <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
                  <div class="library-row__body">
                    <div class="row-title">
                      <div><h3>{{ book.title }}</h3><p>{{ book.author }}</p></div>
                      <div class="due-copy"><strong>{{ book.progressLabel }}</strong><small>{{ dueDate(book.id) }}</small></div>
                    </div>
                    <div class="row-progress"><i><span :style="{ width: book.progress + '%' }"></span></i><small>{{ book.progress }}%</small></div>
                  </div>
                  <button class="more-button" aria-label="More options"><span class="material-symbols-rounded">more_vert</span></button>
                </article>
              </div>
              <section class="book-shelf">
                <div class="book-shelf__heading"><h2>Returned recently <span>3</span></h2><button aria-label="See all"><span class="material-symbols-rounded">chevron_right</span></button></div>
                <div class="book-shelf__rail">
                  <article v-for="(book, index) in savedBooks.slice(0, 3)" :key="book.id" class="returned-card">
                    <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
                    <h3>{{ book.title }}</h3><p>{{ book.author }}</p><small>Returned {{ returnDates[index] }}</small>
                  </article>
                </div>
              </section>
              <button class="fine-card" @click="showToast('No outstanding fees.')">
                <span class="material-symbols-rounded">account_balance_wallet</span>
                <span><strong>Fines & fees</strong><small>You’re all good! No fines at the moment.</small></span>
                <i class="material-symbols-rounded">chevron_right</i>
              </button>
              <aside class="review-card">
                <div><h3>Love the library?</h3><p>Support it by leaving a review.</p><button @click="showToast('Thanks for supporting Libris.')">Leave a review</button></div>
                <span class="material-symbols-rounded">favorite</span>
              </aside>
            </template>

            <template v-else-if="activeTab === 'reading'">
              <div class="library-list compact-list">
                <article v-for="book in sortedItems" :key="book.id" class="library-row reading-row">
                  <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
                  <div class="library-row__body">
                    <h3>{{ book.title }}</h3><p>{{ book.author }}</p>
                    <div class="row-progress"><i><span :style="{ width: book.progress + '%' }"></span></i><small>{{ book.progress }}%</small></div>
                    <small class="last-read">{{ lastRead(book.id) }}</small>
                  </div>
                  <button class="round-action" aria-label="Continue reading" @click="showToast('Continuing ' + book.title)"><span class="material-symbols-rounded">chevron_right</span></button>
                </article>
              </div>
            </template>

            <template v-else-if="activeTab === 'collections'">
              <div class="collection-list">
                <article v-for="(collection, index) in sortedItems" :key="collection.id" class="collection-row">
                  <div class="collection-art" :class="'collection-art--' + ((index % 4) + 1)"><span class="material-symbols-rounded">collections_bookmark</span></div>
                  <div><h3>{{ collection.title }}</h3><p>{{ collection.count }} books</p></div>
                  <button aria-label="Collection options"><span class="material-symbols-rounded">more_horiz</span></button>
                </article>
                <button class="new-collection" @click="openNewCollection"><span class="material-symbols-rounded">add</span>New collection</button>
              </div>
            </template>

            <template v-else>
              <div class="library-list saved-list">
                <article v-for="book in sortedItems" :key="book.id" class="library-row saved-row">
                  <BookCover :cover="book.cover" :title="book.title" :author="book.author" size="small" />
                  <div class="library-row__body"><h3>{{ book.title }}</h3><p>{{ book.author }}</p></div>
                  <button class="save-button" :aria-label="'Toggle saved state for ' + book.title" @click="toggleSaved(book)">
                    <span class="material-symbols-rounded">{{ unsavedIds.has(book.id) ? "bookmark_add" : "bookmark" }}</span>
                  </button>
                </article>
              </div>
            </template>
          </section>
        </Transition>
      </div>
    </div>

    <Transition name="toast"><div v-if="toast" class="mini-toast" role="status"><span class="material-symbols-rounded">check_circle</span>{{ toast }}</div></Transition>
    <BottomSheet v-if="showNewCollectionSheet" :open="showNewCollectionSheet" title="New Collection" :height="330" @close="showNewCollectionSheet = false">
      <div class="new-collection-form">
        <label>Collection name<input ref="collectionNameInput" v-model="newCollectionName" placeholder="e.g. Weekend reading" @keydown.enter="createCollection" /></label>
        <label>Description<textarea v-model="newCollectionDescription" rows="3" placeholder="What belongs on this shelf?"></textarea></label>
        <div><AppButton variant="secondary" @click="showNewCollectionSheet = false">Cancel</AppButton><AppButton :disabled="!newCollectionName.trim()" @click="createCollection">Create</AppButton></div>
      </div>
    </BottomSheet>
    <BottomNav :model-value="activeNav" @update:model-value="activeNav = $event" />
  </main>
</template>

<script setup>
import { ref, computed, nextTick, watch, onMounted, onUnmounted } from "vue"
import { gsap } from "@/animations/setup"
import { useNav } from "@/composables/useNav"
import useReducedMotion from "@/composables/useReducedMotion"
import LibraryTabs from "@/components/library/LibraryTabs.vue"
import BookCover from "@/components/book/BookCover.vue"
import BottomNav from "@/components/navigation/BottomNav.vue"
import BottomSheet from "@/components/common/BottomSheet.vue"
import AppButton from "@/components/common/AppButton.vue"
import { waitlistBooks, borrowedBooks, readingBooks, userCollections, savedBooks } from "@/mock/data/library"

const { current: activeNav } = useNav()
const isReducedMotion = useReducedMotion()
const root = ref(null)
const content = ref(null)
const activeTab = ref("waitlists")
const sortAscending = ref(false)
const showNewCollectionSheet = ref(false)
const newCollectionName = ref("")
const newCollectionDescription = ref("")
const collectionNameInput = ref(null)
const toast = ref("")
const unsavedIds = ref(new Set())
const returnDates = ["Jul 28, 2026", "Jul 20, 2026", "Jul 12, 2026"]
let entrance = null
let panelTween = null
let toastTimer = null

const lists = { waitlists: waitlistBooks, borrowed: borrowedBooks, reading: readingBooks, collections: userCollections, saved: savedBooks }
const titles = { waitlists: "Items on hold", borrowed: "Currently borrowed", reading: "Currently reading", collections: "Collections", saved: "Saved books" }
const sortNames = { waitlists: "Soonest", borrowed: "Due date", reading: "Recent", collections: "Updated", saved: "Recent" }
const currentItems = computed(() => lists[activeTab.value] || [])
const sortedItems = computed(() => sortAscending.value ? [...currentItems.value].reverse() : [...currentItems.value])
const panelTitle = computed(() => titles[activeTab.value])
const sortLabel = computed(() => sortNames[activeTab.value])

function waitDate(position) {
  return ["Est. available Aug 10, 2026", "Est. available Aug 14, 2026", "Est. available Aug 22, 2026", "Est. available Sep 02, 2026", "Est. available Sep 08, 2026"][position - 1]
}
function dueDate(id) { return ["Aug 15, 2026", "Aug 22, 2026", "Aug 28, 2026", "Sep 01, 2026"][id % 4] }
function lastRead(id) { return ["Read today", "Read yesterday", "Read 3 days ago"][id % 3] }
function showToast(message) {
  toast.value = message
  clearTimeout(toastTimer)
  toastTimer = setTimeout(() => { toast.value = "" }, 2200)
}
function toggleSaved(book) {
  const next = new Set(unsavedIds.value)
  if (next.has(book.id)) { next.delete(book.id); showToast("Book saved") }
  else { next.add(book.id); showToast("Removed from saved") }
  unsavedIds.value = next
}
function openNewCollection() {
  showNewCollectionSheet.value = true
  nextTick(() => collectionNameInput.value?.focus())
}
function createCollection() {
  const name = newCollectionName.value.trim()
  if (!name) return
  userCollections.unshift({ id: Date.now(), title: name, description: newCollectionDescription.value.trim(), count: 0, type: "default" })
  newCollectionName.value = ""
  newCollectionDescription.value = ""
  showNewCollectionSheet.value = false
  showToast("Collection created")
}
async function animatePanel() {
  if (isReducedMotion.value) return
  await nextTick()
  panelTween?.kill()
  const items = content.value?.querySelectorAll(".library-row, .collection-row, .returned-card")
  panelTween = gsap.fromTo(items, { opacity: 0, y: 10 }, { opacity: 1, y: 0, duration: .35, stagger: .045, ease: "power3.out" })
}
watch(activeTab, () => { sortAscending.value = false; animatePanel() })
onMounted(() => {
  if (isReducedMotion.value) return
  entrance = gsap.timeline({ defaults: { ease: "power3.out" } })
    .fromTo(".library-header", { opacity: 0, y: -8 }, { opacity: 1, y: 0, duration: .4 })
    .fromTo(".library-tabs", { opacity: 0, y: 8 }, { opacity: 1, y: 0, duration: .4 }, .12)
  animatePanel()
})
onUnmounted(() => { entrance?.kill(); panelTween?.kill(); clearTimeout(toastTimer) })
</script>

<style scoped>
.library-view { min-height:100vh; padding-bottom:calc(5.25rem + env(safe-area-inset-bottom)); background:var(--page-gradient); }
.library-shell { width:min(100%,52rem); margin:auto; padding:0 var(--page-padding-mobile); }
.library-header { display:flex; align-items:center; justify-content:space-between; height:7rem; padding-top:env(safe-area-inset-top); }
.library-header h1 { margin:0; font:500 clamp(2rem,8vw,2.7rem)/1 var(--font-display); }
.library-header button { position:relative; display:grid; place-items:center; width:2.8rem; height:2.8rem; border:1px solid var(--border-soft); border-radius:13px; color:var(--text-secondary); background:rgba(18,14,28,.74); cursor:pointer; }
.library-header button i { position:absolute; top:.35rem; right:.35rem; width:.38rem; height:.38rem; border-radius:50%; background:var(--accent); box-shadow:0 0 8px var(--accent); }
.library-content { padding:1.7rem 0 2rem; }
.library-panel { min-height:30rem; }
.panel-heading,.row-title,.book-shelf__heading { display:flex; align-items:center; justify-content:space-between; gap:1rem; }
.panel-heading { margin-bottom:.9rem; }
.panel-heading h2,.book-shelf__heading h2 { margin:0; font:650 1rem var(--font-body); }
.panel-heading h2 span,.book-shelf__heading h2 span { display:inline-grid; place-items:center; min-width:1.65rem; height:1.35rem; margin-left:.35rem; padding:0 .4rem; border-radius:7px; color:#c9b3e8; background:rgba(127,85,172,.18); font-size:.68rem; }
.panel-heading > button { display:flex; align-items:center; gap:.1rem; border:0; color:var(--text-secondary); background:transparent; font:.7rem var(--font-body); cursor:pointer; }
.panel-heading > button span { font-size:1rem; color:var(--accent); }
.library-list { display:flex; flex-direction:column; gap:.7rem; }
.library-row { position:relative; display:flex; align-items:stretch; gap:.85rem; min-height:8.3rem; padding:.65rem; overflow:hidden; border:1px solid var(--border-subtle); border-radius:11px; background:linear-gradient(135deg,rgba(22,18,32,.94),rgba(14,12,22,.92)); transition:transform .2s ease,border-color .2s ease; }
.library-row:hover { transform:translateY(-2px); border-color:rgba(179,136,255,.24); }
.library-row :deep(.book-cover.small) { width:5.1rem; height:7rem; align-self:center; border-radius:6px; }
.library-row__body { flex:1; min-width:0; padding:.25rem 0; }
.library-row h3 { margin:0; color:var(--text-primary); font:620 .88rem/1.2 var(--font-body); }
.library-row p { margin:.25rem 0 0; color:var(--text-tertiary); font-size:.7rem; }
.waitlist-copy { display:flex; flex-direction:column; gap:.15rem; margin-top:1.45rem; }
.waitlist-copy span { display:flex; align-items:center; gap:.25rem; color:#c5a5ef; font-size:.67rem; }
.waitlist-copy i { font-size:.9rem; }
.waitlist-copy small { color:var(--accent); font-size:.65rem; }
.outline-action { align-self:flex-end; margin:0 .25rem .2rem 0; padding:.45rem .85rem; border:1px solid var(--accent); border-radius:99px; color:#d0adff; background:transparent; font:600 .68rem var(--font-body); cursor:pointer; }
.row-title { align-items:flex-start; }
.due-copy { display:flex; flex-direction:column; flex-shrink:0; text-align:right; }
.due-copy strong { color:var(--text-secondary); font-size:.67rem; font-weight:500; }
.due-copy small { color:var(--text-tertiary); font-size:.62rem; }
.row-progress { display:flex; align-items:center; gap:.75rem; margin-top:2rem; }
.row-progress > i { flex:1; height:4px; overflow:hidden; border-radius:99px; background:rgba(255,255,255,.045); }
.row-progress > i span { display:block; height:100%; border-radius:inherit; background:linear-gradient(90deg,#9270e8,#bc87ff); box-shadow:0 0 10px rgba(169,120,250,.32); }
.row-progress small { color:#baa0dd; font-size:.65rem; }
.more-button,.round-action,.save-button,.collection-row > button,.book-shelf__heading button { display:grid; place-items:center; align-self:center; border:0; color:var(--text-tertiary); background:transparent; cursor:pointer; }
.more-button span,.round-action span { font-size:1.1rem; }
.library-note,.review-card { position:relative; display:flex; align-items:center; justify-content:space-between; min-height:7rem; margin-top:1rem; padding:1rem 1.1rem; overflow:hidden; border:1px solid rgba(179,136,255,.14); border-radius:11px; background:linear-gradient(120deg,rgba(27,18,42,.85),rgba(21,13,35,.62)); }
.library-note h3,.review-card h3 { margin:0; font:600 .86rem var(--font-body); }
.library-note p,.review-card p { max-width:16rem; margin:.35rem 0 0; color:var(--text-secondary); font-size:.69rem; line-height:1.55; }
.hourglass { color:#ae73ff; font-size:3.5rem; text-shadow:0 0 20px #7634c8; transform:rotate(8deg); }
.book-shelf { margin-top:1.6rem; }
.book-shelf__heading { margin-bottom:.75rem; }
.book-shelf__heading button span { font-size:1.05rem; }
.book-shelf__rail { display:flex; gap:.65rem; overflow-x:auto; scrollbar-width:none; }
.returned-card { flex:0 0 7.25rem; min-width:0; padding:.45rem; border:1px solid var(--border-subtle); border-radius:9px; background:rgba(18,15,27,.76); }
.returned-card :deep(.book-cover.small) { width:100%; height:8.2rem; border-radius:5px; }
.returned-card h3 { margin:.45rem 0 0; overflow:hidden; color:var(--text-primary); font:600 .66rem/1.2 var(--font-body); white-space:nowrap; text-overflow:ellipsis; }
.returned-card p,.returned-card small { display:block; margin:.1rem 0 0; overflow:hidden; color:var(--text-tertiary); font-size:.58rem; white-space:nowrap; text-overflow:ellipsis; }
.fine-card { display:flex; align-items:center; gap:.75rem; width:100%; margin-top:.9rem; padding:.8rem .9rem; border:1px solid var(--border-subtle); border-radius:10px; color:var(--text-primary); background:rgba(18,15,27,.76); text-align:left; cursor:pointer; }
.fine-card > span:first-child { color:#bc83ff; }
.fine-card > span:nth-child(2) { display:flex; flex:1; flex-direction:column; }
.fine-card strong { font-size:.76rem; }
.fine-card small { margin-top:.15rem; color:var(--text-tertiary); font-size:.62rem; }
.fine-card > i { color:var(--text-secondary); font-size:1rem; }
.review-card { margin-top:.9rem; }
.review-card button { margin-top:.7rem; padding:.4rem .75rem; border:1px solid var(--accent); border-radius:99px; color:#cda9ff; background:transparent; font:.62rem var(--font-body); }
.review-card > span { color:#b46fff; font-size:3.4rem; text-shadow:0 0 22px #7e3cc2; }
.compact-list .library-row { min-height:7.5rem; }
.reading-row .row-progress { margin-top:1.25rem; }
.last-read { display:block; margin-top:.35rem; color:var(--text-disabled); font-size:.6rem; }
.collection-list { display:flex; flex-direction:column; gap:.65rem; }
.collection-row { display:flex; align-items:center; gap:.75rem; min-height:5.2rem; padding:.55rem .7rem; border:1px solid var(--border-subtle); border-radius:10px; background:rgba(18,15,27,.78); }
.collection-art { display:grid; place-items:center; width:4.4rem; height:4rem; flex-shrink:0; border-radius:7px; color:#ccb0f0; background:linear-gradient(135deg,#302047,#171322); }
.collection-art--2 { background:linear-gradient(135deg,#183449,#111a25); }
.collection-art--3 { background:linear-gradient(135deg,#433120,#211710); }
.collection-art--4 { background:linear-gradient(135deg,#243523,#141d13); }
.collection-row > div:nth-child(2) { flex:1; }
.collection-row h3 { margin:0; font:600 .75rem var(--font-body); }
.collection-row p { margin:.2rem 0 0; color:var(--text-tertiary); font-size:.63rem; }
.new-collection { display:flex; align-items:center; justify-content:center; gap:.45rem; min-height:3.5rem; border:1px dashed rgba(179,136,255,.3); border-radius:10px; color:var(--accent); background:rgba(119,75,176,.05); font:600 .72rem var(--font-body); cursor:pointer; }
.saved-row { min-height:5.3rem; }
.saved-row :deep(.book-cover.small) { width:3.2rem; height:4.3rem; }
.saved-row .library-row__body { align-self:center; }
.save-button { width:2.5rem; height:2.5rem; color:var(--accent); }
.save-button span { font-size:1.25rem; font-variation-settings:"FILL" 1; }
.mini-toast { position:fixed; z-index:180; left:50%; bottom:6rem; display:flex; align-items:center; gap:.45rem; transform:translateX(-50%); padding:.65rem .9rem; border:1px solid rgba(179,136,255,.25); border-radius:10px; color:var(--text-primary); background:rgba(20,15,31,.94); box-shadow:0 14px 35px rgba(0,0,0,.34); font-size:.7rem; white-space:nowrap; }
.mini-toast span { color:var(--accent); font-size:1rem; }
.toast-enter-active,.toast-leave-active { transition:opacity .2s ease,transform .2s ease; }
.toast-enter-from,.toast-leave-to { opacity:0; transform:translate(-50%,8px); }
.library-panel-enter-active,.library-panel-leave-active { transition:opacity .16s ease,transform .16s ease; }
.library-panel-enter-from { opacity:0; transform:translateY(7px); }
.library-panel-leave-to { opacity:0; transform:translateY(-5px); }
.new-collection-form { display:flex; flex-direction:column; gap:1rem; }
.new-collection-form label { display:flex; flex-direction:column; gap:.4rem; color:var(--text-secondary); font-size:.72rem; }
.new-collection-form input,.new-collection-form textarea { padding:.7rem .8rem; border:1px solid var(--border-soft); border-radius:9px; outline:0; color:var(--text-primary); background:var(--surface-1); font:.78rem var(--font-body); }
.new-collection-form > div { display:flex; justify-content:flex-end; gap:.65rem; margin-top:.5rem; }
@media (min-width:768px) {
  .library-shell { padding-inline:var(--page-padding-tablet); }
  .library-header { height:8rem; }
}
@media (max-width:430px) {
  .library-shell { padding-inline:.95rem; }
  .library-header { height:6.2rem; }
  .library-row { gap:.65rem; }
  .library-row :deep(.book-cover.small) { width:4.7rem; height:6.6rem; }
  .waitlist-copy { margin-top:1rem; }
}
</style>
