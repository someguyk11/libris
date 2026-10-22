import { onMounted, onUnmounted } from "vue"

import { gsap } from "../animations/setup"
import useReducedMotion from "./useReducedMotion"

const prefersReducedMotion = () => useReducedMotion().value

/**
 * Detects whether an element lives inside a fixed-position overlay
 * (like our BottomSheet). For those ScrollTrigger can never fire,
 * so the reveal must play immediately instead of waiting for the
 * window to scroll.
 */
function isOverlay(target) {
  let node = target?.parentElement || null

  while (node && node !== document.documentElement) {
    const style = window.getComputedStyle(node)
    if (style.position === "fixed") return true
    node = node.parentElement
  }

  return false
}

function useReveal() {

  const cleanups = []

  onMounted(() => {
    if (prefersReducedMotion()) {
      document.body.classList.add("is-motion-reduced")
    }
  })

  onUnmounted(() => {
    cleanups.forEach((fn) => fn && fn())
    cleanups.length = 0
  })

  /**
   * Reveal a single section element once on scroll.
   * Inside overlays (bottom sheets) it animates immediately.
   */
  function reveal(target, options = {}) {

    if (!target || prefersReducedMotion()) return

    const overlay = isOverlay(target)

    gsap.set(target, {
      opacity: 0,
      y: options.y ?? 16
    })

    const tween = gsap.fromTo(
      target,
      { opacity: 0, y: options.y ?? 16 },
      {
        opacity: 1,
        y: 0,
        duration: options.duration ?? 0.5,
        ease: "power3.out",
        delay: options.delay ?? 0,
        ...(overlay
          ? {}
          : {
              scrollTrigger: {
                trigger: target,
                start: "top 86%",
                once: true
              }
            })
      }
    )

    cleanups.push(() => (tween.scrollTrigger ? tween.scrollTrigger.kill() : tween.kill()))
  }

  /**
   * Reveal a row of cards once, with a small stagger.
   * Inside overlays (bottom sheets) it plays immediately.
   */
  function stagger(target, items = null, options = {}) {

    if (!target || prefersReducedMotion()) return

    const targets = items instanceof NodeList
      ? items
      : target.children

    if (!targets || !targets.length) return

    const overlay = options.overlay ?? isOverlay(target)

    if (!overlay) {
      gsap.set(targets, {
        opacity: 0,
        y: options.y ?? 12
      })
    }

    const tween = gsap.fromTo(
      targets,
      { opacity: 0, y: options.y ?? 12 },
      {
        opacity: 1,
        y: 0,
        duration: options.duration ?? 0.4,
        ease: "power3.out",
        stagger: options.stagger ?? 0.06,
        delay: options.delay ?? 0,
        onComplete: options.onComplete,
        ...(overlay
          ? {}
          : {
              scrollTrigger: {
                trigger: target,
                start: "top 82%",
                once: true
              }
            })
      }
    )

    cleanups.push(() => (tween.scrollTrigger ? tween.scrollTrigger.kill() : tween.kill()))
  }

  return { reveal, stagger }
}

export default useReveal
