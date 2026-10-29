#pragma once

// Self-contained: no internet, CDN, backend or browser storage required.
static const char LocalDrivePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Atlas local drive</title><style>
body{background:#101827;color:#edf2ff;font:16px system-ui;max-width:640px;margin:24px auto;padding:16px}
button{font:inherit;padding:20px;border:0;border-radius:12px;background:#2976c9;color:white;touch-action:none;user-select:none}
button:disabled{opacity:.4}.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}#front{grid-column:2}#left{grid-row:2}#stop{grid-row:2;background:#b62d39}#right{grid-row:2}#back{grid-column:2}
pre{white-space:pre-wrap;background:#1a293d;padding:14px;border-radius:12px}.actions{display:flex;gap:12px;margin-top:16px}.actions button{flex:1}#emergency{background:#b62d39}
input,select{box-sizing:border-box;font:inherit;padding:12px;margin:8px 0;width:100%;background:#1a293d;color:white;border:1px solid #617288;border-radius:8px}input[type=checkbox]{width:auto}label{display:block}.tabs{display:flex;flex-wrap:wrap;gap:12px;margin-bottom:16px}.tabs button{flex:1;min-width:0;padding:14px 10px}[hidden]{display:none!important}a{color:#8bc9ff}
</style><h1>Atlas · Local drive</h1>
<p>Keep Atlas in sight. Local routes use photointerrupters and ultrasonic checkpoints, not timing or backend commands.</p>
<nav class="tabs" aria-label="Atlas options"><button id="wifi-tab">Wi-Fi connection</button><button id="drive-tab" aria-pressed="true">Movement</button><button id="replay-tab" aria-pressed="false">Replay</button></nav>
<p id="mode-notice" hidden>Wi-Fi setup is selected. Driving is disabled. <button id="drive-enable">Enable drive &amp; replay</button></p>
<p id="connection">Connecting to ESP32…</p><section id="movement-panel">
<p>Hold a direction; release to stop. One encoder-measured step per press; left/right turn 90°. For recording, hold until the step is validated, then release. Interrupting a move invalidates the entire unsaved path.</p>
<div class="pad">
<button id="front" data-command="front" disabled>▲ Front</button><button id="left" data-command="left" disabled>◀ Left 90°</button>
<button id="stop">STOP</button><button id="right" data-command="right" disabled>Right 90° ▶</button><button id="back" data-command="back" disabled>▼ Back</button></div>
<h2>Save a path</h2><p id="record-status">Not recording</p>
<button id="record" disabled>Start new recording</button><button id="discard">Discard unsaved path</button>
<label>Route name<input id="route-name" maxlength="40" placeholder="Entrance to shelf"></label>
<label>Save slot<select id="save-slot"></select></label><p>Automatic fills an empty slot first, then replaces the oldest saved route when all three are full.</p><button id="save-route" disabled>Save path on ESP32</button>
</section><section id="replay-panel" hidden>
<h2>Replay a saved route</h2><label>Selected route<select id="replay-slot"></select></label>
<p><a id="route-json" target="_blank" rel="noopener">View selected route JSON</a></p>
<label><input id="start-confirmed" type="checkbox"> Atlas is at this route's original start position and heading; the area is clear.</label>
<p>Keep this page open and in the foreground. Wi-Fi/status loss cancels replay. No automatic resume after an interruption.</p>
<button id="replay-route" disabled>Replay selected route</button><button id="replay-stop">STOP replay</button><p id="replay-progress">No replay running</p>
</section><div class="actions"><button id="emergency">Latch E-stop</button><button id="reset">Reset safety</button></div>
<pre id="status">Waiting for local sensor/encoder status…</pre><p>Wi-Fi loss stops and latches safety. Reset never resumes motion. Use a physical power cut if necessary.</p>
<script>
let token='',sequence=0,held='',busy=false,ready=false,lastStatus=0,lastState=null,routeList='',replaySequence=0,modeChanging=false;
const connection=document.querySelector('#connection'),status=document.querySelector('#status');
const directions=[...document.querySelectorAll('[data-command]')];
function disable(){ready=false;directions.forEach(b=>b.disabled=true)}
async function send(command,extra={}){
 if(!token) return;
 const seq=++sequence;
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),500);
 try{
  const r=await fetch('/drive/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({token,sequence:seq,command,...extra}),signal:controller.signal,cache:'no-store'});
  if(!r.ok) throw Error('Command rejected ('+r.status+')');
 }catch(e){held='';disable();connection.textContent=e.message+' — movement unverified; heartbeat will stop Atlas.'}
 finally{clearTimeout(timer)}
}
function stop(command='stop'){held='';void send(command)}
async function poll(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),500);
 try{
  const r=await fetch('/drive/status',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error('Status unavailable');
  const s=await r.json();
  if(token&&token!==s.token){held='';disable();location.reload();return}
  token=s.token;sequence=Math.max(sequence,s.sequence);lastStatus=performance.now();
  lastState=s;ready=s.fresh&&s.ready&&!s.latched&&!modeChanging&&s.localMode!=='wifi'&&!s.modeChanging;directions.forEach(b=>b.disabled=!ready||s.storageBusy||s.replaying||((b.dataset.command==='left'||b.dataset.command==='right')&&!s.turnsCalibrated));
  document.querySelector('#mode-notice').hidden=s.localMode!=='wifi';
  connection.textContent=modeChanging||s.modeChanging?'Switching mode · motors inhibited':s.localMode==='wifi'?'Wi-Fi connection mode · driving disabled':ready?'Local control ready · '+s.stepCm+' cm per straight press':'Movement inhibited · '+(s.configurationError||s.fault);
  const names=['Front','Back','Left','Right'];
  status.textContent='Safety: '+s.fault+' | latch: '+s.latched+'\nNavigation: '+s.navigation+' | moving: '+s.moving+'\n'+s.message+'\nPhotointerrupter edges: L '+s.leftEdges+' / R '+s.rightEdges+'\n'+s.sensors.map((v,i)=>names[i]+': '+(v.valid?v.cm.toFixed(1)+' cm':'INVALID / STALE')).join('\n')+'\nDistance calibrated: '+s.distanceCalibrated+' | turns: '+s.turnsCalibrated;
  if(held&&!ready)stop();
  if(held==='replay'&&s.appliedSequence>=replaySequence&&!s.replaying&&!s.storageBusy){held='';document.querySelector('#start-confirmed').checked=false}
  renderRoutes(s);
 }catch(e){disable();if(held)stop();connection.textContent='ESP32 status lost — movement unverified'}
 finally{clearTimeout(timer);setTimeout(poll,300)}
}
directions.forEach(b=>{
 b.addEventListener('pointerdown',e=>{if(e.button!==0||!ready||held||busy)return;e.preventDefault();b.setPointerCapture(e.pointerId);held=b.dataset.command;busy=true;send(held).finally(()=>busy=false)});
 for(const name of ['pointerup','pointercancel','lostpointercapture'])b.addEventListener(name,()=>{if(held)stop()});
 b.addEventListener('contextmenu',e=>e.preventDefault());
});
setInterval(()=>{if(performance.now()-lastStatus>700){if(held)stop();disable();renderRoutes(lastState)}if(held&&!busy){busy=true;send(held==='replay'?'heartbeat':held).finally(()=>busy=false)}},200);
document.querySelector('#stop').onclick=()=>stop();document.querySelector('#emergency').onclick=()=>stop('emergency');document.querySelector('#reset').onclick=()=>stop('reset');
function renderRoutes(s){
 if(!s)return;
 const routes=s.routes||[],serialized=JSON.stringify(routes);
 if(serialized!==routeList){
  routeList=serialized;
  document.querySelector('#start-confirmed').checked=false;
  for(const [id,onlyValid]of [['save-slot',false],['replay-slot',true]]){
   const select=document.querySelector('#'+id),selected=select.value;select.replaceChildren();
   if(!onlyValid){const option=document.createElement('option');option.value='-1';option.textContent='Automatic · empty slot, otherwise replace oldest';select.append(option)}
   for(const route of routes){if(onlyValid&&!route.valid)continue;const option=document.createElement('option');option.value=String(route.slot);option.textContent='Slot '+(route.slot+1)+' · '+(route.valid?route.name+' ('+route.segments+' steps)':'Empty / invalid');select.append(option)}
   if([...select.options].some(o=>o.value===selected))select.value=selected;
  }
 }
 const occupied=Boolean(held||s.storageBusy||s.replaying||s.stepPending);
 document.querySelector('#record').disabled=!ready||occupied||s.recording||!s.storageReady;
 document.querySelector('#save-route').disabled=!ready||occupied||!s.storageReady||!s.routeCount||!(document.querySelector('#route-name').value||'').trim();
 document.querySelector('#replay-route').disabled=!ready||occupied||s.recording||!s.storageReady||!document.querySelector('#replay-slot').value||!document.querySelector('#start-confirmed').checked;
 document.querySelector('#replay-slot').disabled=occupied;
 document.querySelector('#record-status').textContent=(s.recording?'Recording':'RAM path')+' · '+(s.routeCount||0)+' / '+(s.maxRouteSegments||32)+' validated steps'+(s.stepPending?' · wait for validation before release':'');
 document.querySelector('#replay-progress').textContent=s.replaying?'Replay: '+(s.routeIndex||0)+' / '+(s.routeCount||0)+' completed steps':s.storageBusy?'Loading/saving locally…':'No replay running';
 const slot=document.querySelector('#replay-slot').value;
 const link=document.querySelector('#route-json');link.hidden=!slot;link.href=slot?'/drive/route/'+slot:'';
}
function tab(replay){
 if(held)stop();document.querySelector('#movement-panel').hidden=replay;document.querySelector('#replay-panel').hidden=!replay;
 document.querySelector('#drive-tab').setAttribute('aria-pressed',String(!replay));document.querySelector('#replay-tab').setAttribute('aria-pressed',String(replay));
}
async function modeRequest(path,data){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),1500);
 try{const r=await fetch(path,{method:data?'POST':'GET',cache:'no-store',signal:controller.signal,...(data?{headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}:{})});const value=await r.json();if(!r.ok)throw Error(value.error||'Mode request failed');return value}finally{clearTimeout(timer)}
}
async function changeMode(mode,target=''){
 if(modeChanging)return;
 if(mode==='wifi'&&(held||lastState?.recording)&&!confirm('Switch to Wi-Fi connection? Motion will stop and the unsaved recording will be discarded. Saved routes are kept.'))return;
 modeChanging=true;held='';disable();renderRoutes(lastState);
 try{
  await send('stop');
  const operation=await modeRequest('/local/mode',{mode}),deadline=performance.now()+15000;
  while(performance.now()<deadline){
   const s=await modeRequest('/status');
   if(s.operation!==operation.operation)throw Error('Network settings changed elsewhere. Check the current mode.');
   if(s.operationStatus==='failed')throw Error('Mode switch failed. Movement remains unverified.');
   if(s.operationStatus==='complete'&&s.localMode===mode){if(target)location.href=target;return}
   await new Promise(resolve=>setTimeout(resolve,150));
  }
  throw Error('Mode switch not confirmed. Wait for scanning/connecting to finish and check status.');
 }catch(error){connection.textContent=error.message+' Rejoin the Atlas AP if needed.'}
 finally{modeChanging=false}
}
document.querySelector('#wifi-tab').onclick=()=>changeMode('wifi','/');
document.querySelector('#drive-enable').onclick=()=>changeMode('drive');
document.querySelector('#drive-tab').onclick=()=>{tab(false);if(lastState?.localMode==='wifi')void changeMode('drive')};
document.querySelector('#replay-tab').onclick=()=>{tab(true);if(lastState?.localMode==='wifi')void changeMode('drive')};
document.querySelector('#record').onclick=()=>{if(ready&&!held&&!lastState?.storageBusy)void send('record')};
document.querySelector('#discard').onclick=()=>{if(confirm('Discard the unsaved RAM path? Saved routes are kept.'))stop('discard')};
document.querySelector('#save-route').onclick=()=>{
 if(!ready||held||lastState?.storageBusy)return;
 const name=document.querySelector('#route-name').value.trim(),value=document.querySelector('#save-slot').value;if(!name||value==='')return;
 const slot=Number(value),existing=lastState?.routes?.find(r=>r.slot===slot);
 if(existing?.valid&&!confirm('Replace saved route "'+existing.name+'" in this slot?'))return;
 void send('save',{slot,name});
};
document.querySelector('#replay-route').onclick=()=>{
 if(!ready||held||lastState?.storageBusy||lastState?.recording||!document.querySelector('#start-confirmed').checked)return;
 const value=document.querySelector('#replay-slot').value;if(value==='')return;
 held='replay';replaySequence=sequence+1;busy=true;send('replay',{slot:Number(value),startConfirmed:true}).finally(()=>busy=false);
};
document.querySelector('#replay-stop').onclick=()=>stop();
for(const id of ['route-name','start-confirmed','replay-slot'])document.querySelector('#'+id).addEventListener('input',()=>renderRoutes(lastState));
document.querySelector('#replay-slot').addEventListener('change',()=>{document.querySelector('#start-confirmed').checked=false;renderRoutes(lastState)});
addEventListener('blur',()=>{if(held)stop()});addEventListener('pagehide',()=>{if(held)stop()});document.addEventListener('visibilitychange',()=>{if(document.hidden&&held)stop()});addEventListener('keydown',e=>{if(e.key==='Escape')stop()});
poll();
</script></html>)HTML";
