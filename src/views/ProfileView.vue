<template>
  <main ref="root" class="profile-view">
    <Transition name="profile-page" mode="out-in">
      <section v-if="!settingsOpen" key="profile" class="profile-overview">
        <div class="profile-hero" :style="{ backgroundImage: 'url(' + libraryArt + ')' }">
          <header>
            <button class="profile-brand" @click="activeNav = 'explore'">Libris</button>
            <div>
              <button aria-label="Notifications" @click="showToast('You are all caught up.')"><span class="material-symbols-rounded">notifications</span></button>
              <button aria-label="Open settings" @click="openSettings"><span class="material-symbols-rounded">settings</span></button>
            </div>
          </header>
        </div>

        <div class="profile-identity">
          <div class="profile-initial">R</div>
          <template v-if="!editing">
            <h1>{{ user.name }}</h1>
            <p>@{{ user.username }}</p>
            <button @click="editing = true">Edit Profile <span class="material-symbols-rounded">edit</span></button>
          </template>
          <form v-else class="profile-edit" @submit.prevent="saveProfile">
            <input v-model="draftName" aria-label="Name" />
            <div><span>@</span><input v-model="draftUsername" aria-label="Username" /></div>
            <div class="profile-edit__actions"><button type="button" @click="cancelEdit">Cancel</button><button type="submit">Save</button></div>
          </form>
        </div>

        <div class="profile-content">
          <section class="profile-section">
            <p class="section-label">READING OVERVIEW</p>
            <div class="profile-stats">
              <article v-for="stat in stats" :key="stat.label">
                <span class="material-symbols-rounded">{{ stat.icon }}</span>
                <strong>{{ stat.value }}</strong>
                <small>{{ stat.label }}</small>
              </article>
            </div>
            <div class="overview-charts">
              <div class="month-ring">
                <div><small>This month</small><strong>7</strong><span>Books read</span></div>
              </div>
              <div class="month-bars" aria-label="Books read over six months">
                <i v-for="(height, index) in monthBars" :key="index" :style="{ height: height + '%' }"><span>{{ monthLabels[index] }}</span></i>
              </div>
            </div>
          </section>

          <section class="profile-section">
            <p class="section-label">YOUR READING WORLD</p>
            <div class="reading-world">
              <article>
                <small>Currently reading</small><h3>The Picture of<br>Dorian Gray</h3>
                <span class="material-symbols-rounded">menu_book</span>
              </article>
              <article>
                <small>Active path</small><h3>Understanding<br>Philosophy</h3>
                <span class="material-symbols-rounded">route</span>
              </article>
              <article>
                <small>Favorite collection</small><h3>Quiet Evenings</h3>
                <span class="material-symbols-rounded">chair</span>
              </article>
            </div>
          </section>

          <section class="profile-section">
            <p class="section-label">READING GOAL</p>
            <button class="goal-card" @click="showToast('Reading goal editor coming next.')">
              <div class="goal-ring"><span>56%</span></div>
              <div>
                <h3>2026 Reading Goal</h3>
                <p><strong>28</strong> of 50 books</p>
                <i><span></span></i>
                <small><span class="material-symbols-rounded">calendar_today</span>22 books to go this year</small>
              </div>
            </button>
          </section>

          <section class="profile-section">
            <p class="section-label">MY ACTIVITY</p>
            <div class="activity-list">
              <button v-for="item in activities" :key="item.label" @click="item.route ? activeNav = item.route : showToast(item.label)">
                <span class="material-symbols-rounded">{{ item.icon }}</span>
                <strong>{{ item.label }}</strong>
                <i class="material-symbols-rounded">chevron_right</i>
              </button>
            </div>
          </section>
        </div>
      </section>

      <section v-else key="settings" class="settings-page">
        <header class="settings-header">
          <button @click="closeSettings"><span class="material-symbols-rounded">arrow_back</span></button>
          <h1>Settings</h1>
          <span></span>
        </header>
        <div class="settings-content">
          <section v-for="group in settingsGroups" :key="group.label" class="settings-group">
            <p>{{ group.label }}</p>
            <div>
              <button v-for="item in group.items" :key="item.title" :aria-pressed="item.title === 'Animations' ? animationsEnabled : undefined" @click="item.title === 'Animations' ? setAnimationsEnabled(!animationsEnabled) : showToast(item.title)">
                <span class="settings-icon material-symbols-rounded">{{ item.icon }}</span>
                <span><strong>{{ item.title }}</strong><small>{{ item.description }}</small></span>
                <i class="material-symbols-rounded">{{ item.title === 'Animations' ? (animationsEnabled ? 'toggle_on' : 'toggle_off') : 'chevron_right' }}</i>
              </button>
            </div>
          </section>
          <button class="sign-out" @click="signOut"><span class="material-symbols-rounded">logout</span>Sign Out</button>
        </div>
      </section>
    </Transition>

    <Transition name="toast"><div v-if="toast" class="profile-toast" role="status"><span class="material-symbols-rounded">check_circle</span>{{ toast }}</div></Transition>
    <BottomNav :model-value="activeNav" @update:model-value="activeNav = $event" />
  </main>
</template>

<script setup>
import { reactive, ref, nextTick, onMounted, onUnmounted } from "vue"
import { gsap } from "@/animations/setup"
import { useNav } from "@/composables/useNav"
import useReducedMotion, { animationsEnabled, setAnimationsEnabled } from "@/composables/useReducedMotion"
import BottomNav from "@/components/navigation/BottomNav.vue"
import libraryArt from "@/assets/library.png"

const { current: activeNav } = useNav()
const isReducedMotion = useReducedMotion()
const root = ref(null)
const settingsOpen = ref(false)
const editing = ref(false)
const toast = ref("")
const user = reactive({ name: "Rohit Sharma", username: "rohitreads" })
const draftName = ref(user.name)
const draftUsername = ref(user.username)
const stats = [
  { icon: "books_movies_and_music", value: 12, label: "Books read" },
  { icon: "menu_book", value: 3, label: "Currently reading" },
  { icon: "library_books", value: 4, label: "Collections" },
  { icon: "bookmark", value: 8, label: "Saved books" }
]
const monthBars = [22, 46, 66, 38, 64, 88]
const monthLabels = ["J", "F", "M", "A", "M", "J"]
const activities = [
  { icon: "history", label: "Recently Read" },
  { icon: "inventory_2", label: "Borrow History" },
  { icon: "library_books", label: "My Collections" },
  { icon: "bookmark", label: "Saved Books" },
  { icon: "admin_panel_settings", label: "Library administration", route: "admin-dashboard" }
]
const settingsGroups = [
  {
    label: "PREFERENCES",
    items: [
      { icon: "notifications", title: "Notifications", description: "Manage your alerts and updates" },
      { icon: "dark_mode", title: "Appearance", description: "Dark mode and font size" },
      { icon: "motion_photos_on", title: "Animations", description: "Optional motion; respects your device preference" }
    ]
  },
  {
    label: "ACCOUNT",
    items: [
      { icon: "person", title: "Account Settings", description: "Personal information and security" },
      { icon: "shield", title: "Privacy", description: "Control your private reading data" }
    ]
  },
  {
    label: "SUPPORT",
    items: [
      { icon: "help", title: "Help & Support", description: "Get help and find answers" },
      { icon: "info", title: "About Libris", description: "Learn more about Libris" },
      { icon: "chat_bubble", title: "Send Feedback", description: "Help us improve Libris" }
    ]
  }
]
let entrance = null
let stateTween = null
let toastTimer = null

function showToast(message) {
  toast.value = message
  clearTimeout(toastTimer)
  toastTimer = setTimeout(() => { toast.value = "" }, 2200)
}
function cancelEdit() {
  draftName.value = user.name
  draftUsername.value = user.username
  editing.value = false
}
function saveProfile() {
  if (!draftName.value.trim() || !draftUsername.value.trim()) return
  user.name = draftName.value.trim()
  user.username = draftUsername.value.trim().replace(/^@/, "")
  editing.value = false
  showToast("Profile updated")
}
async function animateState(selector) {
  if (isReducedMotion.value) return
  await nextTick()
  stateTween?.kill()
  stateTween = gsap.fromTo(selector, { opacity: 0, x: 10 }, { opacity: 1, x: 0, duration: .42, ease: "power3.out" })
}
function openSettings() { settingsOpen.value = true; animateState(".settings-content") }
function closeSettings() { settingsOpen.value = false; animateState(".profile-content") }
function signOut() { activeNav.value = "auth" }

onMounted(() => {
  if (isReducedMotion.value) return
  entrance = gsap.timeline({ defaults: { ease: "power3.out" } })
    .fromTo(".profile-hero", { opacity: 0 }, { opacity: 1, duration: .7 })
    .fromTo(".profile-initial", { opacity: 0, scale: .82 }, { opacity: 1, scale: 1, duration: .45 }, .26)
    .fromTo(".profile-identity > *:not(.profile-initial)", { opacity: 0, y: 8 }, { opacity: 1, y: 0, duration: .35, stagger: .06 }, .32)
    .fromTo(".profile-section", { opacity: 0, y: 14 }, { opacity: 1, y: 0, duration: .45, stagger: .08 }, .4)
})
onUnmounted(() => { entrance?.kill(); stateTween?.kill(); clearTimeout(toastTimer) })
</script>

<style scoped>
.profile-view { min-height:100vh; padding-bottom:calc(5.2rem + env(safe-area-inset-bottom)); background:radial-gradient(circle at 50% 0,rgba(79,35,138,.12),transparent 25rem),var(--bg-primary); }
.profile-overview { width:min(100%,50rem); margin:auto; }
.profile-hero { position:relative; isolation:isolate; height:15rem; background-size:cover; background-position:62% 42%; border-bottom:1px solid var(--border-soft); }
.profile-hero::after { content:""; position:absolute; inset:0; z-index:-1; background:linear-gradient(0deg,rgba(8,6,16,.64),rgba(8,6,16,.12)),linear-gradient(90deg,rgba(8,6,16,.62),transparent 65%); }
.profile-hero header { display:flex; align-items:center; justify-content:space-between; padding:calc(1rem + env(safe-area-inset-top)) 1.5rem; }
.profile-brand { border:0; color:var(--text-primary); background:transparent; font:500 2rem var(--font-display); cursor:pointer; }
.profile-hero header > div { display:flex; gap:.55rem; }
.profile-hero header > div button { display:grid; place-items:center; width:2.55rem; height:2.55rem; border:1px solid rgba(243,240,234,.14); border-radius:11px; color:var(--text-primary); background:rgba(9,7,16,.54); cursor:pointer; }
.profile-hero header span { font-size:1.15rem; }
.profile-identity { position:relative; display:flex; flex-direction:column; align-items:center; min-height:10rem; padding:3.5rem 1rem 1rem; text-align:center; }
.profile-initial { position:absolute; top:-3.6rem; display:grid; place-items:center; width:7rem; height:7rem; border:1px solid rgba(244,235,255,.8); border-radius:50%; color:#fff; background:radial-gradient(circle at 45% 35%,#59348e,#160f27 70%); box-shadow:0 10px 35px rgba(0,0,0,.45),0 0 24px rgba(158,93,239,.18); font:650 3rem var(--font-body); }
.profile-identity h1 { margin:0; font:500 2.15rem/1 var(--font-display); }
.profile-identity > p { margin:.5rem 0 0; color:var(--accent); font-size:.85rem; }
.profile-identity > button { display:flex; align-items:center; gap:.45rem; margin-top:.8rem; border:0; color:#c68cff; background:transparent; font:.76rem var(--font-body); cursor:pointer; }
.profile-identity > button span { font-size:.95rem; }
.profile-edit { display:flex; flex-direction:column; gap:.45rem; width:min(100%,20rem); }
.profile-edit > input,.profile-edit > div:first-of-type { height:2.5rem; padding:0 .7rem; border:1px solid var(--border-soft); border-radius:9px; color:var(--text-primary); background:var(--surface-1); }
.profile-edit > div:first-of-type { display:flex; align-items:center; color:var(--accent); }
.profile-edit input { width:100%; border:0; outline:0; color:var(--text-primary); background:transparent; font:.8rem var(--font-body); text-align:center; }
.profile-edit__actions { display:flex; justify-content:center; gap:.65rem; margin-top:.3rem; }
.profile-edit__actions button { padding:.45rem .8rem; border:1px solid var(--border-soft); border-radius:8px; color:var(--text-secondary); background:transparent; font:.7rem var(--font-body); }
.profile-edit__actions button:last-child { border-color:var(--accent); color:var(--accent); }
.profile-content { padding:1rem 1.5rem 2rem; }
.profile-section { margin-top:2.2rem; }
.section-label,.settings-group > p { margin:0 0 .9rem; color:var(--text-secondary); font-size:.68rem; font-weight:600; letter-spacing:.1em; }
.profile-stats { display:grid; grid-template-columns:repeat(4,1fr); }
.profile-stats article { display:flex; flex-direction:column; align-items:center; min-width:0; padding:.25rem .4rem; border-right:1px solid var(--border-soft); text-align:center; }
.profile-stats article:last-child { border:0; }
.profile-stats span { color:#b873ff; font-size:1.5rem; }
.profile-stats strong { margin:.45rem 0 .25rem; font-size:1.2rem; font-weight:500; }
.profile-stats small { color:var(--text-tertiary); font-size:.64rem; line-height:1.35; }
.overview-charts { display:grid; grid-template-columns:1fr 1.35fr; align-items:end; gap:2rem; margin-top:1.5rem; }
.month-ring { display:grid; place-items:center; width:8rem; height:5.2rem; margin:auto; overflow:hidden; }
.month-ring::before { content:""; grid-area:1/1; width:7rem; height:7rem; border:8px solid transparent; border-top-color:#6e51b3; border-right-color:#a16fe9; border-radius:50%; transform:rotate(-45deg); }
.month-ring div { grid-area:1/1; display:flex; flex-direction:column; align-items:center; padding-top:1.5rem; }
.month-ring small,.month-ring span { color:var(--text-tertiary); font-size:.62rem; }
.month-ring strong { font-size:1.45rem; font-weight:500; }
.month-bars { display:flex; align-items:flex-end; justify-content:space-around; height:6.5rem; padding-bottom:1.2rem; border-bottom:1px solid var(--border-soft); }
.month-bars i { position:relative; width:.65rem; min-height:.7rem; border-radius:3px 3px 0 0; background:linear-gradient(0deg,#38294f,#8c5ee0); }
.month-bars i:last-child { background:linear-gradient(0deg,#583080,#ad63ff); box-shadow:0 0 10px rgba(169,95,255,.5); }
.month-bars span { position:absolute; top:calc(100% + .35rem); left:50%; transform:translateX(-50%); color:var(--text-tertiary); font:normal .6rem var(--font-body); }
.reading-world { display:grid; grid-template-columns:repeat(3,1fr); gap:.65rem; }
.reading-world article { position:relative; display:flex; flex-direction:column; align-items:center; min-height:8.1rem; padding:.9rem .45rem 1.4rem; border:1px solid var(--border-subtle); border-radius:10px; background:linear-gradient(145deg,rgba(23,19,33,.92),rgba(13,11,20,.9)); text-align:center; }
.reading-world small { color:var(--text-tertiary); font-size:.58rem; }
.reading-world h3 { margin:.55rem 0 0; font:500 .94rem/1.25 var(--font-display); }
.reading-world article > span { position:absolute; bottom:-1.1rem; display:grid; place-items:center; width:2.4rem; height:2.4rem; border:1px solid rgba(179,136,255,.18); border-radius:50%; color:#b76fff; background:#171023; box-shadow:0 6px 15px rgba(0,0,0,.35); font-size:1.2rem; }
.goal-card { display:flex; align-items:center; gap:1.35rem; width:100%; padding:1rem 1.1rem; border:1px solid var(--border-subtle); border-radius:11px; color:var(--text-primary); background:linear-gradient(135deg,rgba(22,18,32,.94),rgba(13,11,20,.92)); text-align:left; cursor:pointer; }
.goal-ring { display:grid; place-items:center; width:6rem; height:6rem; flex-shrink:0; border-radius:50%; background:conic-gradient(#aa6dff 0 56%,#2b223b 56% 100%); box-shadow:0 0 16px rgba(169,109,255,.14); }
.goal-ring::after { content:""; grid-area:1/1; width:4.8rem; height:4.8rem; border-radius:50%; background:#0e0b16; }
.goal-ring span { z-index:1; grid-area:1/1; font-size:1.2rem; }
.goal-card > div:last-child { flex:1; }
.goal-card h3 { margin:0; font:500 .92rem var(--font-body); }
.goal-card p { margin:.25rem 0 .55rem; color:var(--text-secondary); font-size:.75rem; }
.goal-card p strong { color:var(--accent); font-weight:600; }
.goal-card > div:last-child > i { display:block; height:4px; overflow:hidden; border-radius:99px; background:rgba(255,255,255,.06); }
.goal-card > div:last-child > i span { display:block; width:56%; height:100%; border-radius:inherit; background:linear-gradient(90deg,#8e5de0,#b978ff); }
.goal-card small { display:flex; align-items:center; gap:.35rem; margin-top:.7rem; color:var(--text-tertiary); font-size:.6rem; }
.goal-card small span { font-size:.8rem; }
.activity-list { overflow:hidden; border:1px solid var(--border-subtle); border-radius:10px; background:rgba(16,13,24,.72); }
.activity-list button { display:flex; align-items:center; gap:.8rem; width:100%; min-height:3.2rem; padding:.6rem .85rem; border:0; border-bottom:1px solid var(--border-subtle); color:var(--text-primary); background:transparent; text-align:left; cursor:pointer; }
.activity-list button:last-child { border:0; }
.activity-list button > span { color:#b96fff; font-size:1.15rem; }
.activity-list strong { flex:1; font-size:.72rem; font-weight:500; }
.activity-list i { color:var(--text-secondary); font-size:1rem; }
.settings-page { width:min(100%,50rem); min-height:100vh; margin:auto; padding:0 1.5rem 2rem; }
.settings-header { display:grid; grid-template-columns:2.5rem 1fr 2.5rem; align-items:center; height:6.5rem; padding-top:env(safe-area-inset-top); border-bottom:1px solid var(--border-subtle); }
.settings-header button { display:grid; place-items:center; width:2.4rem; height:2.4rem; border:1px solid var(--border-soft); border-radius:10px; color:var(--text-secondary); background:rgba(18,15,27,.7); }
.settings-header h1 { margin:0; font:500 1.8rem var(--font-display); text-align:center; }
.settings-content { padding-top:1.8rem; }
.settings-group { margin-bottom:2rem; }
.settings-group > div { overflow:hidden; border:1px solid var(--border-subtle); border-radius:12px; background:linear-gradient(145deg,rgba(21,18,30,.94),rgba(13,11,20,.92)); }
.settings-group button { display:flex; align-items:center; gap:.9rem; width:100%; min-height:5rem; padding:.8rem 1rem; border:0; border-bottom:1px solid var(--border-subtle); color:var(--text-primary); background:transparent; text-align:left; cursor:pointer; }
.settings-group button:last-child { border:0; }
.settings-icon { color:#b979ff; font-size:1.5rem; }
.settings-group button > span:nth-child(2) { display:flex; flex:1; flex-direction:column; gap:.2rem; }
.settings-group strong { font-size:.82rem; font-weight:500; }
.settings-group small { color:var(--text-tertiary); font-size:.66rem; }
.settings-group i { color:var(--text-secondary); font-size:1.1rem; }
.sign-out { display:flex; align-items:center; justify-content:center; gap:.7rem; width:100%; min-height:4.2rem; border:1px solid var(--border-subtle); border-radius:12px; color:#ff6c6c; background:rgba(19,15,26,.8); font:.8rem var(--font-body); cursor:pointer; }
.profile-toast { position:fixed; z-index:180; left:50%; bottom:6rem; display:flex; align-items:center; gap:.45rem; transform:translateX(-50%); padding:.65rem .9rem; border:1px solid rgba(179,136,255,.25); border-radius:10px; background:rgba(20,15,31,.95); box-shadow:0 14px 35px rgba(0,0,0,.34); font-size:.7rem; white-space:nowrap; }
.profile-toast span { color:var(--accent); font-size:1rem; }
.profile-page-enter-active,.profile-page-leave-active,.toast-enter-active,.toast-leave-active { transition:opacity .2s ease,transform .2s ease; }
.profile-page-enter-from { opacity:0; transform:translateX(8px); }
.profile-page-leave-to { opacity:0; transform:translateX(-8px); }
.toast-enter-from,.toast-leave-to { opacity:0; transform:translate(-50%,8px); }
@media (min-width:768px) {
  .profile-overview,.settings-page { border-inline:1px solid var(--border-subtle); }
  .profile-content { padding-inline:2.5rem; }
}
@media (max-width:430px) {
  .profile-hero { height:13rem; }
  .profile-hero header { padding-inline:1.1rem; }
  .profile-content,.settings-page { padding-inline:1rem; }
  .profile-initial { width:6.3rem; height:6.3rem; top:-3.2rem; font-size:2.7rem; }
  .profile-identity { padding-top:3.25rem; }
  .overview-charts { gap:1rem; }
  .goal-card { gap:1rem; padding-inline:.8rem; }
  .goal-ring { width:5.3rem; height:5.3rem; }
  .goal-ring::after { width:4.15rem; height:4.15rem; }
}
</style>
