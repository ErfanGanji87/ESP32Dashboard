#include "ESP32Dashboard.h"

const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

ESP32Dashboard dashboard;

float temperature = 25.0, humidity = 50.0;
int motorRPM = 1000, brightness = 50, waterLevel = 65;
bool systemPower = false, ledState = false, fanState = false;

void toggleLED() {
  ledState = !ledState;
  dashboard.updateCard("led_status", ledState ? "ON" : "OFF", ledState ? "Operation successful" : "LED turned off");
  Serial.printf("[ACTION] LED toggled to %s\n", ledState ? "ON" : "OFF");
}

void toggleFan(bool state) {
  fanState = state;
  dashboard.updateCard("fan_status", fanState ? "ACTIVE" : "INACTIVE", fanState ? "Cooling active" : "Fan off");
  Serial.printf("[ACTION] Fan %s\n", fanState ? "activated" : "deactivated");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Initializing ESP32 Dashboard...");

  if (!dashboard.begin(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("WiFi connection failed!");
    while (true) delay(1000);
  }

  dashboard.setTitle("ESP32 Smart System", "Home Automation Dashboard");
  dashboard.setUpdateInterval(1000);

  dashboard.addTemperatureCard("Temperature", []() {
    temperature += random(-5, 5) / 10.0;
    return constrain(temperature, 15.0, 35.0);
  });

  dashboard.addHumidityCard("Humidity", []() {
    humidity += random(-10, 10) / 10.0;
    return constrain(humidity, 30.0, 70.0);
  });

  dashboard.addMotorRPMCard("Motor RPM", []() {
    motorRPM += random(-50, 50);
    return constrain(motorRPM, 800, 1500);
  });

  dashboard.addPercentageCard("Water Level", "Main Tank", []() {
    waterLevel += random(-3, 3);
    return constrain(waterLevel, 0, 100);
  }, "blue");

  dashboard.addStatusCard("System Status", "Control Center",
    []() { return systemPower ? "ACTIVE" : "INACTIVE"; },
    []() { return systemPower ? "All systems normal" : "System off"; },
    "green");

  dashboard.addCustomCard("LED Status", "Main Light",
    []() { return ledState ? "ON" : "OFF"; },
    []() { return ledState ? "Operation successful" : "Ready"; },
    "yellow", "💡");

  dashboard.addCustomCard("Fan Status", "Cooling System",
    []() { return fanState ? "ACTIVE" : "INACTIVE"; },
    []() { return fanState ? "Running" : "Off"; },
    "cyan", "🌀");

  dashboard.addChartCard("Temperature Chart", "Temperature changes", []() { return temperature; }, "red", 15);

  dashboard.addPowerButton("Power Supply", "Main System Control", [](bool state) {
    systemPower = state;
    dashboard.updateCard("status_0", systemPower ? "ACTIVE" : "INACTIVE", systemPower ? "All systems normal" : "System off");
  });

  dashboard.addSwitch("LED Control", "Main Light", [](bool state) {
    ledState = state;
    dashboard.updateCard("custom_0", ledState ? "ON" : "OFF", ledState ? "Operation successful" : "LED turned off");
  }, "yellow");

  dashboard.addButton("Toggle LED", "Change light state", toggleLED, "orange");
  dashboard.addSwitch("Fan Control", "Cooling System", toggleFan, "cyan");

  dashboard.addSlider("Brightness", "Light intensity", [](int value) {
    brightness = value;
    Serial.printf("[ACTION] Brightness set to %d%%\n", brightness);
  }, 0, 100, "blue");

  Serial.printf("Dashboard ready at: http://%s\n", dashboard.getLocalIP().c_str());
}

void loop() {
  dashboard.loop();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    dashboard.printSystemStatus();
    lastPrint = millis();
  }
}
