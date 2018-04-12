#ifndef _WEB_h
#define _WEB_h

// #if defined(ARDUINO) && ARDUINO >= 100
// #include "arduino.h"
// #else
// #include "WProgram.h"
// #endif
#endif


#include "ESP8266WebServer.h"
#include "ESP8266WiFi.h"
#include "PubSubClient.h"

class Web
{
public:
	Web();
	~Web();

	void createAccessPoint(IPAddress ipAddress, const char* ssid);
	void createAccessPoint(IPAddress ipAddress, const char* ssid, const char* password);

	void connectToWiFi(const char *ssid, const char *password, byte timeout_s);
	bool isConnectedToWiFi();
	void disableWiFi();
	
	ESP8266WebServer* createWebServer();

	PubSubClient* createMqttClient(const char* broker, uint16_t port, const char* username, const char* password, const char* id, bool* connected);
	PubSubClient* createMqttClient(const char* broker, uint16_t port, const char* username, const char* password, const char* id, std::function<void(char*, byte*, uint16_t)> callback, bool* connected);


private:
	ESP8266WiFiClass* _wiFi;
	

	void handleNotFound();
	
	WiFiClient _tcpClient;
	PubSubClient _mqttClient;

};


extern Web WEB;
