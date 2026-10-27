# Guest discovery and Atlas UI preview

This is a frontend-only simulation, not a public movement-authorisation system.
It makes no guidance/auth API calls and sends no robot commands. The preview
label remains visible. Catalogue, locations, availability, battery and progress
are illustrative; do not use these shelf directions in a real library.

## Routes

The default site entry opens `#/auth`, with a Continue as guest — Book guidance link.
The separate public guest entry is `/guide` (or `/#/guide`), with these steps:

- `/guide/book/:bookId`
- `/guide/setup`
- `/guide/session/:sessionId` — moving, arrival, assistance and completion states

These follow the existing hash-router convention after initial loading. Netlify
rewrites `/guide` and `/guide/*` to the Vue entry point, without changing the API proxy.
Guest query, layout and selection survive reloads through optional `sessionStorage`.
Missing step prerequisites return visitors to discovery/selection. A new search
after completion resets the previous guest selection and session.
Continue on Atlas selection opens the session directly. Legacy `/guide/verify`
links redirect to selection; there is no Atlas code screen or code requirement.

## Manual UI check

1. Open `/guide`. Search by title, author or ISBN, clear the query, and toggle layout.
2. Select The Alchemist, choose Guide me there, then select an available Atlas.
3. Continue to open Guidance in Progress immediately, without entering a code.
4. Watch the four simulated stages (~20 seconds), inspect View book location,
   then End session and confirm. Search another book resets the flow.
5. For the alternate exception state, start a fresh flow at
   `/?scenario=obstacle#/guide`. Guidance pauses during transit. Continue is rejected
   until the simulated obstacle clears (~6 seconds), after which it resumes.

The pure model is tested in `tests/guest-guide.test.js`. Run `npm run check` for all
frontend tests and the production build.

## Integration boundaries

- Replace `src/mock/data/guestGuide.js` with a documented **guest-safe** catalogue
  adapter, including copy availability and real library locations.
- Replace mock Atlas selection, session creation/status,
  obstacle clearance and cancellation with documented guest-safe endpoints.
- The server must authorize all movement and validate device readiness. Client
  state must never be trusted for real guidance.
- The preview does not call authenticated guidance APIs. Those APIs now dispatch
  without Atlas codes but retain reader authentication, ownership and robot safety
  checks. Account email verification and password-reset codes are unchanged.

Local cover images are from Open Library by ISBN. The hero nebula is from Unsplash
(`photo-1462331940025-496dfbfc7564`). All imagery is served locally; SVG interface
icons are implemented in `GuideIcon.vue`. Existing fonts, BookCover and AppButton
are reused; there are no new dependencies.
