#if defined(ATLAS_MOTOR_BENCH)
#include "AtlasConfig.h"
#include <WiFi.h>
#include <esp_random.h>
#include <esp_timer.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "The isolated motor bench requires the installed Arduino-ESP32 3.x framework."
#endif

// User-supplied wiring. GPIO12 is permitted ONLY in this isolated test build.
// No encoder convention, CPR, sensor pins or turn calibration is assumed.
namespace {
constexpr int Pins[] = {27, 33, 26, 12}; // left IN1/3, IN2/4; right IN1/3, IN2/4
constexpr int SleepPins[] = {19, 18}; // both switched together; side order immaterial
constexpr uint32_t PulseMs = 300;
constexpr uint32_t SettleMs = 200;
constexpr uint8_t Duty = 70; // 70/255, NOT 70 percent
NetworkServer server(80);
NetworkClient client;
esp_timer_handle_t cutoff = nullptr;
bool attached[4]{}, healthy = false, moving = false, armed = false, latched = false;
uint32_t started = 0, stopped = 0, acceptedAt = 0, sequence = 0;
char token[33]{}, request[1024]{}, response[6500]{};
size_t received = 0, responseLength = 0, sent = 0;

// Task-dispatched timer: independent of the HTTP loop. Never wakes a driver.
void disableDrivers(void*) {
    for (int pin : SleepPins) digitalWrite(pin, LOW);
}
void stopMotors() {
    disableDrivers(nullptr);
    if (cutoff) esp_timer_stop(cutoff); // inactive is normal after expiry
    for (uint8_t i = 0; i < 4; ++i)
        if (attached[i] && !ledcWrite(Pins[i], 0)) healthy = false;
    moving = false;
    stopped = millis();
}
bool pulse(char direction) {
    if (!healthy || latched || !armed || moving || millis() - stopped < SettleMs) return false;
    armed = false; // holding/repeated requests cannot extend or repeat the pulse
    const bool forward[] = {direction == 'f' || direction == 'r', direction == 'f' || direction == 'l'};
    disableDrivers(nullptr);
    for (uint8_t side = 0; side < 2; ++side) {
        const uint8_t active = side * 2 + (forward[side] ? 0 : 1);
        const uint8_t inactive = side * 2 + (forward[side] ? 1 : 0);
        if (!ledcWrite(Pins[inactive], 0) || !ledcWrite(Pins[active], Duty)) {
            healthy = false; latched = true; stopMotors(); return false;
        }
    }
    started = millis(); moving = true;
    for (int pin : SleepPins) digitalWrite(pin, HIGH);
    if (esp_timer_start_once(cutoff, PulseMs * 1000ULL) != ESP_OK) {
        latched = true; stopMotors(); return false;
    }
    return true;
}

const char Page[] = R"HTML(<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Atlas MOTOR BENCH ONLY</title><style>body{font:18px system-ui;max-width:600px;margin:30px auto;padding:16px;background:#101827;color:white}button{font:inherit;padding:22px;margin:8px;touch-action:none}strong{color:#ffb55b}</style>
<h1>Atlas motor bench ONLY</h1><p><strong>ALL WHEELS MUST BE LIFTED OFF THE GROUND.</strong> No ultrasonic, stall, distance or angle protection. No recording/replay. Keep motor power cutoff within reach.</p>
<p>One low-duty pulse, up to 300 ms per press. Release to stop sooner. Left/right are direction tests, NOT 90-degree turns.</p>
<label><input id="lifted" type="checkbox"> Wheels are lifted and wiring is checked</label>
<p><button id="arm">Arm next pulse</button></p>
<button data-dir="f" disabled>Front</button><button data-dir="b" disabled>Back</button><button data-dir="l" disabled>Left</button><button data-dir="r" disabled>Right</button>
<p><button id="stop">STOP</button><button id="emergency">Latch E-stop</button><button id="reset">Reset (does not arm)</button></p><pre id="state">Loading...</pre>
<script>
let token='',seq=0,down=false,ready=false;const buttons=[...document.querySelectorAll('[data-dir]')],state=document.querySelector('#state');
function disable(){ready=false;buttons.forEach(b=>b.disabled=true)}
async function command(c){const n=++seq;const r=await fetch('/cmd?token='+token+'&seq='+n+'&dir='+c,{method:'POST',cache:'no-store',signal:AbortSignal.timeout(500)});if(!r.ok)throw Error('Command rejected; stop/reset, then arm again');return r.text()}
function stop(c='s'){down=false;disable();if(token)command(c).catch(e=>state.textContent=e.message)}
document.querySelector('#arm').onclick=async()=>{if(!document.querySelector('#lifted').checked)return;try{await command('a');ready=true;buttons.forEach(b=>b.disabled=false);state.textContent='Armed for ONE pulse'}catch(e){disable();state.textContent=e.message}};
buttons.forEach(b=>{b.onpointerdown=e=>{if(e.button!==0||!ready||down)return;e.preventDefault();b.setPointerCapture(e.pointerId);down=true;ready=false;command(b.dataset.dir).then(()=>state.textContent='Pulse sent; release, then arm for another').catch(e=>state.textContent=e.message)};for(const name of ['pointerup','pointercancel','lostpointercapture'])b.addEventListener(name,()=>{if(down)stop()});b.oncontextmenu=e=>e.preventDefault()});
document.querySelector('#stop').onclick=()=>stop();document.querySelector('#emergency').onclick=()=>stop('x');document.querySelector('#reset').onclick=()=>stop('z');document.querySelector('#lifted').onchange=()=>stop();
window.addEventListener('blur',()=>stop());document.addEventListener('visibilitychange',()=>{if(document.hidden)stop()});
fetch('/status',{cache:'no-store'}).then(r=>r.json()).then(s=>{token=s.token;seq=s.sequence;state.textContent=s.healthy?'Ready: confirm wheels lifted, then arm':'PWM/timer initialization failed; do not test'}).catch(()=>state.textContent='ESP32 unavailable; reload');
</script>)HTML";

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
        char body[160];
        snprintf(body, sizeof(body), "{\"token\":\"%s\",\"sequence\":%lu,\"healthy\":%s}",
            token, static_cast<unsigned long>(sequence), healthy ? "true" : "false");
        reply(body, true, true); return;
    }
    char supplied[33]{}, direction = 0; unsigned long incoming = 0;
    if (sscanf(request, "POST /cmd?token=%32[0123456789abcdef]&seq=%lu&dir=%c HTTP/1.", supplied, &incoming, &direction) != 3 ||
        strcmp(supplied, token) || incoming <= sequence) { reply("Invalid/stale command", false); return; }
    sequence = incoming;
    bool ok = true;
    if (direction == 's' || direction == 'x' || direction == 'z') {
        armed = false; stopMotors();
        if (direction == 'x') latched = true;
        if (direction == 'z' && healthy) latched = false;
    } else if (direction == 'a') {
        ok = healthy && !latched && !moving && millis() - stopped >= SettleMs;
        armed = ok;
    } else if (direction == 'f' || direction == 'b' || direction == 'l' || direction == 'r') ok = pulse(direction);
    else ok = false;
    reply(ok ? "Accepted" : "Rejected: stop/reset and arm again", ok);
}
} // namespace

void setup() {
    Serial.begin(AtlasConfig::SerialBaud);
    for (int pin : SleepPins) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    healthy = true;
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(Pins[i], OUTPUT); digitalWrite(Pins[i], LOW);
        attached[i] = ledcAttach(Pins[i], AtlasConfig::PwmFrequencyHz, 8);
        if (!attached[i] || !ledcWrite(Pins[i], 0)) healthy = false;
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
    Serial.println("MOTOR BENCH ONLY: wheels lifted! http://192.168.4.1/drive; GPIO12 boot risk remains.");
}

void loop() {
    // Independent loop cutoff in addition to the timer-task nSLEEP cutoff.
    if (moving && (millis() - started >= PulseMs || WiFi.softAPgetStationNum() == 0)) {
        stopMotors(); armed = false;
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
        const int available = client.availableForWrite();
        if (available > 0) {
            size_t chunk = responseLength - sent;
            if (chunk > 256) chunk = 256;
            if (chunk > static_cast<size_t>(available)) chunk = available;
            sent += client.write(reinterpret_cast<const uint8_t*>(response + sent), chunk);
        }
        if (sent >= responseLength) client.stop();
    }
}
#endif
