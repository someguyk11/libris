<template>
  <main class="rooms-view">
    <header><p class="eyebrow">READING ROOMS</p><h1>A little quiet,<br><em>in good company.</em></h1><p>Choose a place to settle in. Your timer and page stay with you on this device.</p></header>
    <div class="rooms-grid">
      <button v-for="(room, index) in rooms" :key="room.id" class="room-entry" :style="{ backgroundPosition: `${40 + index * 8}% center` }" @click="enterRoom(room)">
        <span class="room-entry__readers"><i></i>{{ room.readers }} readers</span><div><h2>{{ room.title }}</h2><p>{{ room.description }}</p><span>{{ room.timeLeft }} session <i class="material-symbols-rounded">east</i></span></div>
      </button>
    </div>
    <BottomNav model-value="rooms" @update:model-value="current = $event" />
  </main>
</template>
<script setup>
import { rooms } from "@/mock/data"
import BottomNav from "@/components/navigation/BottomNav.vue"
import { useNav } from "@/composables/useNav"
const { current, enterRoom } = useNav()
</script>
<style scoped>
.rooms-view{min-height:100vh;padding:3rem max(1.25rem,calc((100vw - 1100px)/2)) 7rem;background:var(--page-gradient)}
.rooms-view header{max-width:36rem;margin-bottom:2rem}.eyebrow{font-size:.65rem;letter-spacing:.18em;color:var(--accent)}h1{margin:.7rem 0 1rem;font:400 clamp(2.8rem,7vw,4.5rem)/.95 var(--font-display)}h1 em{color:#be91f5}.rooms-view header>p:last-child{color:var(--text-secondary);font-size:.85rem;line-height:1.7}
.rooms-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:1rem}.room-entry{display:flex;flex-direction:column;justify-content:space-between;min-height:19rem;padding:1.5rem;border:1px solid var(--border-soft);border-radius:16px;background:linear-gradient(0deg,rgba(8,6,16,.98),rgba(8,6,16,.12)),url('@/assets/twilight-lakeside.png') center/cover;color:var(--text-primary);text-align:left;cursor:pointer;transition:border-color .2s,translate .2s}.room-entry:hover{border-color:var(--accent);translate:0 -3px}.room-entry__readers{display:flex;align-items:center;gap:.4rem;font-size:.7rem;color:#d5c2ed}.room-entry__readers i{width:6px;height:6px;border-radius:50%;background:#79d8ad}.room-entry h2{font:400 2rem var(--font-display);margin-bottom:.4rem}.room-entry p{font-size:.75rem;color:var(--text-secondary);line-height:1.6}.room-entry div>span{display:flex;align-items:center;justify-content:space-between;margin-top:1rem;color:var(--accent-hover);font-size:.75rem}@media(max-width:600px){.rooms-grid{grid-template-columns:1fr}.room-entry{min-height:17rem}}
</style>
