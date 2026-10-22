#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <EEPROM.h>            

// ======================
// AP MODE & CONFIG
// ======================
DNSServer dnsServer;
const byte DNS_PORT = 53;
const char* ap_ssid = "Libot_Setup";
const char* ap_password = ""; 
const char* portal_ip_str = "192.168.4.1";
WebServer server(80);

String command = "";

String ssid = "";
String pass = "";

bool wifiConfigured = false;
bool wifiConnectingStarted = false;
bool wifiConnected = false;
volatile bool apModeActive = false; 

unsigned long connectionStartTime = 0;
const unsigned long connectionTimeout = 15000; 

enum ScanState { SCAN_IDLE, SCAN_REQUESTED, SCAN_RUNNING, SCAN_READY };
ScanState currentScanState = SCAN_IDLE;
unsigned long lastScanRequestTime = 0;
int foundNetworksCount = 0;

// ======================
// BACKEND CONFIG & LED
// ======================
String robotId = "robot1";
String serverBase = "https://libot-8okb.onrender.com";

int lastSequence = -1;
unsigned long lastBackendCheck = 0;
const unsigned long backendInterval = 2000; 

#define LED_PIN 2
unsigned long lastBlink = 0;
bool ledState = false;

// ======================
// MULTI-SLOT EEPROM CONFIG
// ======================
#define SLOT_SIZE 96             
#define MAX_SLOTS 5
#define EEPROM_SIZE 512             // Expanded to 512 bytes for isolation safety
#define OTA_FLAG_ADDR 500           // Safe, non-overlapping address block for flag status

void writeStringToEEPROM(int addrOffset, const String &str, int maxLen) {
  int len = str.length();
  if (len > maxLen - 1) len = maxLen - 1;
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + i, str[i]);
  }
  EEPROM.write(addrOffset + len, '\0'); 
}

String readStringFromEEPROM(int addrOffset, int maxLen) {
  char data[64]; 
  int i = 0;
  for (i = 0; i < maxLen - 1; i++) {
    char c = EEPROM.read(addrOffset + i);
    if (c == '\0' || c == 0xFF) break; 
    data[i] = c;
  }
  data[i] = '\0';
  return String(data);
}

void saveToStorage(String targetSsid, String targetPass) {
  int targetSlot = -1;
  for (int i = 0; i < MAX_SLOTS; i++) {
    String savedSsid = readStringFromEEPROM(i * SLOT_SIZE, 32);
    if (savedSsid == targetSsid) { targetSlot = i; break; }
  }
  if (targetSlot == -1) {
    for (int i = 0; i < MAX_SLOTS; i++) {
      String savedSsid = readStringFromEEPROM(i * SLOT_SIZE, 32);
      if (savedSsid.length() == 0 || savedSsid.startsWith("\xFF") || savedSsid[0] == 0) {
        targetSlot = i;
        break;
      }
    }
  }
  if (targetSlot == -1) targetSlot = 0;
  int baseAddr = targetSlot * SLOT_SIZE;
  for (int i = 0; i < SLOT_SIZE; i++) EEPROM.write(baseAddr + i, 0);
  writeStringToEEPROM(baseAddr, targetSsid, 32);       
  writeStringToEEPROM(baseAddr + 32, targetPass, 64);  
  EEPROM.commit();                      
  Serial.printf("[EEPROM] Network saved in Slot %d\n", targetSlot);
}

void wipeAllStorage() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("[EEPROM] Factory Wipe Performed Successfully.");
}

void deleteSlotFromStorage(int slot) {
  if (slot < 0 || slot >= MAX_SLOTS) return;
  int baseAddr = slot * SLOT_SIZE;
  for (int i = 0; i < SLOT_SIZE; i++) {
    EEPROM.write(baseAddr + i, 0);
  }
  EEPROM.commit();
  Serial.printf("[EEPROM] Cleared Slot %d\n", slot);
}

// ======================
// HARDWARE INTERRUPT EVENT MONITOR
// ======================
void onWiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_AP_START) {
    Serial.println("\n[Hardware Event] AP Interface detected ALIVE inside core stack.");
  }
}

// ======================
// HTML / CSS / JS DASHBOARD
// ======================
String getPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Libot Control Panel</title>
    <style>
        body { font-family: -apple-system, sans-serif; background: #0b0f1a; color: #ffffff; padding: 20px; margin: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; box-sizing: border-box; }
        .card { background: #121a2b; padding: 30px; border-radius: 16px; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3); width: 100%; max-width: 420px; box-sizing: border-box; }
        h2 { margin-top: 0; color: #4f46e5; font-size: 24px; text-align: center; }
        h4 { margin: 20px 0 10px 0; color: #9ca3af; font-weight: 500; border-bottom: 1px solid #1c2a44; padding-bottom: 5px; }
        .network, .history-panel { padding: 14px; margin: 8px 0; background: #1c2a44; border-radius: 10px;
        cursor: pointer; transition: background 0.2s ease, transform 0.1s; font-size: 14px; display: flex; justify-content: space-between; align-items: center;
        }
        .network:hover, .history-panel:hover { background: #2a3b63; transform: translateY(-1px);
        }
        .history-panel:active { transform: translateY(1px);
        }
        .forget-btn { background: transparent; border: none; color: #ef4444; font-size: 22px; cursor: pointer;
        padding: 0 8px; line-height: 1; transition: color 0.2s; }
        .forget-btn:hover { color: #dc2626;
        }
        input { width: 100%; padding: 12px; margin-top: 8px; margin-bottom: 12px; background: #1c2a44;
        border: 1px solid #2a3b63; border-radius: 10px; color: white; font-size: 15px; box-sizing: border-box; outline: none;
        }
        input:focus { border-color: #4f46e5;
        }
        button.main-btn { width: 100%; padding: 14px; background: #4f46e5; font-size: 16px; margin-top: 10px;
        border-radius: 10px; border: none; color: white; font-weight: bold; cursor: pointer;
        }
        button.main-btn:hover { background: #4338ca;
        }
        #status { color: #a78bfa; font-weight: 500; margin-bottom: 15px; text-align: center;
        background: rgba(79, 70, 229, 0.1); padding: 10px; border-radius: 8px; }
        #wifiList, #historyList { max-height: 160px;
        overflow-y: auto; }
    </style>
</head>
<body>
<div class="card">
    <h2>Libot Dashboard</h2>
    <div id="status">System Online</div>
    
    <h4>Saved Networks</h4>
    <div id="historyList">Checking history...</div>

    <h4>Scan Nearby WiFi</h4>
    <div id="wifiList">Scanning...</div>

    <h4>Manual Credentials</h4>
    <label>SSID</label>
    <input id="ssid" placeholder="Network Name">
    <label>Password</label>
    <input id="pass" type="password" placeholder="Password">
    <button id="connectBtn" class="main-btn" onclick="connectManual()">Connect & Save</button>
</div>

<script>
let scanInterval, statusInterval;
function selectSSID(name){ document.getElementById("ssid").value = name; }

async function loadHistory() {
    try {
        let response = await fetch("/history");
        let history = await response.json();
        let html = "";
        if(history.length === 0) {
            document.getElementById("historyList").innerHTML = "<div style='color:#6b7280;font-size:13px;padding:8px;'>No saved profiles found.</div>";
            return;
        }
        for(let item of history) {
            html += `<div class="history-panel" onclick="connectSaved(${item.slot})">
                <span>💾 ${item.ssid}</span>
                <button class="forget-btn" onclick="forgetNetwork(event, ${item.slot})">&times;</button>
            </div>`;
        }
        document.getElementById("historyList").innerHTML = html;
    } catch(e) {}
}

async function loadNetworks(){
    try {
        let response = await fetch("/scan");
        let networks = await response.json();
        let html = "";
        networks.sort((a,b) => b.rssi - a.rssi);
        for(let n of networks){
            html += `<div class="network" onclick="selectSSID('${n.ssid}')"><span>📶 ${n.ssid}</span><span style="color:#6b7280; font-size:12px;">${n.rssi} dBm</span></div>`;
        }
        if(html !== "") document.getElementById("wifiList").innerHTML = html;
    } catch(e) {}
}

async function connectManual(){
    let s = document.getElementById("ssid").value;
    let p = document.getElementById("pass").value;
    executeConnect({ ssid: s, pass: p });
}

async function connectSaved(slotId) {
    executeConnect({ slot: slotId });
}

async function forgetNetwork(event, slotId) {
    event.stopPropagation(); 
    if(confirm("Forget this network profile?")) {
        await fetch("/forget", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ slot: slotId })
        });
        loadHistory();
    }
}

async function executeConnect(payload) {
    document.getElementById("status").innerHTML = "Connecting to requested node...";
    try {
        await fetch("/connect", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });
        statusInterval = setInterval(async () => {
            let res = await fetch("/status");
            let data = await res.json();
            if(data.connected) {
                document.getElementById("status").innerHTML = "Connected Successfully!";
                clearInterval(statusInterval);
            }
         }, 1000);
    } catch(e) {}
}

loadHistory();
loadNetworks();
scanInterval = setInterval(loadNetworks, 5000);
</script>
</body>
</html>
)rawliteral";
  return page;
}

// ======================
// LOCAL WEB API ENDPOINTS
// ======================
void handleRoot() { server.send(200, "text/html", getPage()); }

void handleStatus() { 
  JsonDocument doc; 
  doc["connected"] = wifiConnected; 
  doc["wifiConfigured"] = wifiConfigured;
  doc["apModeActive"] = apModeActive;
  doc["currentSsid"] = wifiConnected ? WiFi.SSID() : "None";
  doc["ipAddress"] = wifiConnected ? WiFi.localIP().toString() : "192.168.4.1";
  doc["signalStrength"] = wifiConnected ? WiFi.RSSI() : 0;
  
  String out; 
  serializeJson(doc, out); 
  server.send(200, "application/json", out); 
}

void handleHistory() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < MAX_SLOTS; i++) {
    String savedSsid = readStringFromEEPROM(i * SLOT_SIZE, 32);
    if (savedSsid.length() > 0 && !savedSsid.startsWith("\xFF") && savedSsid[0] != 0) {
      JsonObject obj = arr.createNestedObject();
      obj["slot"] = i;
      obj["ssid"] = savedSsid;
    }
  }
  String out; serializeJson(arr, out);
  server.send(200, "application/json", out);
}

void handleForget() {
  String body = server.arg("plain"); 
  JsonDocument doc;
  if (!deserializeJson(doc, body) && doc.containsKey("slot")) {
    int slotTarget = doc["slot"].as<int>();
    deleteSlotFromStorage(slotTarget);
    server.send(200, "application/json", "{\"status\":\"forgotten\"}");
    return;
  }
  server.send(400, "application/json", "{\"status\":\"error\"}");
}

void handleScan() {
  if (currentScanState == SCAN_READY && foundNetworksCount >= 0) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < foundNetworksCount; i++) {
      if (WiFi.SSID(i).length() == 0) continue;
      JsonObject net = arr.createNestedObject(); 
      net["ssid"] = WiFi.SSID(i); 
      net["rssi"] = WiFi.RSSI(i);
    }
    String out; serializeJson(arr, out);
    server.send(200, "application/json", out);
    currentScanState = SCAN_IDLE;
    return;
  }
  server.send(200, "application/json", "[]");
  if (currentScanState == SCAN_IDLE) {
    currentScanState = SCAN_REQUESTED;
  }
}

void handleConnect() {
  String body = server.arg("plain");
  JsonDocument doc;
  if (deserializeJson(doc, body)) { server.send(400, "application/json", "{\"status\":\"error\"}"); return; }
  
  if (doc.containsKey("slot")) {
    int targetSlot = doc["slot"].as<int>();
    ssid = readStringFromEEPROM(targetSlot * SLOT_SIZE, 32);
    pass = readStringFromEEPROM(targetSlot * SLOT_SIZE + 32, 64);
  } else {
    ssid = doc["ssid"].as<String>();
    pass = doc["pass"].as<String>();
  }

  WiFi.scanDelete(); 
  currentScanState = SCAN_IDLE;

  server.send(200, "application/json", "{\"status\":\"ok\"}");
  wifiConfigured = true; wifiConnectingStarted = false;
  connectionStartTime = millis();
}

void handleNotFound() { 
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(307, "text/plain", ""); 
}

// ======================
// RUNTIME ENGINE CORES
// ======================
void runScanEngine() {
  if (currentScanState == SCAN_REQUESTED) {
    WiFi.scanNetworks(true);
    lastScanRequestTime = millis();
    currentScanState = SCAN_RUNNING;
  }
  if (currentScanState == SCAN_RUNNING) {
    int res = WiFi.scanComplete();
    if (res >= 0) {
      foundNetworksCount = res;
      currentScanState = SCAN_READY;
    } else if (res == -2 || (millis() - lastScanRequestTime > 8000)) {
      foundNetworksCount = 0;
      currentScanState = SCAN_IDLE;
    }
  }
}

void verifyWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiConfigured = false; wifiConnectingStarted = false; apModeActive = false;
    saveToStorage(ssid, pass); 
    WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
    Serial.printf("\n[WiFi] System connected to Node! IP: %s\n", WiFi.localIP().toString().c_str());
  } 
  else if (millis() - connectionStartTime > connectionTimeout) {
    wifiConfigured = false;
    wifiConnectingStarted = false;
    Serial.println("\n[WiFi] Link execution dropped out.");
  }
}

void tryAutoConnectFromStorage() {
  byte otaPending = EEPROM.read(OTA_FLAG_ADDR);
  
  if (otaPending != 1) {
    Serial.println("[Boot Load] No pending OTA reconfiguration flag set. Skipping auto-connect.");
    return;
  }
  
  Serial.println("\n[Boot Load] OTA Reconfiguration Detected! Clearing flag and scanning profiles...");
  EEPROM.write(OTA_FLAG_ADDR, 0);
  EEPROM.commit();
  delay(200);

  for (int i = MAX_SLOTS - 1; i >= 0; i--) {
    String savedSsid = readStringFromEEPROM(i * SLOT_SIZE, 32);
    String savedPass = readStringFromEEPROM(i * SLOT_SIZE + 32, 64);
    
    if (savedSsid.length() > 0 && !savedSsid.startsWith("\xFF") && savedSsid[0] != 0) {
      Serial.printf("[Boot Load] Prioritizing profile in Slot %d: SSID='%s'...\n", i, savedSsid.c_str());
      
      // 1. FORCE THE HARDWARE RADIO TO DROP THE OLD REGISTER STATE COMPLETELY
      WiFi.softAPdisconnect(true);
      WiFi.disconnect(true, true);
      delay(600); // Generous time slice for the physical transceiver to clear out
      
      WiFi.mode(WIFI_STA); 
      delay(200);
      
      // 2. Synchronize our global tracking states
      ssid = savedSsid;
      pass = savedPass;
      wifiConfigured = true;
      wifiConnectingStarted = true;
      
      // 3. FORCE OVERWRITE INTERNAL REGISTERS (The 'true' parameter forces NVS cache override)
      WiFi.begin(ssid.c_str(), pass.c_str(), 0, NULL, true);
      
      // 4. Poll for connection status (Expanded to 15 seconds for slower phone hotspots)
      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) { 
        delay(500);
        Serial.print(".");
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        wifiConfigured = false;
        wifiConnectingStarted = false;
        apModeActive = false;
        Serial.printf("\n[Boot Load] OTA Connection Successful! Real SSID: %s | IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return; 
      } else {
        Serial.printf("\n[Boot Load] Slot %d network failed to respond. Checking older profiles...\n", i);
        wifiConnected = false;
        wifiConfigured = false;
        wifiConnectingStarted = false;
      }
    }
  }
  Serial.println("[Boot Load] No responsive profile slots matched. Falling back to Captive Portal.");
}

void moveRobot() {
  // Add movement code here
}

void checkBackend() {
  if (!wifiConnected) return;
  unsigned long currentMillis = millis();
  if (currentMillis - lastBackendCheck >= backendInterval) {
    lastBackendCheck = currentMillis;
    
    HTTPClient http; 
    
    // STEP 1: Execute POST request to /robot/{robot_id}/telemetry
    String telemetryUrl = serverBase + "/robot/" + robotId + "/telemetry";
    http.begin(telemetryUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(1500);
    
    JsonDocument telemetryDoc; 
    telemetryDoc["wifi_ssid"] = WiFi.SSID();
    telemetryDoc["signal_strength"] = WiFi.RSSI();
    telemetryDoc["uptime_seconds"] = millis() / 1000;
    
    String telemetryJson;
    serializeJson(telemetryDoc, telemetryJson);
    
    int postCode = http.POST(telemetryJson); 
    http.end();
    
    // STEP 2: Execute GET request to poll commands & runtime properties
    String commandUrl = serverBase + "/robot/" + robotId + "/command";
    http.begin(commandUrl);
    http.setTimeout(1500);
    
    int getCode = http.GET(); 
    if (getCode == HTTP_CODE_OK) {
      String payload = http.getString(); 
      JsonDocument doc; 
      
      if (!deserializeJson(doc, payload) && doc.containsKey("data")) {
        JsonObject dataObj = doc["data"];
        
        // Handle Factory Reset Flag triggered remotely
        if (dataObj.containsKey("factory_reset") && dataObj["factory_reset"].as<bool>() == true) {
          wipeAllStorage();
          http.end();
          delay(500);
          ESP.restart();
        }
        
        // Handle Remote Over-The-Air Wi-Fi Provisions
        if (dataObj.containsKey("new_network") && !dataObj["new_network"].isNull()) {
          JsonObject newNet = dataObj["new_network"];
          ssid = newNet["ssid"].as<String>();
          pass = newNet["password"].as<String>();
          
          Serial.printf("\n[OTA] Received new target network: %s\n", ssid.c_str());
          saveToStorage(ssid, pass);
          
          // Write the flag to 1 so the ESP32 handles it cleanly on next boot
          EEPROM.write(OTA_FLAG_ADDR, 1);
          EEPROM.commit();
          
          http.end();
          Serial.println("[System] Saved credentials and set OTA flag. Safe-delay before rebooting...");
          delay(2000); 
          ESP.restart();
        }

        // Process Motion Command Sets
        if (dataObj.containsKey("command")) {
          command = dataObj["command"].as<String>();
          int sequence = dataObj["sequence"].as<int>();
          if (sequence != lastSequence) {
            Serial.printf("\nEXEC COMMAND: %s (Seq: %d)\n", command.c_str(), sequence);
            moveRobot()
            lastSequence = sequence;
          }
        }
      }
    } else {
      Serial.printf("[Backend Info] Command fetch issue. Error code: %d\n", getCode);
    }
    http.end();
  }
}

// ======================
// HARDWARE INITIALIZATION
// ======================
// ======================
// HARDWARE INITIALIZATION
// ======================
void setup() {
  Serial.begin(115200);
  delay(1000); 
  
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("EEPROM init failed!");
  }
  delay(500);
  Serial.println("\n[Firmware] Loading Secure Multi-Profile Controller... Stable Boot.");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 1. Initialize an active mode first so the network stack queues are allocated in core memory
  WiFi.mode(WIFI_AP_STA); 
  delay(100);
  
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.onEvent(onWiFiEvent);
  delay(500); // Crucial delay to let the radio completely settle its wipe states

  // 2. Map local web service dashboard routing paths
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/history", handleHistory);
  server.on("/forget", HTTP_POST, handleForget);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);

  // 3. Conditionally trigger OTA connection attempt
  tryAutoConnectFromStorage();

  // 4. Fall back to local access point setup if not connected online
  if (!wifiConnected) {
    Serial.println("[System] Initializing Fallback SoftAP radio deployment...");
    WiFi.mode(WIFI_AP_STA); 
    delay(200); 
    
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    delay(200);
    
    WiFi.softAP(ap_ssid, ap_password);
    delay(200);
    
    // Fire up the Captive Portal DNS configuration cleanly on the main thread
    IPAddress local_ip_address(192, 168, 4, 1);
    dnsServer.start(DNS_PORT, "*", local_ip_address);
    
    // Safely enable the loop engine background workers
    apModeActive = true; 
    Serial.println("[System] Captive Portal Active and Flag Armed Safely!");
  }

  // 5. CRITICAL FIX: Only activate the web listener once an interface (STA or AP) is definitively up!
  server.begin();
  Serial.println("[System] Local WebServer engine started successfully.");
}

void loop() {
  if (apModeActive) dnsServer.processNextRequest();
  server.handleClient();
  
  if (apModeActive) runScanEngine();
  if (wifiConfigured && !wifiConnectingStarted) {
    wifiConnectingStarted = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
  if (wifiConfigured && wifiConnectingStarted) verifyWiFiConnection();
  if (wifiConnected) checkBackend();
  
  if (wifiConnected) {
    if (millis() - lastBlink > 500) { 
      lastBlink = millis(); 
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState); 
    }
  } else { 
    digitalWrite(LED_PIN, LOW); 
  }
}