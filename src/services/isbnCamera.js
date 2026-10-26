import { canonicalIsbn } from '../utils/isbn.js'

export function cameraError(error) {
  if (['NotAllowedError', 'PermissionDeniedError', 'SecurityError'].includes(error?.name)) return 'Camera permission was denied. Allow camera access in your browser settings, then restart, or enter an ISBN manually.'
  if (['NotFoundError', 'DevicesNotFoundError', 'OverconstrainedError'].includes(error?.name)) return 'No suitable camera is available. Connect a camera or enter the ISBN manually.'
  if (['NotReadableError', 'TrackStartError'].includes(error?.name)) return 'The camera is busy or unavailable. Close other camera apps, then restart, or enter the ISBN manually.'
  return error?.userMessage || 'Scanning failed. Stop and restart the camera, or enter the ISBN manually.'
}

async function loadDecoder() {
  const [{ BrowserMultiFormatReader }, { BarcodeFormat, DecodeHintType }] = await Promise.all([import('@zxing/browser'), import('@zxing/library')])
  return new BrowserMultiFormatReader(new Map([[DecodeHintType.POSSIBLE_FORMATS, [BarcodeFormat.EAN_13]]]), { delayBetweenScanAttempts: 180, delayBetweenScanSuccess: 1000 })
}

// Ownership of the stream stays here, including permission/start/close races.
export function createIsbnCamera({ mediaDevices = globalThis.navigator?.mediaDevices, secure = globalThis.isSecureContext, decoder = loadDecoder, now = Date.now } = {}) {
  let generation = 0, stream, controls, recent = new Map(), rejectAt = -Infinity
  function stop() {
    generation++
    controls?.stop(); controls = null
    stream?.getTracks().forEach(track => track.stop()); stream = null
    recent.clear()
  }
  async function start(video, { detected, rejected, failed, paused = () => false }) {
    stop()
    const run = generation
    if (!secure || !mediaDevices?.getUserMedia) {
      throw { userMessage: 'Camera scanning requires HTTPS (or localhost) and a browser with camera support. You can always enter an ISBN manually.' }
    }
    const reader = await decoder()
    if (run !== generation) return false
    const acquired = await mediaDevices.getUserMedia({ audio: false, video: { facingMode: { ideal: 'environment' }, width: { ideal: 1280 }, height: { ideal: 720 } } })
    if (run !== generation) { acquired.getTracks().forEach(track => track.stop()); return false }
    stream = acquired
    try {
      const next = await reader.decodeFromStream(acquired, video, (result, error) => {
        if (run !== generation || paused()) return
        if (result) {
          let isbn
          try { isbn = canonicalIsbn(result.getText()) } catch (reason) {
            if (now() - rejectAt > 2500) { rejectAt = now(); rejected?.(reason.message) }
            return
          }
          const last = recent.get(isbn)
          if (last !== undefined && now() - last < 2500) return
          recent.set(isbn, now())
          if (recent.size > 100) recent.delete(recent.keys().next().value)
          detected(isbn)
        } else if (error && !['NotFoundException', 'ChecksumException', 'FormatException'].includes(error.name)) {
          stop(); failed?.(cameraError(error))
        }
      })
      if (run !== generation) { next.stop(); acquired.getTracks().forEach(track => track.stop()); return false }
      controls = next
      return true
    } catch (error) {
      if (run !== generation) return false
      stop(); throw error
    }
  }
  return { start, stop }
}
