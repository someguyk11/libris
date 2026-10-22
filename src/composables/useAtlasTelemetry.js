import { onBeforeUnmount, onMounted } from 'vue'
import { apiMode, backend } from '@/services/api.js'

// Read status only: GET /command is reserved for firmware and consumes config flags.
export function useAtlasTelemetry(getUnits) {
  let timer, disposed = false
  async function poll() {
    if (!apiMode || disposed) return
    await Promise.allSettled(getUnits().map(async unit => {
      const identifier = unit.identifier
      try {
        const response = await backend.robotStatus(identifier)
        if (disposed || unit.identifier !== identifier) return
        unit.telemetry = response.data; unit.connectionError = ''
      } catch (error) {
        if (disposed || unit.identifier !== identifier) return
        unit.telemetry = null; unit.connectionError = error.message
      }
    }))
    if (!disposed) timer = setTimeout(poll, 2000)
  }
  onMounted(poll)
  onBeforeUnmount(() => { disposed = true; clearTimeout(timer) })
}
export function atlasLastSeen(telemetry) {
  return telemetry?.last_seen ? new Date(telemetry.last_seen * 1000).toLocaleString() : 'Never connected'
}
