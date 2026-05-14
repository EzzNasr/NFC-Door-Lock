/**
 * @file Alexandria_Gate_Ultimate.ino
 * @author Professionalized by Manus AI
 * @brief Ultimate ESP32 Gate Control: HomeKit (HomeSpan) + WebSockets + Web UI.
 * 
 * REQUIRED LIBRARIES:
 * 1. HomeSpan (by Gregg Greene)
 * 2. ESP32Servo
 * 3. WebSockets (by Markus Sattler)
 */

#include "HomeSpan.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// =============================================================================
//                               CONFIGURATION
// =============================================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_TOKEN     = "EZZ_SECURE_TOKEN";

// --- Pin Assignments ---
const int PIN_SERVO  = 26;
const int PIN_LED_R  = 25; 
const int PIN_LED_Y  = 33; 
const int PIN_LED_G  = 32; 
const int PIN_BUZZER = 27;

// --- Servo Settings ---
const int SERVO_LOCKED_POS = 0;
const int SERVO_OPEN_POS   = 90;

// =============================================================================
//                             GLOBAL OBJECTS
// =============================================================================
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Servo gateServo;

// =============================================================================
//                             SYSTEM STATE
// =============================================================================
enum GateState { STATE_LOCKED, STATE_OPENING, STATE_OPEN, STATE_CLOSING };
GateState currentState = STATE_LOCKED;
unsigned long stateStartTime = 0;

void broadcastStatus(String status) {
    String json = "{\"status\":\"" + status + "\"}";
    webSocket.broadcastTXT(json);
}

// =============================================================================
//                             HOMESPAN ACCESSORY
// =============================================================================
struct MyGate : Service::GarageDoorOpener {
    
    SpanCharacteristic *current;
    SpanCharacteristic *target;
    
    MyGate() : Service::GarageDoorOpener() {
        current = new Characteristic::CurrentDoorState(1); // Start Closed
        target = new Characteristic::TargetDoorState(1);  // Start Closed
        Serial.println("HomeKit Gate Accessory Initialized");
    }

    boolean update() override {
        if (target->getNewVal() == 0) { // Target: Open
            Serial.println("HomeKit: Opening Gate...");
            triggerOpen();
        } else { // Target: Closed
            Serial.println("HomeKit: Closing Gate...");
            triggerClose();
        }
        return true;
    }

    void loop() override {
        // Update HomeKit state based on system state
        if (currentState == STATE_OPEN && current->getVal() != 0) {
            current->setVal(0); // Set HomeKit to Open
        } else if (currentState == STATE_LOCKED && current->getVal() != 1) {
            current->setVal(1); // Set HomeKit to Closed
            target->setVal(1);
        }
    }

    void triggerOpen() {
        if (currentState == STATE_LOCKED) {
            currentState = STATE_OPENING;
            stateStartTime = millis();
            broadcastStatus("opening");
        }
    }

    void triggerClose() {
        currentState = STATE_CLOSING;
        stateStartTime = millis();
        broadcastStatus("locking");
    }
};

MyGate *homeKitGate;

// =============================================================================
//                               WEB INTERFACE
// =============================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Alexandria Gate Pro</title>
    <style>
        :root { --bg: #050505; --card: rgba(20, 20, 25, 0.8); --accent: #007AFF; --success: #34C759; --danger: #FF3B30; --text: #FFFFFF; }
        body { margin: 0; font-family: -apple-system, sans-serif; background: var(--bg); color: var(--text); height: 100vh; display: flex; align-items: center; justify-content: center; }
        .glass-card { background: var(--card); backdrop-filter: blur(20px); border-radius: 32px; padding: 40px; width: 90%; max-width: 380px; text-align: center; border: 1px solid rgba(255,255,255,0.1); }
        .status-ring { width: 120px; height: 120px; margin: 0 auto 30px; border-radius: 50%; border: 4px solid rgba(255,255,255,0.1); display: flex; align-items: center; justify-content: center; font-size: 40px; transition: 0.5s; }
        .status-ring.active { border-color: var(--accent); box-shadow: 0 0 20px rgba(0,122,255,0.3); }
        .status-ring.success { border-color: var(--success); box-shadow: 0 0 20px rgba(52,199,89,0.3); }
        .btn { width: 100%; background: var(--accent); border: none; border-radius: 18px; padding: 20px; color: white; font-size: 17px; font-weight: 600; cursor: pointer; margin-top: 20px; }
        .btn:active { transform: scale(0.96); }
        .footer { margin-top: 30px; font-size: 11px; opacity: 0.5; }
    </style>
</head>
<body>
    <div class="glass-card">
        <h1 id="status-text">System Secure</h1>
        <div id="ring" class="status-ring">🔒</div>
        <button class="btn" onclick="send('open')">Unlock Gate</button>
        <div class="footer">HOMEKIT + WEBSOCKETS ENABLED</div>
    </div>
    <script>
        const socket = new WebSocket('ws://' + window.location.hostname + ':81');
        socket.onmessage = function(event) {
            const data = JSON.parse(event.data);
            updateUI(data.status);
        };

        function updateUI(status) {
            const ring = document.getElementById('ring');
            const text = document.getElementById('status-text');
            if (status === 'opening') {
                text.innerText = "Opening...";
                ring.innerText = "⏳";
                ring.className = "status-ring active";
            } else if (status === 'open') {
                text.innerText = "Access Granted";
                ring.innerText = "🔓";
                ring.className = "status-ring success";
            } else {
                text.innerText = "System Secure";
                ring.innerText = "🔒";
                ring.className = "status-ring";
            }
        }

        function send(action) {
            fetch('/' + action + '?token=EZZ_SECURE_TOKEN');
        }
    </script>
</body>
</html>
)rawliteral";

// =============================================================================
//                               HANDLERS
// =============================================================================

void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

void handleOpen() {
    if (server.arg("token") == API_TOKEN) {
        homeKitGate->triggerOpen();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(401, "text/plain", "Unauthorized");
    }
}

void handleClose() {
    if (server.arg("token") == API_TOKEN) {
        homeKitGate->triggerClose();
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    }
}

// =============================================================================
//                               CORE LOGIC
// =============================================================================

void updateHardware() {
    unsigned long elapsed = millis() - stateStartTime;
    switch (currentState) {
        case STATE_OPENING:
            digitalWrite(PIN_LED_R, HIGH);
            if (elapsed > 1000) {
                digitalWrite(PIN_LED_Y, HIGH);
                digitalWrite(PIN_LED_G, HIGH);
                gateServo.write(SERVO_OPEN_POS);
                tone(PIN_BUZZER, 1500, 200);
                currentState = STATE_OPEN;
                stateStartTime = millis();
                broadcastStatus("open");
            }
            break;
        case STATE_OPEN:
            if (elapsed > 5000) {
                currentState = STATE_CLOSING;
                stateStartTime = millis();
                broadcastStatus("locking");
            }
            break;
        case STATE_CLOSING:
            digitalWrite(PIN_LED_R, LOW);
            digitalWrite(PIN_LED_Y, LOW);
            digitalWrite(PIN_LED_G, LOW);
            gateServo.write(SERVO_LOCKED_POS);
            tone(PIN_BUZZER, 800, 300);
            currentState = STATE_LOCKED;
            broadcastStatus("locked");
            break;
        default: break;
    }
}

// =============================================================================
//                               SETUP & LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_Y, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    
    gateServo.attach(PIN_SERVO);
    gateServo.write(SERVO_LOCKED_POS);

    // HomeSpan Setup
    homeSpan.setControlPin(0); // Use Boot button for HomeSpan config if needed
    homeSpan.begin(Category::GarageDoorOpeners, "Alexandria Gate");
    
    new SpanAccessory();
        new Service::AccessoryInformation();
            new Characteristic::Identify();
            new Characteristic::Manufacturer("Manus AI");
            new Characteristic::SerialNumber("AG-001");
            new Characteristic::Model("Pro V2");
        homeKitGate = new MyGate();

    // WiFi & Servers
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    server.on("/", handleRoot);
    server.on("/open", handleOpen);
    server.on("/close", handleClose);
    server.begin();
    
    webSocket.begin();
    Serial.println("System Ready!");
}

void loop() {
    homeSpan.poll();
    server.handleClient();
    webSocket.loop();
    updateHardware();
}
