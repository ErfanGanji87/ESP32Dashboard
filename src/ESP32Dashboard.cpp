#include "ESP32Dashboard.h"

ESP32Dashboard::ESP32Dashboard() {
    server = nullptr;
    webSocket = nullptr;
    dashboardTitle = "ESP32 Dashboard";
    dashboardSubtitle = "Real-time monitoring system";
    darkMode = false;
    lastUpdate = 0;
    updateInterval = 1000;
    serialMonitoring = true;
    serialBaudRate = 115200;
}

ESP32Dashboard::~ESP32Dashboard() {
    if (server) delete server;
    if (webSocket) delete webSocket;
}

void ESP32Dashboard::enableSerialMonitoring(bool enable) {
    serialMonitoring = enable;
    if (enable) {
        Serial.begin(serialBaudRate);
        logToSerial("Serial monitoring enabled", "SYSTEM");
    }
}

void ESP32Dashboard::setSerialBaudRate(unsigned long baudRate) {
    serialBaudRate = baudRate;
    Serial.begin(baudRate);
    logToSerial("Serial baud rate set to " + String(baudRate), "SYSTEM");
}

void ESP32Dashboard::setWiFiCredentials(const char* ssid, const char* password) {
    this->ssid = ssid;
    this->password = password;
    logToSerial("WiFi credentials configured", "WIFI");
    logToSerial("SSID: " + String(ssid), "WIFI");
}

void ESP32Dashboard::logToSerial(String message, String category) {
    if (!serialMonitoring) return;

    String timestamp = String(millis());
    Serial.println("[" + timestamp + "ms] [" + category + "] " + message);
}

void ESP32Dashboard::printSeparator() {
    if (!serialMonitoring) return;
    Serial.println("================================================");
}

void ESP32Dashboard::printSystemStatus() {
    if (!serialMonitoring) return;

    printSeparator();
    logToSerial("ESP32 DASHBOARD SYSTEM STATUS", "STATUS");
    printSeparator();

    logToSerial("WiFi SSID: " + ssid, "WIFI");
    logToSerial("WiFi Status: " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"), "WIFI");
    if (WiFi.status() == WL_CONNECTED) {
        logToSerial("IP Address: " + WiFi.localIP().toString(), "WIFI");
        logToSerial("Signal Strength: " + String(WiFi.RSSI()) + " dBm", "WIFI");
    }

    logToSerial("Web Server: RUNNING on port 80", "SERVER");
    logToSerial("WebSocket Server: RUNNING on port 81", "SERVER");
    logToSerial("Connected Clients: " + String(webSocket->connectedClients()), "SERVER");

    logToSerial("Dashboard Title: " + dashboardTitle, "DASHBOARD");
    logToSerial("Total Cards: " + String(cards.size()), "DASHBOARD");
    logToSerial("Total Controls: " + String(controls.size()), "DASHBOARD");
    logToSerial("Update Interval: " + String(updateInterval) + "ms", "DASHBOARD");

    printSeparator();
}

void ESP32Dashboard::printAllStates() {
    if (!serialMonitoring) return;

    printSeparator();
    logToSerial("CURRENT CONTROL STATES", "STATES");
    printSeparator();

    for (auto& control : controls) {
        String stateInfo = "ID: " + control.id + " | Title: " + control.title;

        if (control.type == CONTROL_SWITCH || control.type == CONTROL_POWER_BUTTON) {
            stateInfo += " | State: " + String(control.state ? "ON" : "OFF");
        }
        else if (control.type == CONTROL_SLIDER) {
            stateInfo += " | Value: " + String(control.value);
        }
        else if (control.type == CONTROL_BUTTON) {
            stateInfo += " | Type: BUTTON";
        }

        logToSerial(stateInfo, "STATE");
    }

    printSeparator();
}

void ESP32Dashboard::printWiFiStatus() {
    if (!serialMonitoring) return;

    printSeparator();
    logToSerial("WIFI CONNECTION STATUS", "WIFI");
    printSeparator();

    logToSerial("SSID: " + ssid, "WIFI");
    logToSerial("Status: " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED ✅" : "DISCONNECTED ❌"), "WIFI");

    if (WiFi.status() == WL_CONNECTED) {
        logToSerial("IP Address: " + WiFi.localIP().toString(), "WIFI");
        logToSerial("Gateway: " + WiFi.gatewayIP().toString(), "WIFI");
        logToSerial("Subnet: " + WiFi.subnetMask().toString(), "WIFI");
        logToSerial("DNS: " + WiFi.dnsIP().toString(), "WIFI");
        logToSerial("Signal Strength: " + String(WiFi.RSSI()) + " dBm", "WIFI");
        logToSerial("MAC Address: " + WiFi.macAddress(), "WIFI");
    }

    printSeparator();
}

void ESP32Dashboard::printWebServerInfo() {
    if (!serialMonitoring) return;

    printSeparator();
    logToSerial("WEB SERVER INFORMATION", "SERVER");
    printSeparator();

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        logToSerial("🌐 Web Dashboard URL: http://" + ip, "SERVER");
        logToSerial("📱 Mobile Access: http://" + ip, "SERVER");
        logToSerial("🔗 API Endpoint: http://" + ip + "/api/data", "SERVER");
        logToSerial("⚡ WebSocket: ws://" + ip + ":81", "SERVER");
    }
    else {
        logToSerial("❌ WiFi not connected - Server not accessible", "SERVER");
    }

    logToSerial("Server Status: RUNNING ✅", "SERVER");
    logToSerial("Connected Clients: " + String(webSocket->connectedClients()), "SERVER");

    printSeparator();
}

bool ESP32Dashboard::begin(const char* ssid, const char* password, int port, int wsPort) {
    this->ssid = ssid;
    this->password = password;

    if (serialMonitoring) {
        Serial.begin(serialBaudRate);
        delay(1000);

        printSeparator();
        logToSerial("ESP32 DASHBOARD STARTING...", "SYSTEM");
        printSeparator();
    }

    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        if (serialMonitoring) Serial.print(".");
        attempts++;

        if (attempts % 10 == 0) {
            logToSerial("Connection attempt " + String(attempts) + "/30", "WIFI");
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        logToSerial("❌ FAILED TO CONNECT TO WIFI!", "ERROR");
        return false;
    }

    logToSerial("✅ WiFi connected successfully!", "WIFI");
    printWiFiStatus();

    server = new WebServer(port);
    webSocket = new WebSocketsServer(wsPort);

    server->on("/", [this]() { handleRoot(); });
    server->on("/api/data", [this]() { handleApiData(); });
    server->on("/api/control", HTTP_POST, [this]() { handleApiControl(); });
    server->onNotFound([this]() { handleNotFound(); });

    server->begin();
    webSocket->begin();
    webSocket->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        webSocketEvent(num, type, payload, length);
        });

    logToSerial("✅ Web server started successfully!", "SERVER");
    printWebServerInfo();
    printSystemStatus();

    return true;
}

void ESP32Dashboard::setTitle(const char* title, const char* subtitle) {
    dashboardTitle = title;
    if (strlen(subtitle) > 0) {
        dashboardSubtitle = subtitle;
    }
}

void ESP32Dashboard::setUpdateInterval(unsigned long interval) {
    updateInterval = interval;
}

void ESP32Dashboard::loop() {
    server->handleClient();
    webSocket->loop();

    if (millis() - lastUpdate >= updateInterval) {
        // Update chart data for all chart cards
        for (auto& card : cards) {
            if (card.type == CARD_CHART && card.valueCallback) {
                float value = card.valueCallback().toFloat();
                addChartDataPoint(card.id.c_str(), value);
            }
        }

        sendDataToClients();
        lastUpdate = millis();
    }
}

void ESP32Dashboard::addChartDataPoint(const char* cardId, float value) {
    for (auto& card : cards) {
        if (card.id == cardId && card.type == CARD_CHART) {
            ChartDataPoint point;
            point.timestamp = millis();
            point.value = value;

            card.chartData.push_back(point);

            // Keep only maxDataPoints
            if (card.chartData.size() > card.maxDataPoints) {
                card.chartData.erase(card.chartData.begin());
            }
            break;
        }
    }
}

String ESP32Dashboard::addTemperatureCard(const char* title, std::function<float()> callback) {
    DashboardCard card;
    card.id = "temp_" + String(cards.size());
    card.title = title;
    card.description = "Temperature";
    card.color = "orange";
    card.icon = "🌡️";
    card.type = CARD_TEMPERATURE;
    card.valueCallback = [callback]() -> String {
        float temp = callback();
        return String(temp, 1) + "°C";
        };
    card.statusCallback = [callback]() -> String {
        float temp = callback();
        if (temp > 30) return "🔥 High temperature";
        if (temp < 15) return "❄️ Low temperature";
        return "✅ Normal range";
        };

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addHumidityCard(const char* title, std::function<float()> callback) {
    DashboardCard card;
    card.id = "hum_" + String(cards.size());
    card.title = title;
    card.description = "Humidity";
    card.color = "blue";
    card.icon = "💧";
    card.type = CARD_HUMIDITY;
    card.valueCallback = [callback]() -> String {
        float hum = callback();
        return String(hum, 1) + "%";
        };
    card.statusCallback = [callback]() -> String {
        float hum = callback();
        if (hum > 70) return "💧 High humidity";
        if (hum < 30) return "🏜️ Low humidity";
        return "✅ Optimal";
        };

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addMotorRPMCard(const char* title, std::function<int()> callback) {
    DashboardCard card;
    card.id = "rpm_" + String(cards.size());
    card.title = title;
    card.description = "Motor RPM";
    card.color = "green";
    card.icon = "⚙️";
    card.type = CARD_MOTOR_RPM;
    card.valueCallback = [callback]() -> String {
        return String(callback());
        };
    card.statusCallback = [callback]() -> String {
        int rpm = callback();
        if (rpm > 1400) return "⚡ High speed";
        if (rpm < 800) return "🐌 Low speed";
        return "✅ Normal speed";
        };

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addStatusCard(const char* title, const char* description, std::function<String()> valueCallback, std::function<String()> statusCallback, const char* color) {
    DashboardCard card;
    card.id = "status_" + String(cards.size());
    card.title = title;
    card.description = description;
    card.color = color;
    card.icon = "ℹ️";
    card.type = CARD_STATUS;
    card.valueCallback = valueCallback;
    card.statusCallback = statusCallback;

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addPercentageCard(const char* title, const char* description, std::function<int()> callback, const char* color) {
    DashboardCard card;
    card.id = "pct_" + String(cards.size());
    card.title = title;
    card.description = description;
    card.color = color;
    card.icon = "📊";
    card.type = CARD_PERCENTAGE;
    card.valueCallback = [callback]() -> String {
        return String(callback()) + "%";
        };
    card.statusCallback = [callback]() -> String {
        int pct = callback();
        if (pct > 80) return "🔋 Excellent";
        if (pct > 50) return "✅ Good";
        if (pct > 20) return "⚠️ Low";
        return "🔴 Critical";
        };

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addCustomCard(const char* title, const char* description, std::function<String()> valueCallback, std::function<String()> statusCallback, const char* color, const char* icon) {
    DashboardCard card;
    card.id = "custom_" + String(cards.size());
    card.title = title;
    card.description = description;
    card.color = color;
    card.icon = strlen(icon) > 0 ? icon : "⭐";
    card.type = CARD_CUSTOM;
    card.valueCallback = valueCallback;
    card.statusCallback = statusCallback;

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addChartCard(const char* title, const char* description, std::function<float()> callback, const char* color, int maxPoints) {
    DashboardCard card;
    card.id = "chart_" + String(cards.size());
    card.title = title;
    card.description = description;
    card.color = color;
    card.icon = "📈";
    card.type = CARD_CHART;
    card.maxDataPoints = maxPoints;
    card.valueCallback = [callback]() -> String {
        return String(callback(), 2);
        };
    card.statusCallback = []() -> String {
        return "Real-time data";
        };

    cards.push_back(card);
    return card.id;
}

String ESP32Dashboard::addSwitch(const char* title, const char* description, std::function<void(bool)> callback, const char* color) {
    DashboardControl control;
    control.id = "switch_" + String(controls.size());
    control.title = title;
    control.description = description;
    control.type = CONTROL_SWITCH;
    control.state = false;
    control.color = color;
    control.switchCallback = callback;

    controls.push_back(control);
    return control.id;
}

String ESP32Dashboard::addButton(const char* title, const char* description, std::function<void()> callback, const char* color) {
    DashboardControl control;
    control.id = "btn_" + String(controls.size());
    control.title = title;
    control.description = description;
    control.type = CONTROL_BUTTON;
    control.state = false;
    control.color = color;
    control.buttonCallback = callback;

    controls.push_back(control);
    return control.id;
}

String ESP32Dashboard::addPowerButton(const char* title, const char* description, std::function<void(bool)> callback) {
    DashboardControl control;
    control.id = "power_" + String(controls.size());
    control.title = title;
    control.description = description;
    control.type = CONTROL_POWER_BUTTON;
    control.state = false;
    control.color = "green";
    control.switchCallback = callback;

    controls.push_back(control);
    return control.id;
}

String ESP32Dashboard::addSlider(const char* title, const char* description, std::function<void(int)> callback, int min, int max, const char* color) {
    DashboardControl control;
    control.id = "slider_" + String(controls.size());
    control.title = title;
    control.description = description;
    control.type = CONTROL_SLIDER;
    control.value = min;
    control.minValue = min;
    control.maxValue = max;
    control.color = color;
    control.sliderCallback = callback;

    controls.push_back(control);
    return control.id;
}

bool ESP32Dashboard::getSwitchState(const char* id) {
    for (auto& control : controls) {
        if (control.id == id && (control.type == CONTROL_SWITCH || control.type == CONTROL_POWER_BUTTON)) {
            return control.state;
        }
    }
    return false;
}

void ESP32Dashboard::setSwitchState(const char* id, bool state) {
    for (auto& control : controls) {
        if (control.id == id && (control.type == CONTROL_SWITCH || control.type == CONTROL_POWER_BUTTON)) {
            control.state = state;
            if (control.switchCallback) {
                control.switchCallback(state);
            }
            break;
        }
    }
}

int ESP32Dashboard::getSliderValue(const char* id) {
    for (auto& control : controls) {
        if (control.id == id && control.type == CONTROL_SLIDER) {
            return control.value;
        }
    }
    return 0;
}

void ESP32Dashboard::setSliderValue(const char* id, int value) {
    for (auto& control : controls) {
        if (control.id == id && control.type == CONTROL_SLIDER) {
            control.value = value;
            if (control.sliderCallback) {
                control.sliderCallback(value);
            }
            break;
        }
    }
}

void ESP32Dashboard::updateCard(const char* id, const char* value, const char* status) {
    for (auto& card : cards) {
        if (card.id == id) {
            card.value = value;
            if (strlen(status) > 0) {
                card.status = status;
            }
            break;
        }
    }
}

String ESP32Dashboard::getLocalIP() {
    return WiFi.localIP().toString();
}

bool ESP32Dashboard::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int ESP32Dashboard::getConnectedClients() {
    return webSocket->connectedClients();
}

void ESP32Dashboard::handleRoot() {
    String html = generateHTML();
    server->send(200, "text/html", html);
}

void ESP32Dashboard::handleApiData() {
    DynamicJsonDocument doc(4096);

    JsonArray cardArray = doc.createNestedArray("cards");
    for (auto& card : cards) {
        JsonObject cardObj = cardArray.createNestedObject();
        cardObj["id"] = card.id;
        cardObj["title"] = card.title;
        cardObj["description"] = card.description;
        cardObj["value"] = card.valueCallback ? card.valueCallback() : card.value;
        cardObj["status"] = card.statusCallback ? card.statusCallback() : card.status;
        cardObj["color"] = card.color;
        cardObj["icon"] = card.icon;
        cardObj["type"] = card.type;

        if (card.type == CARD_CHART) {
            JsonArray chartArray = cardObj.createNestedArray("chartData");
            for (auto& point : card.chartData) {
                JsonObject pointObj = chartArray.createNestedObject();
                pointObj["timestamp"] = point.timestamp;
                pointObj["value"] = point.value;
            }
        }
    }

    JsonArray controlArray = doc.createNestedArray("controls");
    for (auto& control : controls) {
        JsonObject controlObj = controlArray.createNestedObject();
        controlObj["id"] = control.id;
        controlObj["title"] = control.title;
        controlObj["description"] = control.description;
        controlObj["type"] = control.type;
        controlObj["state"] = control.state;
        controlObj["value"] = control.value;
        controlObj["color"] = control.color;
    }

    doc["timestamp"] = millis();
    doc["connectedClients"] = webSocket->connectedClients();

    String jsonString;
    serializeJson(doc, jsonString);
    server->send(200, "application/json", jsonString);
}

void ESP32Dashboard::handleApiControl() {
    if (server->hasArg("plain")) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, server->arg("plain"));

        String controlId = doc["id"];
        String action = doc["action"];

        for (auto& control : controls) {
            if (control.id == controlId) {
                if (action == "toggle" && (control.type == CONTROL_SWITCH || control.type == CONTROL_POWER_BUTTON)) {
                    control.state = !control.state;
                    if (control.switchCallback) {
                        control.switchCallback(control.state);
                    }
                }
                else if (action == "click" && control.type == CONTROL_BUTTON) {
                    if (control.buttonCallback) {
                        control.buttonCallback();
                    }
                }
                else if (action == "slide" && control.type == CONTROL_SLIDER) {
                    control.value = doc["value"];
                    if (control.sliderCallback) {
                        control.sliderCallback(control.value);
                    }
                }
                break;
            }
        }

        server->send(200, "application/json", "{\"status\":\"success\"}");
        sendDataToClients();
    }
    else {
        server->send(400, "application/json", "{\"error\":\"No data received\"}");
    }
}

void ESP32Dashboard::handleNotFound() {
    server->send(404, "text/plain", "File Not Found");
}

void ESP32Dashboard::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
    case WStype_DISCONNECTED:
        logToSerial("Client #" + String(num) + " disconnected", "WEBSOCKET");
        if (onClientDisconnect) onClientDisconnect();
        break;

    case WStype_CONNECTED:
        logToSerial("Client #" + String(num) + " connected from " + webSocket->remoteIP(num).toString(), "WEBSOCKET");
        if (onClientConnect) onClientConnect();
        sendDataToClients();
        break;

    case WStype_TEXT:
        logToSerial("Message from client #" + String(num) + ": " + String((char*)payload), "WEBSOCKET");

        DynamicJsonDocument doc(512);
        deserializeJson(doc, (char*)payload);

        if (doc.containsKey("id") && doc.containsKey("action")) {
            String controlId = doc["id"];
            String action = doc["action"];

            for (auto& control : controls) {
                if (control.id == controlId) {
                    if (action == "toggle" && (control.type == CONTROL_SWITCH || control.type == CONTROL_POWER_BUTTON)) {
                        control.state = !control.state;
                        logToSerial("Control '" + control.title + "' toggled to " + String(control.state ? "ON" : "OFF"), "CONTROL");
                        if (control.switchCallback) {
                            control.switchCallback(control.state);
                        }
                    }
                    else if (action == "click" && control.type == CONTROL_BUTTON) {
                        logToSerial("Button '" + control.title + "' clicked", "CONTROL");
                        if (control.buttonCallback) {
                            control.buttonCallback();
                        }
                    }
                    else if (action == "slide" && control.type == CONTROL_SLIDER) {
                        control.value = doc["value"];
                        logToSerial("Slider '" + control.title + "' set to " + String(control.value), "CONTROL");
                        if (control.sliderCallback) {
                            control.sliderCallback(control.value);
                        }
                    }
                    break;
                }
            }
            sendDataToClients();
        }

        if (onCustomMessage) {
            onCustomMessage(String((char*)payload), String(num));
        }
        break;
    }
}

void ESP32Dashboard::sendDataToClients() {
    DynamicJsonDocument doc(4096);

    JsonArray cardArray = doc.createNestedArray("cards");
    for (auto& card : cards) {
        JsonObject cardObj = cardArray.createNestedObject();
        cardObj["id"] = card.id;
        cardObj["value"] = card.valueCallback ? card.valueCallback() : card.value;
        cardObj["status"] = card.statusCallback ? card.statusCallback() : card.status;
        cardObj["type"] = card.type;

        if (card.type == CARD_CHART) {
            JsonArray chartArray = cardObj.createNestedArray("chartData");
            for (auto& point : card.chartData) {
                JsonObject pointObj = chartArray.createNestedObject();
                pointObj["timestamp"] = point.timestamp;
                pointObj["value"] = point.value;
            }
        }
    }

    JsonArray controlArray = doc.createNestedArray("controls");
    for (auto& control : controls) {
        JsonObject controlObj = controlArray.createNestedObject();
        controlObj["id"] = control.id;
        controlObj["state"] = control.state;
        controlObj["value"] = control.value;
    }

    doc["timestamp"] = millis();
    doc["connectedClients"] = webSocket->connectedClients();

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket->broadcastTXT(jsonString);
}

String ESP32Dashboard::generateHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)rawliteral";

    html += dashboardTitle;
    html += R"rawliteral(</title>
    <style>)rawliteral";
    html += generateCSS();
    html += R"rawliteral(</style>
</head>
<body>
    <div class="dashboard-container">
        <header class="dashboard-header">
            <div class="header-content">
                <div class="logo-section">
                    <div class="logo-icon">📊</div>
                    <div class="logo-text">
                        <h1>)rawliteral";
    html += dashboardTitle;
    html += R"rawliteral(</h1>
                        <p>)rawliteral";
    html += dashboardSubtitle;
    html += R"rawliteral(</p>
                    </div>
                </div>
                <div class="header-controls">
                    <div id="connectionStatus" class="status-indicator online">
                        <div class="status-dot"></div>
                        <span>Online</span>
                    </div>
                    <div id="clientCounter" class="client-counter">
                        <span>👥 <span id="clientCount">0</span></span>
                    </div>
                    <button class="theme-toggle" onclick="toggleTheme()" title="Toggle Theme">
                        <span id="themeIcon">🌙</span>
                    </button>
                </div>
            </div>
        </header>

        <main class="dashboard-main">
            <div class="cards-grid" id="cardsContainer">)rawliteral";
    html += generateCards();
    html += R"rawliteral(</div>
            
            <div class="controls-section">
                <h2 class="section-title">🎛️ Controls</h2>
                <div class="controls-grid" id="controlsContainer">)rawliteral";
    html += generateControls();
    html += R"rawliteral(</div>
            </div>
        </main>
    </div>

    <script>)rawliteral";
    html += generateJavaScript();
    html += R"rawliteral(</script>
</body>
</html>)rawliteral";

    return html;
}

String ESP32Dashboard::generateCards() {
    String cardsHTML = "";

    for (auto& card : cards) {
        if (card.type == CARD_CHART) {
            cardsHTML += R"rawliteral(
        <div class="dashboard-card chart-card">
            <div class="card-header">
                <div class="card-icon">)rawliteral";
            cardsHTML += card.icon;
            cardsHTML += R"rawliteral(</div>
                <div class="card-info">
                    <h3 class="card-title">)rawliteral";
            cardsHTML += card.title;
            cardsHTML += R"rawliteral(</h3>
                    <p class="card-description">)rawliteral";
            cardsHTML += card.description;
            cardsHTML += R"rawliteral(</p>
                </div>
            </div>
            <div class="chart-container">
                <canvas id=")rawliteral";
            cardsHTML += card.id;
            cardsHTML += R"rawliteral(_chart" class="chart-canvas"></canvas>
            </div>
            <div class="card-footer">
                <span class="card-value text-)rawliteral";
            cardsHTML += card.color;
            cardsHTML += R"rawliteral(" id=")rawliteral";
            cardsHTML += card.id;
            cardsHTML += R"rawliteral(_value">)rawliteral";
            cardsHTML += card.valueCallback ? card.valueCallback() : card.value;
            cardsHTML += R"rawliteral(</span>
                <span class="card-status" id=")rawliteral";
            cardsHTML += card.id;
            cardsHTML += R"rawliteral(_status">)rawliteral";
            cardsHTML += card.statusCallback ? card.statusCallback() : card.status;
            cardsHTML += R"rawliteral(</span>
            </div>
        </div>)rawliteral";
        }
        else {
            cardsHTML += R"rawliteral(
        <div class="dashboard-card">
            <div class="card-header">
                <div class="card-icon">)rawliteral";
            cardsHTML += card.icon;
            cardsHTML += R"rawliteral(</div>
                <div class="card-info">
                    <h3 class="card-title">)rawliteral";
            cardsHTML += card.title;
            cardsHTML += R"rawliteral(</h3>
                    <p class="card-description">)rawliteral";
            cardsHTML += card.description;
            cardsHTML += R"rawliteral(</p>
                </div>
            </div>
            <div class="card-content">
                <div class="card-value text-)rawliteral";
            cardsHTML += card.color;
            cardsHTML += R"rawliteral(" id=")rawliteral";
            cardsHTML += card.id;
            cardsHTML += R"rawliteral(_value">)rawliteral";
            cardsHTML += card.valueCallback ? card.valueCallback() : card.value;
            cardsHTML += R"rawliteral(</div>
                <div class="card-status" id=")rawliteral";
            cardsHTML += card.id;
            cardsHTML += R"rawliteral(_status">)rawliteral";
            cardsHTML += card.statusCallback ? card.statusCallback() : card.status;
            cardsHTML += R"rawliteral(</div>
            </div>
        </div>)rawliteral";
        }
    }

    return cardsHTML;
}

String ESP32Dashboard::generateControls() {
    String controlsHTML = "";

    for (auto& control : controls) {
        if (control.type == CONTROL_POWER_BUTTON) {
            controlsHTML += R"rawliteral(
        <div class="control-card power-control">
            <div class="control-header">
                <h3>)rawliteral";
            controlsHTML += control.title;
            controlsHTML += R"rawliteral(</h3>
                <p>)rawliteral";
            controlsHTML += control.description;
            controlsHTML += R"rawliteral(</p>
            </div>
            <div class="power-button-container">
                <button class="power-button" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(" onclick="toggleControl(')rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(')">
                    <div class="power-icon">⚡</div>
                    <span id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_text">OFF</span>
                </button>
            </div>
            <div class="power-status" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_status">
                <span>System Inactive</span>
            </div>
        </div>)rawliteral";
        }
        else if (control.type == CONTROL_SWITCH) {
            controlsHTML += R"rawliteral(
        <div class="control-card">
            <div class="control-header">
                <div class="control-info">
                    <h3>)rawliteral";
            controlsHTML += control.title;
            controlsHTML += R"rawliteral(</h3>
                    <p>)rawliteral";
            controlsHTML += control.description;
            controlsHTML += R"rawliteral(</p>
                </div>
                <div class="control-indicator" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_indicator"></div>
            </div>
            <div class="switch-container">
                <label class="switch">
                    <input type="checkbox" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_input" onchange="toggleControl(')rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(')">
                    <span class="switch-slider"></span>
                </label>
                <span class="switch-status" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_status">OFF</span>
            </div>
        </div>)rawliteral";
        }
        else if (control.type == CONTROL_BUTTON) {
            controlsHTML += R"rawliteral(
        <div class="control-card">
            <div class="control-header">
                <h3>)rawliteral";
            controlsHTML += control.title;
            controlsHTML += R"rawliteral(</h3>
                <p>)rawliteral";
            controlsHTML += control.description;
            controlsHTML += R"rawliteral(</p>
            </div>
            <button class="action-button bg-)rawliteral";
            controlsHTML += control.color;
            controlsHTML += R"rawliteral(" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(" onclick="clickControl(')rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(')">
                Execute
            </button>
        </div>)rawliteral";
        }
        else if (control.type == CONTROL_SLIDER) {
            controlsHTML += R"rawliteral(
        <div class="control-card">
            <div class="control-header">
                <h3>)rawliteral";
            controlsHTML += control.title;
            controlsHTML += R"rawliteral(</h3>
                <p>)rawliteral";
            controlsHTML += control.description;
            controlsHTML += R"rawliteral(</p>
            </div>
            <div class="slider-container">
                <input type="range" class="slider" id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_input" min=")rawliteral";
            controlsHTML += String(control.minValue);
            controlsHTML += R"rawliteral(" max=")rawliteral";
            controlsHTML += String(control.maxValue);
            controlsHTML += R"rawliteral(" value=")rawliteral";
            controlsHTML += String(control.value);
            controlsHTML += R"rawliteral(" oninput="slideControl(')rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(', this.value)">
                <div class="slider-value">
                    <span id=")rawliteral";
            controlsHTML += control.id;
            controlsHTML += R"rawliteral(_value">)rawliteral";
            controlsHTML += String(control.value);
            controlsHTML += R"rawliteral(</span>
                </div>
            </div>
        </div>)rawliteral";
        }
    }

    return controlsHTML;
}

String ESP32Dashboard::generateCSS() {
    return R"rawliteral(
    * {
        margin: 0;
        padding: 0;
        box-sizing: border-box;
    }

    :root {
        --primary-color: #3b82f6;
        --secondary-color: #64748b;
        --success-color: #22c55e;
        --warning-color: #f59e0b;
        --danger-color: #ef4444;
        --info-color: #06b6d4;
        --purple-color: #a855f7;
        --orange-color: #f97316;
        
        --bg-primary: #ffffff;
        --bg-secondary: #f8fafc;
        --bg-tertiary: #f1f5f9;
        --text-primary: #0f172a;
        --text-secondary: #64748b;
        --border-color: #e2e8f0;
        --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
    }

    body {
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        background: linear-gradient(135deg, var(--bg-secondary) 0%, var(--bg-tertiary) 100%);
        color: var(--text-primary);
        min-height: 100vh;
        transition: all 0.3s ease;
    }

    body.dark {
        --bg-primary: #1e293b;
        --bg-secondary: #0f172a;
        --bg-tertiary: #334155;
        --text-primary: #f8fafc;
        --text-secondary: #94a3b8;
        --border-color: #334155;
    }

    .dashboard-container {
        max-width: 1400px;
        margin: 0 auto;
        padding: 0 1rem;
    }

    .dashboard-header {
        background: var(--bg-primary);
        border-bottom: 1px solid var(--border-color);
        padding: 1rem 0;
        margin-bottom: 2rem;
        box-shadow: var(--shadow);
        position: sticky;
        top: 0;
        z-index: 100;
        backdrop-filter: blur(10px);
    }

    .header-content {
        display: flex;
        justify-content: space-between;
        align-items: center;
        flex-wrap: wrap;
        gap: 1rem;
    }

    .logo-section {
        display: flex;
        align-items: center;
        gap: 1rem;
    }

    .logo-icon {
        width: 3rem;
        height: 3rem;
        background: linear-gradient(135deg, var(--primary-color), var(--info-color));
        border-radius: 0.75rem;
        display: flex;
        align-items: center;
        justify-content: center;
        font-size: 1.5rem;
        color: white;
        box-shadow: var(--shadow);
    }

    .logo-text h1 {
        font-size: 1.5rem;
        font-weight: 700;
        margin-bottom: 0.25rem;
    }

    .logo-text p {
        color: var(--text-secondary);
        font-size: 0.875rem;
    }

    .header-controls {
        display: flex;
        align-items: center;
        gap: 1rem;
    }

    .status-indicator {
        display: flex;
        align-items: center;
        gap: 0.5rem;
        padding: 0.5rem 1rem;
        border-radius: 9999px;
        font-size: 0.875rem;
        font-weight: 500;
        transition: all 0.3s ease;
    }

    .status-indicator.online {
        background: var(--success-color);
        color: white;
    }

    .status-indicator.offline {
        background: var(--danger-color);
        color: white;
    }

    .status-dot {
        width: 0.5rem;
        height: 0.5rem;
        border-radius: 50%;
        background: currentColor;
        animation: pulse 2s infinite;
    }

    .client-counter {
        background: var(--info-color);
        color: white;
        padding: 0.5rem 1rem;
        border-radius: 9999px;
        font-size: 0.875rem;
        font-weight: 500;
    }

    .theme-toggle {
        width: 2.5rem;
        height: 2.5rem;
        border: none;
        background: var(--bg-tertiary);
        border-radius: 50%;
        cursor: pointer;
        font-size: 1.25rem;
        transition: all 0.3s ease;
        display: flex;
        align-items: center;
        justify-content: center;
    }

    .theme-toggle:hover {
        background: var(--border-color);
        transform: scale(1.1);
    }

    .dashboard-main {
        padding-bottom: 2rem;
    }

    .cards-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
        gap: 1.5rem;
        margin-bottom: 3rem;
    }

    .dashboard-card {
        background: var(--bg-primary);
        border: 1px solid var(--border-color);
        border-radius: 1rem;
        padding: 1.5rem;
        box-shadow: var(--shadow);
        transition: all 0.3s ease;
        position: relative;
        overflow: hidden;
    }

    .dashboard-card:hover {
        transform: translateY(-4px);
        box-shadow: var(--shadow-lg);
    }

    .card-header {
        display: flex;
        align-items: center;
        gap: 1rem;
        margin-bottom: 1rem;
    }

    .card-icon {
        font-size: 2rem;
        width: 3rem;
        height: 3rem;
        display: flex;
        align-items: center;
        justify-content: center;
        background: var(--bg-tertiary);
        border-radius: 0.75rem;
    }

    .card-info h3 {
        font-size: 1.125rem;
        font-weight: 600;
        margin-bottom: 0.25rem;
    }

    .card-info p {
        color: var(--text-secondary);
        font-size: 0.875rem;
    }

    .card-content {
        text-align: center;
    }

    .card-value {
        font-size: 2.5rem;
        font-weight: 700;
        margin-bottom: 0.5rem;
        display: block;
    }

    .card-status {
        color: var(--text-secondary);
        font-size: 0.875rem;
    }

    .chart-card {
        grid-column: span 2;
    }

    .chart-container {
        height: 200px;
        margin: 1rem 0;
        position: relative;
    }

    .chart-canvas {
        width: 100%;
        height: 100%;
    }

    .card-footer {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-top: 1rem;
        padding-top: 1rem;
        border-top: 1px solid var(--border-color);
    }

    .section-title {
        font-size: 1.5rem;
        font-weight: 700;
        margin-bottom: 1.5rem;
        color: var(--text-primary);
    }

    .controls-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
        gap: 1.5rem;
    }

    .control-card {
        background: var(--bg-primary);
        border: 1px solid var(--border-color);
        border-radius: 1rem;
        padding: 1.5rem;
        box-shadow: var(--shadow);
        transition: all 0.3s ease;
    }

    .control-card:hover {
        transform: translateY(-2px);
        box-shadow: var(--shadow-lg);
    }

    .control-header {
        display: flex;
        justify-content: space-between;
        align-items: flex-start;
        margin-bottom: 1rem;
    }

    .control-info h3 {
        font-size: 1.125rem;
        font-weight: 600;
        margin-bottom: 0.25rem;
    }

    .control-info p {
        color: var(--text-secondary);
        font-size: 0.875rem;
    }

    .control-indicator {
        width: 0.75rem;
        height: 0.75rem;
        border-radius: 50%;
        background: var(--text-secondary);
        transition: all 0.3s ease;
    }

    .control-indicator.active {
        background: var(--success-color);
        animation: pulse 2s infinite;
    }

    .switch-container {
        display: flex;
        align-items: center;
        justify-content: space-between;
    }

    .switch {
        position: relative;
        display: inline-block;
        width: 3.5rem;
        height: 2rem;
    }

    .switch input {
        opacity: 0;
        width: 0;
        height: 0;
    }

    .switch-slider {
        position: absolute;
        cursor: pointer;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background-color: var(--text-secondary);
        transition: 0.3s;
        border-radius: 2rem;
    }

    .switch-slider:before {
        position: absolute;
        content: "";
        height: 1.5rem;
        width: 1.5rem;
        left: 0.25rem;
        bottom: 0.25rem;
        background-color: white;
        transition: 0.3s;
        border-radius: 50%;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
    }

    .switch input:checked + .switch-slider {
        background-color: var(--success-color);
    }

    .switch input:checked + .switch-slider:before {
        transform: translateX(1.5rem);
    }

    .switch-status {
        font-weight: 500;
        color: var(--text-secondary);
    }

    .power-control {
        text-align: center;
    }

    .power-button-container {
        margin: 1.5rem 0;
    }

    .power-button {
        width: 5rem;
        height: 5rem;
        border-radius: 50%;
        border: 3px solid var(--text-secondary);
        background: linear-gradient(135deg, var(--bg-tertiary), var(--bg-secondary));
        cursor: pointer;
        transition: all 0.3s ease;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        font-weight: 600;
        color: var(--text-primary);
    }

    .power-button:hover {
        transform: scale(1.05);
    }

    .power-button.active {
        border-color: var(--success-color);
        background: linear-gradient(135deg, var(--success-color), #16a34a);
        color: white;
        box-shadow: 0 0 20px rgba(34, 197, 94, 0.3);
    }

    .power-icon {
        font-size: 1.5rem;
        margin-bottom: 0.25rem;
    }

    .power-status {
        color: var(--text-secondary);
        font-size: 0.875rem;
    }

    .power-status.active {
        color: var(--success-color);
    }

    .action-button {
        width: 100%;
        padding: 0.75rem 1.5rem;
        border: none;
        border-radius: 0.5rem;
        font-weight: 600;
        cursor: pointer;
        transition: all 0.3s ease;
        color: white;
    }

    .action-button:hover {
        transform: translateY(-1px);
        box-shadow: var(--shadow);
    }

    .slider-container {
        display: flex;
        flex-direction: column;
        gap: 1rem;
    }

    .slider {
        width: 100%;
        height: 0.5rem;
        border-radius: 0.25rem;
        background: var(--bg-tertiary);
        outline: none;
        -webkit-appearance: none;
        transition: all 0.3s ease;
    }

    .slider::-webkit-slider-thumb {
        appearance: none;
        width: 1.5rem;
        height: 1.5rem;
        border-radius: 50%;
        background: var(--primary-color);
        cursor: pointer;
        box-shadow: var(--shadow);
        transition: all 0.3s ease;
    }

    .slider::-webkit-slider-thumb:hover {
        transform: scale(1.1);
    }

    .slider::-moz-range-thumb {
        width: 1.5rem;
        height: 1.5rem;
        border-radius: 50%;
        background: var(--primary-color);
        cursor: pointer;
        border: none;
        box-shadow: var(--shadow);
    }

    .slider-value {
        text-align: center;
        font-weight: 600;
        font-size: 1.125rem;
        color: var(--primary-color);
    }

    /* Color utilities */
    .text-blue { color: var(--primary-color); }
    .text-green { color: var(--success-color); }
    .text-orange { color: var(--orange-color); }
    .text-red { color: var(--danger-color); }
    .text-purple { color: var(--purple-color); }
    .text-cyan { color: var(--info-color); }
    .text-yellow { color: var(--warning-color); }

    .bg-blue { background: var(--primary-color); }
    .bg-green { background: var(--success-color); }
    .bg-orange { background: var(--orange-color); }
    .bg-red { background: var(--danger-color); }
    .bg-purple { background: var(--purple-color); }
    .bg-cyan { background: var(--info-color); }
    .bg-yellow { background: var(--warning-color); }

    @keyframes pulse {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.5; }
    }

    @media (max-width: 768px) {
        .dashboard-container {
            padding: 0 0.5rem;
        }
        
        .cards-grid {
            grid-template-columns: 1fr;
        }
        
        .chart-card {
            grid-column: span 1;
        }
        
        .controls-grid {
            grid-template-columns: 1fr;
        }
        
        .card-value {
            font-size: 2rem;
        }
        
        .power-button {
            width: 4rem;
            height: 4rem;
        }
    }
  )rawliteral";
}

String ESP32Dashboard::generateJavaScript() {
    return R"rawliteral(
    let ws;
    let isDarkMode = false;
    let reconnectAttempts = 0;
    const maxReconnectAttempts = 5;
    const charts = {};

    document.addEventListener('DOMContentLoaded', function() {
        initWebSocket();
        loadTheme();
    });

    function initWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.hostname}:81`;
        
        console.log('Connecting to WebSocket:', wsUrl);
        ws = new WebSocket(wsUrl);
        
        ws.onopen = function() {
            console.log('✅ WebSocket connected');
            reconnectAttempts = 0;
            updateConnectionStatus(true);
        };
        
        ws.onmessage = function(event) {
            try {
                const data = JSON.parse(event.data);
                updateUI(data);
            } catch (error) {
                console.error('❌ Error parsing WebSocket data:', error);
            }
        };
        
        ws.onclose = function() {
            console.log('❌ WebSocket disconnected');
            updateConnectionStatus(false);
            
            if (reconnectAttempts < maxReconnectAttempts) {
                reconnectAttempts++;
                const delay = Math.min(1000 * Math.pow(2, reconnectAttempts), 30000);
                console.log(`🔄 Reconnecting in ${delay}ms (attempt ${reconnectAttempts}/${maxReconnectAttempts})`);
                setTimeout(initWebSocket, delay);
            } else {
                console.log('❌ Max reconnection attempts reached');
            }
        };
        
        ws.onerror = function(error) {
            console.error('❌ WebSocket error:', error);
        };
    }

    function updateUI(data) {
        console.log('📊 Updating UI with data:', data);
        
        // Update client count
        if (data.connectedClients !== undefined) {
            const clientCountEl = document.getElementById('clientCount');
            if (clientCountEl) {
                clientCountEl.textContent = data.connectedClients;
            }
        }
        
        // Update cards
        if (data.cards) {
            data.cards.forEach(card => {
                const valueEl = document.getElementById(card.id + '_value');
                const statusEl = document.getElementById(card.id + '_status');
                
                if (valueEl) {
                    valueEl.textContent = card.value;
                    console.log(`📊 Updated ${card.id} value: ${card.value}`);
                }
                if (statusEl) {
                    statusEl.textContent = card.status;
                }
                
                // Update charts
                if (card.type === 6 && card.chartData) { // CARD_CHART = 6
                    updateChart(card.id, card.chartData);
                }
            });
        }
        
        // Update controls
        if (data.controls) {
            data.controls.forEach(control => {
                updateControlUI(control.id, control.state, control.value);
            });
        }
    }

    function updateChart(cardId, chartData) {
        const canvas = document.getElementById(cardId + '_chart');
        if (!canvas) return;
        
        const ctx = canvas.getContext('2d');
        const rect = canvas.getBoundingClientRect();
        canvas.width = rect.width;
        canvas.height = rect.height;
        
        // Clear canvas
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        
        if (chartData.length < 2) return;
        
        // Find min/max values
        const values = chartData.map(point => point.value);
        const minValue = Math.min(...values);
        const maxValue = Math.max(...values);
        const range = maxValue - minValue || 1;
        
        // Set up drawing
        const padding = 20;
        const chartWidth = canvas.width - 2 * padding;
        const chartHeight = canvas.height - 2 * padding;
        
        // Draw grid lines
        ctx.strokeStyle = isDarkMode ? '#334155' : '#e2e8f0';
        ctx.lineWidth = 1;
        
        // Horizontal grid lines
        for (let i = 0; i <= 4; i++) {
            const y = padding + (chartHeight / 4) * i;
            ctx.beginPath();
            ctx.moveTo(padding, y);
            ctx.lineTo(canvas.width - padding, y);
            ctx.stroke();
        }
        
        // Draw chart line
        ctx.strokeStyle = '#3b82f6';
        ctx.lineWidth = 2;
        ctx.beginPath();
        
        chartData.forEach((point, index) => {
            const x = padding + (chartWidth / (chartData.length - 1)) * index;
            const y = padding + chartHeight - ((point.value - minValue) / range) * chartHeight;
            
            if (index === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        });
        
        ctx.stroke();
        
        // Draw data points
        ctx.fillStyle = '#3b82f6';
        chartData.forEach((point, index) => {
            const x = padding + (chartWidth / (chartData.length - 1)) * index;
            const y = padding + chartHeight - ((point.value - minValue) / range) * chartHeight;
            
            ctx.beginPath();
            ctx.arc(x, y, 3, 0, 2 * Math.PI);
            ctx.fill();
        });
    }

    function updateControlUI(id, state, value) {
        console.log(`🎛️ Updating control ${id}: state=${state}, value=${value}`);
        
        // Update switches
        const switchInput = document.getElementById(id + '_input');
        const indicator = document.getElementById(id + '_indicator');
        const status = document.getElementById(id + '_status');
        
        if (switchInput) {
            switchInput.checked = state;
        }
        
        if (indicator) {
            indicator.classList.toggle('active', state);
        }
        
        if (status) {
            status.textContent = state ? 'ON' : 'OFF';
        }
        
        // Update power buttons
        const powerButton = document.getElementById(id);
        const powerText = document.getElementById(id + '_text');
        const powerStatus = document.getElementById(id + '_status');
        
        if (powerButton && powerButton.classList.contains('power-button')) {
            powerButton.classList.toggle('active', state);
            if (powerText) {
                powerText.textContent = state ? 'ON' : 'OFF';
            }
            if (powerStatus) {
                powerStatus.classList.toggle('active', state);
                powerStatus.textContent = state ? 'System Active' : 'System Inactive';
            }
        }
        
        // Update sliders
        const sliderInput = document.getElementById(id + '_input');
        const sliderValue = document.getElementById(id + '_value');
        
        if (sliderInput && sliderInput.type === 'range') {
            sliderInput.value = value;
        }
        
        if (sliderValue) {
            sliderValue.textContent = value;
        }
    }

    function updateConnectionStatus(connected) {
        const statusEl = document.getElementById('connectionStatus');
        if (statusEl) {
            statusEl.className = connected ? 'status-indicator online' : 'status-indicator offline';
            statusEl.innerHTML = connected ? 
                '<div class="status-dot"></div><span>Online</span>' : 
                '<div class="status-dot"></div><span>Offline</span>';
        }
    }

    function toggleControl(id) {
        console.log(`🎛️ Toggling control: ${id}`);
        if (ws && ws.readyState === WebSocket.OPEN) {
            const message = JSON.stringify({ id: id, action: 'toggle' });
            ws.send(message);
            console.log(`📤 Sent: ${message}`);
        } else {
            console.error('❌ WebSocket not connected');
        }
    }

    function clickControl(id) {
        console.log(`🔘 Clicking control: ${id}`);
        if (ws && ws.readyState === WebSocket.OPEN) {
            const message = JSON.stringify({ id: id, action: 'click' });
            ws.send(message);
            console.log(`📤 Sent: ${message}`);
        } else {
            console.error('❌ WebSocket not connected');
        }
    }

    function slideControl(id, value) {
        console.log(`🎚️ Sliding control ${id} to: ${value}`);
        if (ws && ws.readyState === WebSocket.OPEN) {
            const message = JSON.stringify({ id: id, action: 'slide', value: parseInt(value) });
            ws.send(message);
            console.log(`📤 Sent: ${message}`);
        } else {
            console.error('❌ WebSocket not connected');
        }
        
        // Update display immediately for responsiveness
        const valueSpan = document.getElementById(id + '_value');
        if (valueSpan) {
            valueSpan.textContent = value;
        }
    }

    function toggleTheme() {
        isDarkMode = !isDarkMode;
        document.body.classList.toggle('dark', isDarkMode);
        
        const themeIcon = document.getElementById('themeIcon');
        if (themeIcon) {
            themeIcon.textContent = isDarkMode ? '☀️' : '🌙';
        }
        
        localStorage.setItem('darkMode', isDarkMode);
        console.log(`🎨 Theme changed to: ${isDarkMode ? 'dark' : 'light'}`);
        
        // Redraw charts with new theme
        Object.keys(charts).forEach(chartId => {
            const canvas = document.getElementById(chartId + '_chart');
            if (canvas) {
                // Trigger chart redraw on next update
            }
        });
    }

    function loadTheme() {
        const savedTheme = localStorage.getItem('darkMode');
        if (savedTheme === 'true') {
            toggleTheme();
        }
    }

    // Add some visual feedback for button clicks
    document.addEventListener('click', function(e) {
        if (e.target.classList.contains('action-button') || 
            e.target.classList.contains('power-button')) {
            e.target.style.transform = 'scale(0.95)';
            setTimeout(() => {
                e.target.style.transform = '';
            }, 150);
        }
    });

    // Handle page visibility changes
    document.addEventListener('visibilitychange', function() {
        if (document.hidden) {
            console.log('📱 Page hidden - reducing update frequency');
        } else {
            console.log('📱 Page visible - resuming normal updates');
            if (ws && ws.readyState !== WebSocket.OPEN) {
                console.log('🔄 Reconnecting WebSocket...');
                initWebSocket();
            }
        }
    });

    console.log('🚀 Dashboard JavaScript initialized');
  )rawliteral";
}
