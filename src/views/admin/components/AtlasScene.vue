<template>
  <div ref="root" class="atlas-scene">
    <svg :viewBox="`0 0 ${viewport.width} ${viewport.height}`" role="img" aria-label="Illustrative forward-facing 3D obstacle scene. Not live sensor data.">
      <defs>
        <linearGradient :id="skyId" x2="0" y2="1"><stop stop-color="#121026" /><stop offset="1" stop-color="#080a17" /></linearGradient>
        <linearGradient :id="floorId" x2="0" y2="1"><stop stop-color="#1e1537" /><stop offset="1" stop-color="#100d21" /></linearGradient>
        <radialGradient :id="glowId"><stop stop-color="#6b38b3" stop-opacity=".13" /><stop offset="1" stop-color="#6b38b3" stop-opacity="0" /></radialGradient>
      </defs>
      <rect :width="viewport.width" :height="viewport.height" :fill="`url(#${skyId})`" />
      <rect :y="viewport.height * .47" :width="viewport.width" :height="viewport.height * .53" :fill="`url(#${floorId})`" />
      <ellipse :cx="viewport.width / 2" :cy="viewport.height * .53" :rx="viewport.width * .58" :ry="viewport.height * .46" :fill="`url(#${glowId})`" />
      <g stroke="#8a55d2" stroke-opacity=".13" stroke-width=".8">
        <line v-for="(line, index) in gridLines" :key="index" :x1="line[0][0]" :y1="line[0][1]" :x2="line[1][0]" :y2="line[1][1]" />
      </g>
      <g stroke="#7450ac" stroke-opacity=".5" stroke-width=".8" stroke-linejoin="round">
        <polygon v-for="face in faces" :key="face.id" :points="face.points" :fill="shades[face.shade]" />
      </g>
      <path :d="`M ${viewport.width / 2} ${viewport.height - 20} L ${viewport.width * .28} ${viewport.height * .65} L ${viewport.width * .72} ${viewport.height * .65} Z`" fill="#a978fa" opacity=".035" />
      <path d="M 0 -15 L -12 10 L 0 4 L 12 10 Z" :transform="`translate(${viewport.width / 2}, ${viewport.height - 28})`" fill="#b386ff" />
    </svg>
    <div class="scene-label"><span></span>ILLUSTRATIVE VIEW</div>
    <div class="scene-compass" aria-label="Simulated heading"><span>N</span><span>E</span><span>S</span><span>W</span><i class="material-symbols-rounded" :style="{ transform: `rotate(${heading}deg)` }">navigation</i></div>
    <div class="scene-readout"><span>{{ heading }}°</span><span>FOV {{ resolvedCamera.fov }}°</span></div>
  </div>
</template>
<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, useId } from 'vue'
import { atlasSceneDefaults } from '@/mock/data/atlas.js'
import { sceneFaces, sceneGrid } from '../atlasScene.js'
const props = defineProps({
  camera: { type: Object, default: () => ({}) },
  obstacles: { type: Array, default: () => atlasSceneDefaults.obstacles },
  grid: { type: Object, default: () => atlasSceneDefaults.grid },
  obstacleScale: { type: Number, default: 1 },
})
const id = useId(), skyId = `${id}-sky`, floorId = `${id}-floor`, glowId = `${id}-glow`
const root = ref(null)
const viewport = reactive({ width: 1000, height: 650 })
let observer
onMounted(() => {
  observer = new ResizeObserver(([entry]) => {
    if (entry.contentRect.width > 0 && entry.contentRect.height > 0) {
      viewport.width = entry.contentRect.width
      viewport.height = entry.contentRect.height
    }
  })
  observer.observe(root.value)
})
onBeforeUnmount(() => observer?.disconnect())
const resolvedCamera = computed(() => ({ ...atlasSceneDefaults.camera, ...props.camera }))
const faces = computed(() => sceneFaces(props.obstacles, resolvedCamera.value, props.obstacleScale, viewport))
const gridLines = computed(() => sceneGrid(resolvedCamera.value, props.grid, viewport))
const heading = computed(() => Math.round((resolvedCamera.value.yaw * 180 / Math.PI + 360) % 360))
const shades = ['#19132e', '#281b47', '#141026', '#23173e', '#30204e']
</script>
