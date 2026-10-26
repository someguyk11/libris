<template>
  <section class="isbn-scanner" aria-label="ISBN camera scanner">
    <div class="isbn-scanner-actions"><button type="button" class="admin-button secondary" @click="running || starting ? stop() : start()"><span class="material-symbols-rounded">{{ running || starting ? 'videocam_off' : 'barcode_scanner' }}</span>{{ starting ? 'Cancel camera start' : running ? 'Stop camera' : 'Start camera' }}</button><small class="admin-muted">EAN-13 · rear camera preferred</small></div>
    <video v-show="running || starting" ref="video" autoplay muted playsinline aria-label="Live barcode camera preview" />
    <p v-if="running" class="admin-muted" role="status">{{ paused ? 'Scan paused while this ISBN is checked.' : 'Hold the ISBN barcode steady in view. Non-ISBN product barcodes are ignored.' }}</p>
    <p v-if="message" class="admin-form-error" role="alert">{{ message }}</p>
    <p v-else-if="!running" class="admin-muted">Start the camera only when ready. Camera access needs HTTPS or localhost and your permission. Manual entry is always available below.</p>
  </section>
</template>
<script setup>
import { onBeforeUnmount, ref } from 'vue'
import { cameraError, createIsbnCamera } from '@/services/isbnCamera.js'
const props = defineProps({ paused: Boolean })
const emit = defineEmits(['detected'])
const video = ref(null), running = ref(false), starting = ref(false), message = ref('')
const camera = createIsbnCamera()
let attempt = 0
function stop() { attempt++; camera.stop(); running.value = false; starting.value = false; if (video.value) video.value.srcObject = null }
async function start() {
  const current = ++attempt
  starting.value = true; message.value = ''
  try {
    const started = await camera.start(video.value, { detected: isbn => { message.value = ''; emit('detected', isbn) }, paused: () => props.paused, rejected: text => { message.value = text }, failed: text => { stop(); message.value = text } })
    if (current === attempt) running.value = started
  } catch (error) { if (current === attempt) { stop(); message.value = cameraError(error) } }
  finally { if (current === attempt) starting.value = false }
}
onBeforeUnmount(stop)
defineExpose({ stop })
</script>
<style scoped>
.isbn-scanner { border: 1px solid var(--admin-border, #ffffff15); border-radius: 10px; padding: 14px; }
.isbn-scanner-actions { display: flex; align-items: center; flex-wrap: wrap; gap: 12px; }
video { display: block; width: 100%; max-height: 220px; margin-top: 12px; border-radius: 8px; background: #08060c; object-fit: cover; }
.isbn-scanner p { margin: 10px 0 0; }
</style>
