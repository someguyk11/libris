import { gsap } from "./setup"

/**
 * Page changes — subtle slide, max 8–16px.
 * direction: 1 = forward (enter from right), -1 = back (enter from left)
 */
export function enterPage(target, direction = 1) {

  if (!target) return null

  const x = 8 * direction

  const tween = gsap.fromTo(
    target,
    { opacity: 0, x },
    {
      opacity: 1,
      x: 0,
      duration: 0.4,
      ease: "power3.out"
    }
  )

  return tween
}

export function leavePage(target, direction = -1) {

  if (!target) return null

  const x = 8 * direction

  const tween = gsap.to(target, {
    opacity: 0,
    x,
    duration: 0.35,
    ease: "power3.out"
  })

  return tween
}