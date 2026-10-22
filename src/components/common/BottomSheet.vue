<template>
  <Teleport to="body">
    <Transition
      name="sheet"
      :css="false"
      @before-enter="beforeEnter"
      @enter="enter"
      @leave="leave"
    >
      <div
        v-if="open"
        ref="wrapper"
        class="sheet-wrapper"
        :class="{ 'sheet-wrapper--editorial': variant === 'editorial' }"
        role="dialog"
        aria-modal="true"
        :aria-label="label"
        tabindex="-1"
        @keydown.esc="dismiss"
      >
        <div
          ref="backdrop"
          class="backdrop"
          :class="{ 'is-dragging': dragging }"
          @click="dismiss"
        />

        <div
          ref="sheet"
          class="sheet"
          :class="{ 'is-dragging': dragging }"
          :style="{ transform: `translateY(${translateY}px)` }"
        >
          <div
            class="handle-area"
            role="button"
            aria-label="Drag to close preview"
            @pointerdown="startDrag"
          >
            <div class="handle"></div>
          </div>

          <div
            ref="content"
            class="content"
          >
            <slot />
          </div>
          <button v-if="variant === 'editorial'" class="sheet-close" aria-label="Close preview" @click="dismiss">
            <span class="material-symbols-rounded">close</span>
          </button>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<script setup>
import {
  ref,
  computed,
  watch,
  nextTick,
  onBeforeUnmount
} from "vue"

import { gsap } from "@/animations/setup"
import useReducedMotion from "@/composables/useReducedMotion"
const isReducedMotion = useReducedMotion()

const props = defineProps({
  variant: { type: String, default: "default" },
  label: { type: String, default: "Preview" },
  open: {
    type: Boolean,
    default: false
  }
})

const emit = defineEmits([
  "close"
])

const wrapper = ref(null)
const backdrop = ref(null)
const sheet = ref(null)
const content = ref(null)

const translateY = ref(0)
const dragging = ref(false)

let height = 0
let startY = 0
let rawDelta = 0
let lastY = 0
let lastTime = 0
let velocity = 0
let openTween = null
let leaveTween = null
let dragSession = null

const backdropOpacity = computed(() => {
  if (!height) return 0.45
  const travel = height * 0.5
  const fade = Math.min(1, Math.max(0, translateY.value / travel))
  return Number((0.45 * (1 - fade * 0.7)).toFixed(3))
})

function dismiss() {
  emit("close")
}

function beforeEnter(el) {
  height = sheet.value?.offsetHeight || window.innerHeight
  translateY.value = isReducedMotion.value ? 0 : height
  gsap.set(el, { opacity: 0 })
  gsap.set(backdrop.value, { opacity: 0 })
}

function enter(el, done) {
  document.body.style.overflow = "hidden"
  if (isReducedMotion.value) {
    translateY.value = 0
    gsap.set(el, { opacity: 1 })
    gsap.set(backdrop.value, { opacity: .45 })
    wrapper.value?.focus?.()
    done()
    return
  }

  openTween?.kill()
  gsap.to(backdrop.value, {
    opacity: 0.45,
    duration: 0.35,
    ease: "power3.out"
  })

  openTween = gsap.to(el, {
    opacity: 1,
    duration: 0.25,
    ease: "power3.out"
  })

  gsap.to(
    { y: translateY.value },
    {
      y: 0,
      duration: 0.4,
      ease: "power3.out",
      onUpdate: function () {
        translateY.value = this.targets()[0].y
      },
      onComplete: done
    }
  )

  wrapper.value?.focus?.()
}

function leave(el, done) {
  if (isReducedMotion.value) { done(); return }

  leaveTween?.kill()

  gsap.to(backdrop.value, {
    opacity: 0,
    duration: 0.3,
    ease: "power3.out"
  })

  gsap.to(
    { y: translateY.value },
    {
      y: height,
      duration: 0.3,
      ease: "power3.out",
      onUpdate: function () {
        translateY.value = this.targets()[0].y
      },
      onComplete: done
    }
  )
}

watch(
  () => props.open,
  (isOpen) => {
    document.body.style.overflow = isOpen ? "hidden" : ""
  }
)

onBeforeUnmount(() => {
  document.body.style.overflow = ""
  openTween?.kill()
  leaveTween?.kill()
  detachListeners()
})

function startDrag(e) {
  if (e.pointerType === "mouse" && e.button !== 0) return
  if (content.value && content.value.scrollTop > 0) return

  dragging.value = true
  startY = e.clientY
  lastY = e.clientY
  lastTime = performance.now()
  velocity = 0
  rawDelta = 0

  openTween?.kill()
  leaveTween?.kill()

  attachListeners()
}

function attachListeners() {
  window.addEventListener("pointermove", onDrag, { passive: true })
  window.addEventListener("pointerup", endDrag)
  window.addEventListener("pointercancel", endDrag)
}

function detachListeners() {
  window.removeEventListener("pointermove", onDrag)
  window.removeEventListener("pointerup", endDrag)
  window.removeEventListener("pointercancel", endDrag)
}

function onDrag(e) {
  if (!dragging.value) return

  const now = performance.now()
  const dt = now - lastTime
  if (dt > 0) velocity = (e.clientY - lastY) / dt
  lastY = e.clientY
  lastTime = now

  const delta = e.clientY - startY
  rawDelta = delta

  if (delta <= 0) {
    translateY.value = 0
    return
  }

  const damp = height * 0.45
  translateY.value = delta > damp
    ? damp + (delta - damp) * 0.2
    : delta
}

function endDrag() {
  if (!dragging.value) return
  dragging.value = false
  detachListeners()

  const threshold = Math.max(80, height * 0.2)
  const flung = velocity > 0.8

  if (rawDelta > threshold || (flung && rawDelta > 40)) {
    gsap.to(
      { y: translateY.value },
      {
        y: height,
        duration: Math.min(0.3, Math.max(0.14, (height - translateY.value) / (height * 4))),
        ease: "power3.in",
        onUpdate: function () {
          translateY.value = this.targets()[0].y
        },
        onComplete: () => {
          document.body.style.overflow = ""
          emit("close")
        }
      }
    )
  } else {
    gsap.to(
      { y: translateY.value },
      {
        y: 0,
        duration: 0.28,
        ease: "power3.out",
        onUpdate: function () {
          translateY.value = this.targets()[0].y
        }
      }
    )
  }
}
</script>

<style scoped>

.sheet-wrapper {

  position: fixed;

  inset: 0;

  z-index: 1000;

  display: flex;

  justify-content: center;

  align-items: flex-end;

}

.sheet-wrapper:focus {

  outline: none;

}

.backdrop {

  position: absolute;

  inset: 0;

  background: var(--overlay-dark);

  backdrop-filter: blur(10px) saturate(.85);

  -webkit-backdrop-filter: blur(10px) saturate(.85);

  will-change: opacity;

}

.backdrop.is-dragging {

  transition: none;

}

.sheet {

  position: relative;

  width: 100%;

  max-width: 660px;

  height: min(88vh, 840px);

  display: flex;

  flex-direction: column;

  background: var(--surface);

  border-radius: 28px 28px 0 0;

  border: 1px solid var(--border-soft);

  border-bottom: none;

  box-shadow: var(--shadow-deep);

  overflow: hidden;

  will-change: transform;

}

.sheet.is-dragging {

  transition: none;

}

.handle-area {

  flex: 0 0 auto;

  display: flex;

  justify-content: center;

  align-items: center;

  padding: 12px 0 10px;

  cursor: grab;

  touch-action: none;

}

.handle-area:active {

  cursor: grabbing;

}

.handle {

  width: 40px;

  height: 4px;

  border-radius: 999px;

  background: rgba(243, 240, 234, .22);

}

.content {

  flex: 1;

  overflow-y: auto;

  overscroll-behavior: contain;

  -webkit-overflow-scrolling: touch;

  padding: 0 1.5rem calc(2rem + env(safe-area-inset-bottom));

  scrollbar-width: none;

}

.content::-webkit-scrollbar {

  display: none;

}

@media (max-width: 480px) {

  .sheet {

    border-radius: 22px 22px 0 0;

  }

}

</style>
