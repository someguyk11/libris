import { gsap } from "./setup"

/**
 * Hero entrance sequence.
 * Receives refs/elements for: logo, bell, greeting, title, search, artwork.
 * Quiet staggered load — no slides, no bouncing.
 */
export function animateHero({ logo, bell, greeting, title, search, art }) {

  if (!logo && !greeting) return null

  const tl = gsap.timeline({
    defaults: { ease: "power3.out" }
  })

  if (logo) {
    tl.fromTo(
      logo,
      { opacity: 0, y: -6 },
      { opacity: 1, y: 0, duration: 0.35 },
      0
    )
  }

  if (bell) {
    tl.fromTo(
      bell,
      { opacity: 0, scale: 0.9 },
      { opacity: 1, scale: 1, duration: 0.3 },
      0.08
    )
  }

  if (greeting) {
    tl.fromTo(
      greeting,
      { opacity: 0, y: 12 },
      { opacity: 1, y: 0, duration: 0.45 },
      0.12
    )
  }

  if (title) {
    tl.fromTo(
      title,
      { opacity: 0, y: 16 },
      { opacity: 1, y: 0, duration: 0.5 },
      0.22
    )
  }

  if (search) {
    tl.fromTo(
      search,
      { opacity: 0, y: 12 },
      { opacity: 1, y: 0, duration: 0.45 },
      0.32
    )
  }

  if (art) {
    tl.fromTo(
      art,
      { opacity: 0, scale: 1.03 },
      { opacity: 1, scale: 1, duration: 1.2, ease: "power2.out" },
      0.15
    )
  }

  return tl
}

/**
 * Subtle hero parallax on scroll. Movement stays within ~10px.
 */
export function createHeroParallax(art, { start = "top top", end = 500 } = {}) {

  if (!art) return null

  const tween = gsap.fromTo(
    art,
    { y: 0 },
    {
      y: 10,
      ease: "none",
      scrollTrigger: {
        trigger: art,
        start,
        end,
        scrub: true
      }
    }
  )

  return tween
}