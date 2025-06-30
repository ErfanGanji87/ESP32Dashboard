#ifndef ESP32DASHBOARD_H
#define ESP32DASHBOARD_H

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <functional>

// Card types
enum CardType {
	CARD_TEMPERATURE,
	CARD_HUMIDITY,
	CARD_MOTOR_RPM,
	CARD_CUSTOM,
	CARD_STATUS,
	CARD_PERCENTAGE,
	CARD_CHART
};

// Control types
enum ControlType {
	CONTROL_SWITCH,
	CONTROL_BUTTON,
	CONTROL_POWER_BUTTON,
	CONTROL_SLIDER
};

// Chart data point
struct ChartDataPoint {
	unsigned long timestamp;
	float value;
};

// Card structure
struct DashboardCard {
	String id;
	String title;
	String description;
	String value;
	String status;
	String color;
	String icon;
	CardType type;
	std::function<String()> valueCallback;
	std::function<String()> statusCallback;
	std::vector<ChartDataPoint> chartData;
	int maxDataPoints;
};

// Control structure
struct DashboardControl {
	String id;
	String title;
	String description;
	ControlType type;
	bool state;
	int value;
	int minValue;
	int maxValue;
	String color;
	std::function<void(bool)> switchCallback;
	std::function<void(int)> sliderCallback;
	std::function<void()> buttonCallback;
};

class ESP32Dashboard {
private:
	WebServer* server;
	WebSocketsServer* webSocket;

	std::vector<DashboardCard> cards;
	std::vector<DashboardControl> controls;

	String ssid;
	String password;
	String dashboardTitle;
	String dashboardSubtitle;
	bool darkMode;

	unsigned long lastUpdate;
	unsigned long updateInterval;

	bool serialMonitoring;
	unsigned long serialBaudRate;
	void logToSerial(String message, String category = "INFO");
	void printSeparator();

	void handleRoot();
	void handleApiData();
	void handleApiControl();
	void handleNotFound();
	void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
	void sendDataToClients();
	String generateHTML();
	String generateCards();
	String generateControls();
	String generateCSS();
	String generateJavaScript();
	void addChartDataPoint(const char* cardId, float value);

public:
	ESP32Dashboard();
	~ESP32Dashboard();

	// Initialization
	bool begin(const char* ssid, const char* password, int port = 80, int wsPort = 81);
	void setTitle(const char* title, const char* subtitle = "");
	void setUpdateInterval(unsigned long interval);
	void loop();

	// Card management
	String addTemperatureCard(const char* title, std::function<float()> callback);
	String addHumidityCard(const char* title, std::function<float()> callback);
	String addMotorRPMCard(const char* title, std::function<int()> callback);
	String addStatusCard(const char* title, const char* description, std::function<String()> valueCallback, std::function<String()> statusCallback, const char* color = "blue");
	String addPercentageCard(const char* title, const char* description, std::function<int()> callback, const char* color = "green");
	String addCustomCard(const char* title, const char* description, std::function<String()> valueCallback, std::function<String()> statusCallback, const char* color = "purple", const char* icon = "");
	String addChartCard(const char* title, const char* description, std::function<float()> callback, const char* color = "blue", int maxPoints = 20);

	// Control management
	String addSwitch(const char* title, const char* description, std::function<void(bool)> callback, const char* color = "blue");
	String addButton(const char* title, const char* description, std::function<void()> callback, const char* color = "green");
	String addPowerButton(const char* title, const char* description, std::function<void(bool)> callback);
	String addSlider(const char* title, const char* description, std::function<void(int)> callback, int min = 0, int max = 100, const char* color = "blue");

	// State management
	bool getSwitchState(const char* id);
	void setSwitchState(const char* id, bool state);
	int getSliderValue(const char* id);
	void setSliderValue(const char* id, int value);

	// Card value updates
	void updateCard(const char* id, const char* value, const char* status = "");

	// Utility functions
	String getLocalIP();
	bool isConnected();
	int getConnectedClients();

	// Serial monitoring and configuration
	void enableSerialMonitoring(bool enable = true);
	void printSystemStatus();
	void printAllStates();
	void setSerialBaudRate(unsigned long baudRate);

	// WiFi configuration helpers
	void setWiFiCredentials(const char* ssid, const char* password);
	void printWiFiStatus();
	void printWebServerInfo();

	// Event callbacks
	std::function<void()> onClientConnect;
	std::function<void()> onClientDisconnect;
	std::function<void(String, String)> onCustomMessage;
};

#endif
