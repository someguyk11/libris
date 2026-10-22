import { ref, watch } from "vue"

const routeNames = { "/admin": "admin-dashboard", "/admin/books": "admin-books", "/admin/shelves": "admin-shelves", "/admin/atlas": "admin-atlas", "/admin/settings": "admin-settings", "/rooms": "rooms", "/room": "room", "/explore": "explore", "/discover": "discover", "/library": "library", "/profile": "profile" }
function atlasIdFromHash() {
  const match = (typeof location === "undefined" ? "" : location.hash.slice(1)).match(/^\/admin\/atlas\/([^/]+)$/)
  try { return match ? decodeURIComponent(match[1]) : null } catch { return null }
}
const fromHash = () => atlasIdFromHash() ? "admin-atlas-detail" : routeNames[typeof location === "undefined" ? "" : location.hash.slice(1)] || "explore"
const current = ref(fromHash())
const selectedAtlasId = ref(atlasIdFromHash())
const selectedBook = ref(null)
let savedRoom = null
try { const value = JSON.parse(sessionStorage.getItem("libris-active-room")); if (value?.id && value.title) savedRoom = value } catch { /* Use the first room when no session is available. */ }
const selectedRoom = ref(savedRoom)
const returnTo = ref("explore")
const roomReturnTo = ref("rooms")

if (typeof window !== "undefined") {
  window.addEventListener("hashchange", () => { selectedAtlasId.value = atlasIdFromHash(); current.value = fromHash() })
  watch([current, selectedAtlasId], ([value, atlasId]) => {
    const route = value === "admin-atlas-detail" && atlasId ? `/admin/atlas/${encodeURIComponent(atlasId)}` : Object.keys(routeNames).find(key => routeNames[key] === value)
    if (route && location.hash !== `#${route}`) history.pushState(null, "", `#${route}`)
    window.scrollTo({ top: 0, behavior: "instant" })
  })
}

export function useNav() {

  return {
    current,
    selectedAtlasId,
    openAtlas(id) { selectedAtlasId.value = id; current.value = "admin-atlas-detail" },
    selectedBook,
    selectedRoom,
    enterRoom(room) {
      roomReturnTo.value = current.value === "room" ? "rooms" : current.value
      selectedRoom.value = room
      try { sessionStorage.setItem("libris-active-room", JSON.stringify(room)) } catch { /* The room remains available for this visit. */ }
      current.value = "room"
    },
    leaveRoom() { current.value = roomReturnTo.value },
    set(name) {
      current.value = name
    },
    openBook(book, source = current.value) {
      selectedBook.value = book
      returnTo.value = source === "book" ? "explore" : source
      current.value = "book"
    },
    closeBook() {
      current.value = returnTo.value || "explore"
    }
  }

}
