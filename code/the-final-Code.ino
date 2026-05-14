#include "HomeSpan.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// =============================================================================
//                             CONFIGURATION
// =============================================================================
const char* ssid = "Vodafone_VDSL_DD78";
const char* password = "V9EWBDNRRWRTW5LM";

const char* API_TOKEN = "EZZ_SECURE_TOKEN"; 
const char* WEB_PIN   = "1744";            

// --- Pin Assignments ---
const int PIN_SERVO  = 26;
const int PIN_LED_W  = 25; 
const int PIN_LED_Y  = 33; 
const int PIN_LED_G  = 32; 
const int PIN_BUZZER = 27;

// --- Servo Settings ---
const int SERVO_LOCKED_POS = 0;
const int SERVO_OPEN_POS   = 90;

// =============================================================================
//                             GLOBAL OBJECTS
// =============================================================================
WebServer server(8080); 
WebSocketsServer webSocket = WebSocketsServer(81);
Servo gateServo;

bool networkServicesStarted = false; 

// =============================================================================
//                             SYSTEM STATE
// =============================================================================
enum GateState { STATE_LOCKED, STATE_AUTH_1, STATE_AUTH_2, STATE_OPENING, STATE_OPEN, STATE_CLOSING };
GateState currentState = STATE_LOCKED;
unsigned long stateStartTime = 0;

void broadcastStatus(String status) {
    if (networkServicesStarted) {
        String json = "{\"status\":\"" + status + "\"}";
        webSocket.broadcastTXT(json);
    }
}

// =============================================================================
//                             HOMESPAN
// =============================================================================
struct MyGate : Service::GarageDoorOpener {
    SpanCharacteristic *current;
    SpanCharacteristic *target;
    
    MyGate() : Service::GarageDoorOpener() {
        current = new Characteristic::CurrentDoorState(1);
        target = new Characteristic::TargetDoorState(1);
    }

    boolean update() override {
        if (target->getNewVal() == 0) triggerOpen();
        else triggerClose();
        return true;
    }

    void loop() override {
        if (currentState == STATE_OPEN && current->getVal() != 0) current->setVal(0);
        else if (currentState == STATE_LOCKED && current->getVal() != 1) {
            current->setVal(1);
            target->setVal(1);
        }
    }

    void triggerOpen() {
        if (currentState == STATE_LOCKED || currentState == STATE_CLOSING) {
            currentState = STATE_AUTH_1;
            stateStartTime = millis();
            broadcastStatus("authenticating");
        }
    }

    void triggerClose() {
        if (currentState != STATE_LOCKED && currentState != STATE_CLOSING) {
            currentState = STATE_CLOSING;
            stateStartTime = millis();
            broadcastStatus("locking");
        }
    }
};

MyGate *homeKitGate;

// =============================================================================
//                             WEB INTERFACE
// =============================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Alexandria Gate</title>
    <style>
        :root { --bg: #000000; --card: rgba(28, 28, 30, 0.85); --accent: #0A84FF; --text: #FFFFFF; }
        body { margin: 0; font-family: -apple-system, sans-serif; background: var(--bg); color: var(--text); height: 100vh; display: flex; align-items: center; justify-content: center; overflow: hidden; 
               background-image: radial-gradient(circle at 50% 0%, rgba(10, 132, 255, 0.2) 0%, transparent 50%); }
        .glass-card { background: var(--card); backdrop-filter: blur(20px); -webkit-backdrop-filter: blur(20px); border-radius: 30px; padding: 40px; width: 85%; max-width: 380px; text-align: center; border: 1px solid rgba(255,255,255,0.1); }
        .status-ring { width: 120px; height: 120px; margin: 0 auto 20px; border-radius: 50%; border: 5px solid rgba(255,255,255,0.1); display: flex; align-items: center; justify-content: center; font-size: 50px; }
        .status-ring.auth { border-color: #FFD60A; }
        .status-ring.open { border-color: #30D158; box-shadow: 0 0 30px rgba(48, 209, 88, 0.4); }
        input { background: rgba(255,255,255,0.1); border: 1px solid #444; padding: 15px; border-radius: 12px; color: white; font-size: 20px; text-align: center; width: 80%; margin-bottom: 20px; outline: none; letter-spacing: 5px; }
        input:focus { border-color: var(--accent); }
        .btn { width: 100%; background: var(--accent); border: none; border-radius: 16px; padding: 20px; color: white; font-size: 18px; font-weight: 700; cursor: pointer; }
        .btn:active { transform: scale(0.96); }
        .btn-close { background: #FF453A; margin-top: 15px; }
        .led-strip { display: flex; justify-content: center; gap: 15px; margin-bottom: 25px; }
        .led { width: 10px; height: 10px; border-radius: 50%; background: #333; }
        .led.on-w { background: #FFF; box-shadow: 0 0 10px #FFF; }
        .led.on-y { background: #FFD60A; box-shadow: 0 0 10px #FFD60A; }
        .led.on-g { background: #30D158; box-shadow: 0 0 10px #30D158; }
    </style>
</head>
<body>
    <div class="glass-card">
        <h2 id="status-text" style="font-weight: 500; margin-bottom: 10px;">Secure Access</h2>
        <div class="led-strip">
            <div id="led1" class="led"></div>
            <div id="led2" class="led"></div>
            <div id="led3" class="led"></div>
        </div>
        <div id="ring" class="status-ring">🔒</div>
        <input type="password" id="pinInput" placeholder="PIN" maxlength="4" inputmode="numeric">
        <button class="btn" onclick="trigger('open')">UNLOCK</button>
        <button class="btn btn-close" onclick="trigger('close')">LOCK</button>
    </div>
    <script>
        const socket = new WebSocket('ws://' + window.location.hostname + ':81');
        socket.onmessage = (event) => {
            const data = JSON.parse(event.data);
            updateUI(data.status);
        };
        function updateUI(status) {
            const ring = document.getElementById('ring');
            const txt = document.getElementById('status-text');
            const l1 = document.getElementById('led1');
            const l2 = document.getElementById('led2');
            const l3 = document.getElementById('led3');
            l1.className = 'led'; l2.className = 'led'; l3.className = 'led';
            if (status === 'authenticating') {
                txt.innerText = "Verifying...";
                ring.className = "status-ring auth";
                l1.className = "led on-w";
            } else if (status === 'processing') {
                l1.className = "led on-w";
                l2.className = "led on-y";
            } else if (status === 'open') {
                txt.innerText = "UNLOCKED";
                ring.innerText = "🔓";
                ring.className = "status-ring open";
                l1.className = "led on-w"; l2.className = "led on-y"; l3.className = "led on-g";
            } else {
                txt.innerText = "Secure Access";
                ring.innerText = "🔒";
                ring.className = "status-ring";
            }
        }
        function trigger(action) {
            const pin = document.getElementById('pinInput').value;
            fetch('/' + action + '?token=' + pin)
                .then(response => {
                    if(response.status === 401) {
                        document.getElementById('status-text').innerText = "WRONG PIN";
                        document.getElementById('status-text').style.color = "#FF453A";
                        setTimeout(() => { document.getElementById('status-text').style.color = "white"; }, 2000);
                    }
                });
        }
    </script>
</body>
</html>
)rawliteral";

// =============================================================================
//                             WEB HANDLERS
// =============================================================================

bool checkAuth() {
    String t = server.arg("token");
    if (t == API_TOKEN || t == WEB_PIN) return true;
    return false;
}

void handleRoot() { 
    Serial.println("Web Client Connected: Index");
    server.send(200, "text/html", INDEX_HTML); 
}

void handleOpen() {
    Serial.println("Web Request: OPEN");
    if (checkAuth()) {
        homeKitGate->triggerOpen();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(401, "text/plain", "Unauthorized");
    }
}

void handleClose() {
    Serial.println("Web Request: CLOSE");
    if (checkAuth()) {
        homeKitGate->triggerClose();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    }
}

// =============================================================================
//                             CORE LOGIC
// =============================================================================

void updateHardware() {
    unsigned long elapsed = millis() - stateStartTime;
    switch (currentState) {
        case STATE_LOCKED: break;
        case STATE_AUTH_1: 
            digitalWrite(PIN_LED_W, HIGH);
            if (elapsed > 500) {
                currentState = STATE_AUTH_2;
                broadcastStatus("processing");
            }
            break;
        case STATE_AUTH_2:
            digitalWrite(PIN_LED_Y, HIGH);
            if (elapsed > 1000) currentState = STATE_OPENING;
            break;
        case STATE_OPENING:
            digitalWrite(PIN_LED_G, HIGH);
            gateServo.write(SERVO_OPEN_POS);
            tone(PIN_BUZZER, 1500, 200);
            currentState = STATE_OPEN;
            stateStartTime = millis();
            broadcastStatus("open");
            break;
        case STATE_OPEN:
            // AUTO-CLOSE REMOVED! It stays here until you call close()
            break;
        case STATE_CLOSING:
            digitalWrite(PIN_LED_W, LOW);
            digitalWrite(PIN_LED_Y, LOW);
            digitalWrite(PIN_LED_G, LOW);
            gateServo.write(SERVO_LOCKED_POS);
            tone(PIN_BUZZER, 800, 300);
            currentState = STATE_LOCKED;
            broadcastStatus("locked");
            break;
    }
}

// =============================================================================
//                             SETUP & LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    
    pinMode(PIN_LED_W, OUTPUT);
    pinMode(PIN_LED_Y, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    
    // === SERVO CALIBRATION FIX ===
    gateServo.setPeriodHertz(50); 
    gateServo.attach(PIN_SERVO, 500, 2400); // Standard Pulse Widths
    gateServo.write(SERVO_LOCKED_POS);

    homeSpan.setControlPin(0); 
    homeSpan.setWifiCredentials(ssid, password);
    homeSpan.setPairingCode("11122333"); 
    
    homeSpan.begin(Category::GarageDoorOpeners, "Alexandria Lock V3");
    
    new SpanAccessory();
        new Service::AccessoryInformation();
            new Characteristic::Identify();
            new Characteristic::Manufacturer("Manus AI");
            new Characteristic::SerialNumber("AG-PRO-FINAL");
            new Characteristic::Model("SecureGate");
        homeKitGate = new MyGate();
}

void loop() {
    homeSpan.poll();

    if (WiFi.status() == WL_CONNECTED) {
        if (!networkServicesStarted) {
            server.on("/", handleRoot);
            server.on("/open", handleOpen);
            server.on("/close", handleClose);
            server.begin();
            webSocket.begin();
            networkServicesStarted = true;
            Serial.println(">>> Dashboard Started on Port 8080 <<<");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
        }
        server.handleClient();
        webSocket.loop();
    }
    
    updateHardware();
}