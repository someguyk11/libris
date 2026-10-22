export function secondsRemaining(session, now = Date.now()) {
  return Math.max(0, session.running ? Math.ceil((session.deadline - now) / 1000) : session.seconds)
}
export function durationInSeconds(label = "42 min") {
  const hours = Number(label.match(/(\d+)\s*h/)?.[1] || 0)
  const minutes = Number(label.match(/(\d+)\s*min/)?.[1] || 0)
  return Math.max(60, (hours * 60 + minutes || 42) * 60)
}
export function formatCountdown(seconds) {
  return `${Math.floor(seconds / 60).toString().padStart(2, "0")}:${(seconds % 60).toString().padStart(2, "0")}`
}
