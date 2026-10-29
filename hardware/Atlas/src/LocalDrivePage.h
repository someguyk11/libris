#pragma once

// Self-contained: no internet, CDN, backend or browser storage required.
// Single HTML app served by the ESP32 at /drive. Dark theme, single-column,
// pill-button layout sized to fit inside the ESP32 PROGMEM + gzip budget.
static const char LocalDrivePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Atlas local drive</title><style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; }
body {
  background: #0a0b12; color: #e8eaf2;
  font: 16px/1.5 system-ui, -apple-system, Segoe UI, Roboto, Consolas, monospace;
  max-width: 640px; margin: 24px auto; padding: 16px;
}
h1 { font-size: 40px; font-weight: 600; letter-spacing: 0.5px; margin-bottom: 8px; color: #f2f3f7; }
.subtitle { font-size: 14px; color: #9aa3b8; margin-bottom: 20px; line-height: 1.5; }
.tabs { display: flex; gap: 10px; margin-bottom: 16px; }
.tab {
  flex: 1; padding: 14px; border: 1px solid #2d3148; border-radius: 10px;
  background: #131523; color: #a8b2d1; font-size: 15px; font-weight: 500;
  cursor: pointer; text-align: center;
}
.tab.active { background: #7c5cff; color: #ffffff; border-color: #7c5cff; }
.tab:disabled { opacity: 0.4; cursor: not-allowed; }
.status-bar { margin-bottom: 16px; padding: 8px 12px; border-radius: 8px;
  background: #131523; border: 1px solid #2d3148; font-size: 14px; color: #ff6b6b; }
.card {
  background: #131523; border: 1px solid #2d3148; border-radius: 12px;
  padding: 20px; margin-bottom: 20px;
}
.card h2 { font-size: 20px; margin-bottom: 16px; color: #f2f3f7; letter-spacing: 0.5px; }
.card p { font-size: 14px; color: #9aa3b8; margin-bottom: 14px; line-height: 1.5; }
.btn-group { display: grid; gap: 10px; }
button {
  padding: 14px; border: 1px solid #2d3148; border-radius: 10px;
  background: #1c1f30; color: #e8eaf2; font-size: 15px; font-weight: 500;
  cursor: pointer; text-align: center; min-height: 52px;
}
button:disabled { opacity: 0.4; cursor: not-allowed; border-color: #2d3148; }
button.primary { background: #7c5cff; border-color: #7c5cff; color: #fff; }
button.danger { background: rgba(139, 45, 45, 0.15); border-color: #8b2d2d; color: #ff6b6b; }
button.outline { background: transparent; border-color: #4a90ff; color: #4a90ff; }
button.secondary { border-color: #2d3148; color: #a8b2d1; }
button:active { transform: translateY(1px); }
.direction-pad { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; margin: 12px 0; }
.pad-front { grid-column: 2; }
.pad-left { grid-row: 2; grid-column: 1; }
.pad-stop { grid-row: 2; grid-column: 2; }
.pad-right { grid-row: 2; grid-column: 3; }
.pad-back { grid-column: 2; }
label { display: block; font-size: 13px; color: #9aa3b8; margin-bottom: 6px; }
input, select {
  width: 100%; padding: 12px; border: 1px solid #2d3148; border-radius: 8px;
  background: #0f111a; color: #e8eaf2; font-size: 15px; margin-bottom: 12px;
}
input:disabled, select:disabled { opacity: 0.4; background: #131523; }
.small { font-size: 13px; color: #7e86a0; margin-top: 4px; }
.checkbox-row { display: flex; align-items: flex-start; gap: 10px; margin-bottom: 14px; }
.checkbox-row input[type=checkbox] { width: auto; margin: 2px 0 0 0; accent-color: #7c5cff; }
.checkbox-row label { margin: 0; font-size: 14px; color: #e8eaf2; cursor: pointer; }
.json-link { font-size: 13px; color: #7c5cff; text-decoration: underline; cursor: pointer; display: block; margin-bottom: 14px; }
.json-link:hover { color: #a78bfa; }
.hidden { display: none !important; }
.footer { margin-top: 20px; padding-top: 16px; border-top: 1px solid #2d3148; }
.footer-actions { display: flex; gap: 12px; margin-bottom: 12px; }
.footer-actions button { flex: 1; min-height: 56px; }
.status-line { background: #0f111a; border: 1px solid #2d3148; border-radius: 8px;
  padding: 12px; font-size: 14px; color: #9aa3b8; }
.footer-note { margin-top: 12px; font-size: 13px; color: #7e86a0; line-height: 1.5; }
</style>

<h1>Atlas - Local drive</h1>
<p class="subtitle">Keep Atlas in sight. Local routes use photointerrupters and ultrasonic checkpoints, not timing or backend commands.</p>

<div class="tabs">
  <button class="tab" id="tab-wifi" onclick="switchTab('wifi')">Wi-Fi connection</button>
  <button class="tab active" id="tab-movement" onclick="switchTab('movement')">Movement</button>
  <button class="tab" id="tab-replay" onclick="switchTab('replay')">Replay</button>
</div>

<div id="status-bar" class="status-bar">ESP32 status lost — movement unverified</div>

<!-- Movement tab -->
<div id="panel-movement" class="card">
  <p>Hold a direction; release to stop. One encoder-measured step per press; left/right turn 90&deg;. For recording, hold until the step is validated, then release. Interrupting a move invalidates the entire unsaved path.</p>
  <div class="direction-pad btn-group">
    <button id="btn-front" class="pad-front" onclick="sendCmd('front')">Front</button>
    <button id="btn-left" class="pad-left" onclick="sendCmd('left')">Left 90&deg;</button>
    <button id="btn-stop" class="pad-stop danger" onclick="sendCmd('stop')">STOP</button>
    <button id="btn-right" class="pad-right" onclick="sendCmd('right')">Right 90&deg;</button>
    <button id="btn-back" class="pad-back" onclick="sendCmd('back')">Back</button>
  </div>
  <h2>Save a path</h2>
  <div id="recording-state">Not recording</div>
  <div style="display:flex;gap:10px;margin-bottom:16px;">
    <button class="primary" id="btn-record" onclick="sendCmd('record')">Start new recording</button>
    <button class="secondary" id="btn-discard" onclick="sendCmd('discard')">Discard unsaved path</button>
  </div>
  <label>Route name</label>
  <input id="route-name" type="text" placeholder="Entrance to shelf" maxlength="40">
  <label>Save slot</label>
  <select id="save-slot"><option value="">Select slot</option></select>
  <p class="small">Automatic fills an empty slot first, then replaces the oldest saved route when all three are full.</p>
  <button class="secondary" id="btn-save-path" onclick="savePath()" disabled>Save path on ESP32</button>
</div>

<!-- Replay tab -->
<div id="panel-replay" class="card hidden">
  <h2>Replay a saved route</h2>
  <label>Selected route</label>
  <select id="replay-slot"><option value="">Select a route</option></select>
  <a id="route-json-link" class="json-link" href="#" target="_blank" rel="noopener">View selected route JSON</a>
  <div class="checkbox-row">
    <input type="checkbox" id="start-confirmed">
    <label for="start-confirmed">Atlas is at this route&rsquo;s original start position and heading; the area is clear.</label>
  </div>
  <p class="small">Keep this page open and in the foreground. Wi-Fi/status loss cancels replay. No automatic resume after an interruption.</p>
  <div style="display:flex;gap:10px;margin-top:10px;">
    <button class="secondary" id="btn-replay" onclick="sendReplay()" disabled>Replay selected route</button>
    <button class="danger" id="btn-stop-replay" onclick="sendCmd('stop')">STOP replay</button>
  </div>
  <div id="replay-status">No replay running</div>
</div>

<div class="footer">
  <div class="footer-actions">
    <button class="danger" onclick="sendCmd('emergency')">Latch E-stop</button>
    <button class="outline" onclick="sendCmd('reset')">Reset safety</button>
  </div>
  <div id="status-line" class="status-line">Waiting for local sensor/encoder status...</div>
  <p class="footer-note">Wi-Fi loss stops and latches safety. Reset never resumes motion. Use a physical power cut if necessary.</p>
</div>

<script>
  let token = '', sequence = 0, held = '', busy = false, replaySequence = 0, pollTimer = null;

  function switchTab(tab) {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.getElementById('tab-' + tab).classList.add('active');
    document.querySelectorAll('[id^=panel-]').forEach(p => p.classList.add('hidden'));
    document.getElementById('panel-' + tab).classList.remove('hidden');
    if (tab === 'replay') loadRoutes();
  }

  function statusBar(msg, color) {
    const el = document.getElementById('status-bar');
    el.textContent = msg;
    el.style.color = color || '#ff6b6b';
  }
  function statusLine(msg) {
    document.getElementById('status-line').textContent = msg;
  }

  async function poll() {
    try {
      const r = await fetch('/drive/status', { cache: 'no-store' });
      if (!r.ok) throw Error('Status failed');
      const s = await r.json();
      if (token && token !== s.token) { location.reload(); return; }
      token = s.token;
      sequence = Math.max(sequence, s.sequence);
      const ready = s.fresh && s.ready && !s.latched && s.localMode !== 'wifi' && !s.modeChanging;
      statusBar(s.configurationError || s.fault || (s.moving ? 'Movement active' : 'Local control ready'),
        s.fault && s.fault !== 'None' ? '#ff6b6b' : '#4ade80');
      statusLine(s.message || 'Waiting for local sensor/encoder status...');

      const recordBtn = document.getElementById('btn-record');
      const discardBtn = document.getElementById('btn-discard');
      const saveBtn = document.getElementById('btn-save-path');
      const recordState = document.getElementById('recording-state');
      recordBtn.disabled = !ready || s.storageBusy || s.recording;
      discardBtn.disabled = !ready || s.storageBusy || s.recording;
      saveBtn.disabled = !ready || s.storageBusy || !s.storageReady || !s.routeCount ||
        !document.getElementById('route-name').value.trim();
      recordState.textContent = s.recording ? 'Recording' : 'Not recording';
      recordState.textContent += ' · ' + (s.routeCount || 0) + ' / ' + (s.maxRouteSegments || 32) + ' validated steps';

      const replayBtn = document.getElementById('btn-replay');
      const stopReplayBtn = document.getElementById('btn-stop-replay');
      const replayStatus = document.getElementById('replay-status');
      const slotVal = document.getElementById('replay-slot').value;
      const replayReady = ready && !s.storageBusy && !s.recording && slotVal !== '';
      replayBtn.disabled = !replayReady || !document.getElementById('start-confirmed').checked;
      stopReplayBtn.disabled = !s.replaying;
      replayStatus.textContent = s.replaying
        ? ('Replaying: ' + (s.routeIndex || 0) + ' / ' + (s.routeCount || 0) + ' completed steps')
        : (s.storageBusy ? 'Loading/saving locally…' : 'No replay running');

      if (held === 'replay' && s.appliedSequence >= replaySequence && !s.replaying && !s.storageBusy) {
        held = ''; document.getElementById('start-confirmed').checked = false;
      }
      document.querySelectorAll('button').forEach(b => {
        if (b.id === 'btn-stop' || b.id === 'btn-stop-replay') return;
        if (b.dataset.command && (b.id === 'btn-front' || b.id === 'btn-back' || b.id === 'btn-left' || b.id === 'btn-right')) {
          b.disabled = !ready || s.storageBusy || s.replaying;
        }
      });
      if (held && !ready) stop();
    } catch (e) {
      statusBar('ESP32 status lost — movement unverified');
    }
    clearTimeout(pollTimer);
    pollTimer = setTimeout(poll, 300);
  }

  function send(cmd, extra = {}) {
    if (!token) return;
    const seq = ++sequence;
    const body = Object.assign({ token, sequence: seq, command: cmd }, extra);
    fetch('/drive/command', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body), cache: 'no-store'
    }).then(r => {
      if (!r.ok) throw Error('Command rejected (' + r.status + ')');
    }).catch(e => {
      held = ''; busy = false;
      statusBar(e.message + ' — movement unverified');
    });
  }
  function stop(cmd = 'stop') { held = ''; busy = false; send(cmd); }
  function sendCmd(cmd) {
    held = cmd; busy = true; send(cmd).finally(() => { busy = false; });
  }
  function sendReplay() {
    const slot = document.getElementById('replay-slot').value;
    if (slot === '') return;
    held = 'replay'; replaySequence = sequence + 1; busy = true;
    send('replay', { slot: Number(slot), startConfirmed: true }).finally(() => { busy = false; });
  }
  function savePath() {
    const name = document.getElementById('route-name').value.trim();
    const slot = document.getElementById('save-slot').value;
    if (!name || slot === '') return;
    send('save', { slot: Number(slot), name });
  }

  async function loadRoutes() {
    try {
      const r = await fetch('/drive/status', { cache: 'no-store' });
      const s = await r.json();
      const select = document.getElementById('replay-slot');
      select.innerHTML = '<option value="">Select a route</option>';
      (s.routes || []).forEach(route => {
        const o = document.createElement('option');
        o.value = route.slot;
        o.textContent = 'Slot ' + (route.slot + 1) + ' · ' +
          (route.valid ? route.name + ' (' + route.segments + ' steps)' : 'Empty / invalid');
        select.appendChild(o);
      });
      const link = document.getElementById('route-json-link');
      link.hidden = true;
      select.onchange = function () {
        link.hidden = !this.value;
        link.href = this.value ? '/drive/route/' + this.value : '#';
        document.getElementById('start-confirmed').checked = false;
      };
    } catch (e) {}
  }

  document.getElementById('route-name').addEventListener('input', () => {
    document.getElementById('btn-save-path').disabled = !document.getElementById('route-name').value.trim();
  });
  document.getElementById('start-confirmed').addEventListener('input', () => {
    document.getElementById('btn-replay').disabled = !document.getElementById('start-confirmed').checked;
  });

  document.addEventListener('DOMContentLoaded', poll);
  window.addEventListener('blur', () => { if (held) stop(); });
  document.addEventListener('visibilitychange', () => { if (document.hidden && held) stop(); });
</script>
)HTML";
