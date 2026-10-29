#if defined(ATLAS_MOTOR_BENCH)
#include "AtlasConfig.h"
#include <WiFi.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <sys/socket.h>
#include <errno.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "The isolated motor bench requires the installed Arduino-ESP32 3.x framework."
#endif

// User-supplied wiring: right IN1/3 = GPIO26, right IN2/4 = GPIO25.
// No encoder convention, CPR, sensor pins or turn calibration is assumed.
namespace {
constexpr int Pins[] = {27, 33, 26, 25}; // left IN1/3, IN2/4; right IN1/3, IN2/4
constexpr int SleepPins[] = {19, 18}; // left, right; only right enabled for motion
constexpr uint32_t PulseMs = 300;
constexpr uint32_t DiagnosticMs = 3000;
constexpr uint32_t SettleMs = 200;
NetworkServer server(80);
NetworkClient client;
esp_timer_handle_t cutoff = nullptr;
bool healthy = false, moving = false, armed = false, latched = false;
bool diagnosing = false;
const char* lastResult = "Ready; confirm wheels lifted before arming";
uint32_t started = 0, stopped = 0, acceptedAt = 0, sequence = 0;
char token[33]{}, request[1024]{}, response[8500]{};
size_t received = 0, responseLength = 0, sent = 0;

// Task-dispatched timer: independent of the HTTP loop. Never wakes a driver.
void disableDrivers(void*) {
    for (int pin : SleepPins) digitalWrite(pin, LOW);
}
void stopMotors() {
    disableDrivers(nullptr);
    if (cutoff) esp_timer_stop(cutoff); // inactive is normal after expiry
    for (int pin : Pins) digitalWrite(pin, LOW);
    moving = false;
    diagnosing = false;
    stopped = millis();
}
bool pulse(char direction) {
    if (direction != 'f' && direction != 'b') return false;
    if (!healthy || latched || !armed || moving || diagnosing || millis() - stopped < SettleMs) return false;
    armed = false; // holding/repeated requests cannot extend or repeat the pulse
    disableDrivers(nullptr);
    // Clear every input first. Left inputs and left nSLEEP stay LOW.
    for (int pin : Pins) digitalWrite(pin, LOW);
    const int active = direction == 'f' ? Pins[2] : Pins[3];
    digitalWrite(active, HIGH);
    started = millis(); moving = true;
    digitalWrite(SleepPins[1], HIGH);
    if (esp_timer_start_once(cutoff, PulseMs * 1000ULL) != ESP_OK) {
        latched = true; stopMotors(); return false;
    }
    return true;
}

bool diagnoseSleep() {
    if (!healthy || latched || moving || diagnosing || millis() - stopped < SettleMs) return false;
    armed = false;
    stopMotors(); // All four motor inputs must be LOW before waking the drivers.
    if (!healthy) return false;
    started = millis(); diagnosing = true;
    for (int pin : SleepPins) digitalWrite(pin, HIGH);
    if (esp_timer_start_once(cutoff, DiagnosticMs * 1000ULL) != ESP_OK) {
        latched = true; stopMotors(); return false;
    }
    return true;
}

const char Page[] = R"HTML(<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Atlas MOTOR BENCH ONLY</title><style>body{font:18px system-ui;max-width:600px;margin:30px auto;padding:16px;background:#101827;color:white}button{font:inherit;padding:22px;margin:8px;touch-action:none}strong{color:#ffb55b}</style>
<h1>Atlas motor bench ONLY - RIGHT SIDE</h1><p><strong>ALL WHEELS MUST BE LIFTED OFF THE GROUND.</strong> No ultrasonic, stall, distance or angle protection. No recording/replay. Keep motor power cutoff within reach.</p>
<p>Right driver ONLY: IN1/3 = GPIO26, IN2/4 = GPIO25, SLEEP = GPIO18. Left driver stays disabled during motion. Digital HIGH/LOW, NO PWM: full motor-supply drive for up to 300 ms per press. Release to stop sooner. Front/Back labels require physical direction verification.</p>
<label><input id="lifted" type="checkbox"> Wheels are lifted and wiring is checked</label>
<p><button id="arm">Arm next pulse</button></p>
<button data-dir="f" disabled>Right side Front (GPIO26)</button><button data-dir="b" disabled>Right side Back (GPIO25)</button>
<p><button id="stop">STOP</button><button id="emergency">Latch E-stop</button><button id="reset">Reset (does not arm)</button></p><pre id="state">Loading...</pre>
<pre id="live">Waiting for firmware status...</pre>
<h2>SLEEP voltage diagnostic</h2><p>Disconnect the motor supply first; keep ESP32 USB power and common ground. All motor inputs remain LOW. GPIO18 (right) and GPIO19 (left) are enabled for up to 3 seconds, then disabled. This is NOT a movement test. Status is software state, not measured voltage.</p>
<label><input id="power-off" type="checkbox"> Motor supply is physically disconnected</label><button id="diagnose">Test SLEEP pins for 3 seconds</button>
<script>
let token='',seq=0,down=false,ready=false,generation=0,polling=false;const buttons=[...document.querySelectorAll('[data-dir]')],state=document.querySelector('#state');
function disable(){ready=false;buttons.forEach(b=>b.disabled=true)}
async function command(c){if(!token)throw Error('No session; reload');const n=++seq;const r=await fetch('/cmd?token='+token+'&seq='+n+'&dir='+c+(c==='d'?'&powerOff=1':''),{method:'POST',cache:'no-store',signal:AbortSignal.timeout(500)});const text=await r.text();if(!r.ok)throw Error(text);return text}
function stop(c='s'){generation++;down=false;disable();if(token)command(c).then(t=>state.textContent=t).catch(e=>state.textContent=e.message+'; stopping unverified, use physical cutoff')}
document.querySelector('#arm').onclick=async()=>{if(!document.querySelector('#lifted').checked)return;const g=++generation;disable();try{const t=await command('a');if(g!==generation)return;ready=true;buttons.forEach(b=>b.disabled=false);state.textContent=t}catch(e){disable();state.textContent=e.message}};
buttons.forEach(b=>{b.onpointerdown=e=>{if(e.button!==0||!ready||down)return;e.preventDefault();b.setPointerCapture(e.pointerId);down=true;ready=false;const g=generation;command(b.dataset.dir).then(t=>{if(g===generation)state.textContent=t}).catch(e=>{disable();state.textContent=e.message})};for(const name of ['pointerup','pointercancel','lostpointercapture'])b.addEventListener(name,()=>{if(down)stop()});b.oncontextmenu=e=>e.preventDefault()});
document.querySelector('#stop').onclick=()=>stop();document.querySelector('#emergency').onclick=()=>stop('x');document.querySelector('#reset').onclick=()=>stop('z');document.querySelector('#lifted').onchange=()=>stop();
window.addEventListener('blur',()=>stop());document.addEventListener('visibilitychange',()=>{if(document.hidden)stop()});
document.querySelector('#diagnose').onclick=async()=>{if(!document.querySelector('#power-off').checked){state.textContent='Disconnect motor supply and confirm first';return}generation++;disable();try{state.textContent=await command('d')}catch(e){state.textContent=e.message}};
document.querySelector('#power-off').onchange=()=>stop();
async function poll(){if(polling)return;polling=true;try{const r=await fetch('/status',{cache:'no-store',signal:AbortSignal.timeout(500)});if(!r.ok)throw Error('Status failed');const s=await r.json();if(token&&token!==s.token){stop();throw Error('ESP32 restarted; reload')}if(!token){token=s.token;seq=s.sequence;state.textContent='Connected; confirm wheels lifted, then arm'}document.querySelector('#live').textContent='Firmware: '+(!s.healthy?'FAULT':s.latched?'E-STOP':s.diagnostic?'SLEEP TEST (motor inputs LOW)':s.moving?'PULSE ACTIVE':s.armed?'ARMED':'STOPPED')+'\nLast result: '+s.result+'\nDigital HIGH/LOW (no PWM); pulse limit '+s.pulseMs+' ms';if(!s.healthy||s.latched||s.diagnostic)disable()}catch(e){disable();document.querySelector('#live').textContent=e.message+'; state unverified, use physical cutoff'}finally{polling=false}}
void poll();setInterval(poll,300);
</script>)HTML";
static_assert(sizeof(Page) + 256 < sizeof(response), "Bench HTTP response buffer too small");

void reply(const char* body, bool ok = true, bool json = false) {
    const int length = snprintf(response, sizeof(response),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: %u\r\n\r\n%s",
        ok ? "200 OK" : "409 Conflict", json ? "application/json" : "text/html; charset=utf-8",
        static_cast<unsigned>(strlen(body)), body);
    responseLength = length > 0 && static_cast<size_t>(length) < sizeof(response) ? length : 0;
    sent = 0;
}
void handleRequest() {
    if (!strncmp(request, "GET / HTTP/1.", 13) || !strncmp(request, "GET /drive HTTP/1.", 18)) {
        reply(Page); return;
    }
    if (!strncmp(request, "GET /status HTTP/1.", 19)) {
        char body[512];
        snprintf(body, sizeof(body), "{\"token\":\"%s\",\"sequence\":%lu,\"healthy\":%s,\"armed\":%s,\"moving\":%s,\"diagnostic\":%s,\"latched\":%s,\"mode\":\"digital\",\"pulseMs\":%lu,\"result\":\"%s\"}",
            token, static_cast<unsigned long>(sequence), healthy ? "true" : "false",
            armed ? "true" : "false", moving ? "true" : "false", diagnosing ? "true" : "false",
            latched ? "true" : "false", static_cast<unsigned long>(PulseMs), lastResult);
        reply(body, true, true); return;
    }
    char supplied[33]{}, direction = 0; unsigned long incoming = 0;
    if (sscanf(request, "POST /cmd?token=%32[0123456789abcdef]&seq=%lu&dir=%c HTTP/1.", supplied, &incoming, &direction) != 3 ||
        strcmp(supplied, token)) { reply("Invalid session; reload page", false); return; }
    // Authenticated STOP/E-stop always acts, even after a newer request arrived.
    // Disarming also prevents a delayed movement request from restarting a pulse.
    if (direction == 's' || direction == 'x') {
        if (incoming > sequence) sequence = incoming;
        armed = false; stopMotors();
        if (direction == 'x') latched = true;
        lastResult = direction == 'x' ? "E-stop latched; drivers disabled" : "STOP accepted; drivers disabled";
        Serial.println(lastResult);
        reply(lastResult); return;
    }
    if (incoming <= sequence) { reply("Stale request rejected; release and arm again", false); return; }
    sequence = incoming;
    bool ok = true;
    if (direction == 'z') {
        armed = false; stopMotors();
        ok = healthy;
        if (healthy) latched = false;
        lastResult = ok ? "Reset accepted; stopped and disarmed" : "Reset rejected; initialization/timer fault";
    } else if (direction == 'a') {
        ok = healthy && !latched && !moving && !diagnosing && millis() - stopped >= SettleMs;
        armed = ok;
        lastResult = ok ? "Armed for ONE pulse" : "Arm rejected; check fault, active test or stopped dwell";
    } else if (direction == 'd') {
        ok = strstr(request, "&dir=d&powerOff=1 HTTP/1.") && diagnoseSleep();
        lastResult = ok ? "SLEEP diagnostic accepted; inputs LOW; measure GPIO18/19 now" : "Diagnostic rejected; confirm motor power disconnected, stopped and fault-free";
    } else if (direction == 'f' || direction == 'b') {
        ok = pulse(direction);
        lastResult = ok ? "RIGHT-only HIGH/LOW accepted; no PWM; wheel motion NOT verified" : "Pulse rejected; release, reset faults and arm again";
    } else { ok = false; lastResult = "Unknown command rejected"; }
    Serial.printf("Bench command %c: %s\n", direction, lastResult);
    reply(lastResult, ok);
}
} // namespace

void setup() {
    Serial.begin(AtlasConfig::SerialBaud);
    for (int pin : SleepPins) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    healthy = true;
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(Pins[i], OUTPUT); digitalWrite(Pins[i], LOW);
    }
    esp_timer_create_args_t args{};
    args.callback = disableDrivers; args.dispatch_method = ESP_TIMER_TASK; args.name = "bench-cutoff";
    if (esp_timer_create(&args, &cutoff) != ESP_OK) healthy = false;
    stopMotors();
    WiFi.mode(WIFI_AP);
    const IPAddress ip(AtlasConfig::SetupIp);
    if (!WiFi.softAPConfig(ip, ip, IPAddress(255,255,255,0)) ||
        !WiFi.softAP(AtlasConfig::SetupSsid, AtlasConfig::SetupPassword)) {
        healthy = false; return;
    }
    uint8_t random[16]; esp_fill_random(random, sizeof(random));
    for (uint8_t i = 0; i < sizeof(random); ++i) snprintf(token + i * 2, 3, "%02x", random[i]);
    server.begin();
    Serial.println("MOTOR BENCH ONLY - RIGHT SIDE: wheels lifted! http://192.168.4.1/drive; IN1/3 = GPIO26, IN2/4 = GPIO25; digital HIGH/LOW, no PWM; left disabled for motion.");
}

void loop() {
    // Independent loop cutoff in addition to the timer-task nSLEEP cutoff.
    if ((moving || diagnosing) && (millis() - started >= (diagnosing ? DiagnosticMs : PulseMs) || WiFi.softAPgetStationNum() == 0)) {
        lastResult = WiFi.softAPgetStationNum() == 0 ? "AP disconnected; drivers disabled" :
            diagnosing ? "SLEEP diagnostic finished; drivers disabled" : "Pulse finished; drivers disabled; wheel motion NOT verified";
        stopMotors(); armed = false;
        Serial.println(lastResult);
    }
    if (!client) {
        client = server.accept();
        if (!client) return;
        client.setTimeout(10);
        acceptedAt = millis(); received = responseLength = sent = 0;
    }
    if (millis() - acceptedAt >= 500) { client.stop(); return; }
    if (!responseLength) {
        // At most 128 already-buffered bytes per loop; never wait for a header.
        for (uint8_t i = 0; i < 128 && client.available(); ++i) {
            if (received >= sizeof(request) - 1) { client.stop(); return; }
            request[received++] = client.read(); request[received] = 0;
            if (received >= 4 && !memcmp(request + received - 4, "\r\n\r\n", 4)) {
                handleRequest(); break;
            }
        }
    }
    if (responseLength) {
        // NetworkClient inherits Print::availableForWrite(), which always returns
        // zero in this framework. Send once per loop without waiting for TCP space.
        size_t chunk = responseLength - sent;
        if (chunk > 256) chunk = 256;
        const int written = ::send(client.fd(), response + sent, chunk, MSG_DONTWAIT);
        if (written > 0) {
            sent += static_cast<size_t>(written);
        } else if (written == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            client.stop(); return;
        }
        if (sent >= responseLength) client.stop();
    }
}
#endif
