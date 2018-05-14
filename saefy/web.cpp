#include "Web.h"

ESP8266WebServer _webServer(80);

Web WEB = Web();


Web::Web()
{
	_wiFi = &WiFi;
	_wiFi->persistent(false);
	_wiFi->disconnect(true);
	_wiFi->mode(WIFI_OFF);
}
Web::~Web()
{
	disableWiFi();
}


void Web::createAccessPoint(IPAddress ipAddress, const char* ssid)
{
	createAccessPoint(ipAddress, ssid, nullptr);
}
void Web::createAccessPoint(IPAddress ipAddress, const char* ssid, const char* passwordAp)
{
	if (_wiFi->getMode() == WIFI_STA)
	{
		_wiFi->mode(WIFI_AP_STA);
	}
	else
	{
		_wiFi->mode(WIFI_AP);
	}
	_wiFi->softAPConfig(ipAddress, ipAddress, IPAddress(255, 255, 255, 0));
	_wiFi->softAP(ssid, passwordAp);

	Serial.print(F("WiFi access point set: "));
	if (passwordAp == nullptr)
	{
		Serial.println(ssid);
	}
	else
	{
		Serial.print(ssid);
		Serial.print(F(", "));
		Serial.println(passwordAp);
	}

	Serial.print(F("Hot spot IP: "));
	Serial.println(_wiFi->softAPIP().toString());
}

void Web::connectToWiFi(const char *ssid, const char *passwordWifi, byte timeout_s)
{
	Serial.print(F("Connecting to: "));
	Serial.print(ssid);
	Serial.print(F(", "));
	Serial.print(passwordWifi);
	Serial.print(F("..."));

	if (_wiFi->getMode() == WIFI_AP)
	{
		_wiFi->mode(WIFI_AP_STA);
	}
	else
	{
		_wiFi->mode(WIFI_STA);
	}
	if (isConnectedToWiFi())
	{
		_wiFi->disconnect(false);
	}

	_wiFi->begin(ssid, passwordWifi);
	byte current_s = 0;
	while ((!isConnectedToWiFi())
		&& (current_s <= timeout_s))
	{
		Serial.print(F("."));
		delay(1000);
		current_s++;
	}
	Serial.println();

	if (isConnectedToWiFi())
	{
		Serial.print(F("Connected to WiFi; IP address: "));
		Serial.println(WiFi.localIP().toString());
	}
	else
	{
		Serial.print(F("Connection to WiFi could not be estabilished; status: "));
		Serial.println(_wiFi->status());
	}
}
bool Web::isConnectedToWiFi()
{
	return 	_wiFi->status() == WL_CONNECTED;
}
void Web::disableWiFi()
{
	Serial.println(F("Closing WiFi..."));

	_wiFi->softAPdisconnect(true);
	_wiFi->persistent(false);
	_wiFi->disconnect(true);
	_wiFi->mode(WIFI_OFF);
 // _wiFi->forceSleepBegin();
  //delay(1);
}

int32_t Web::getStrengthSignal(){
	if (_wiFi->status() == WL_CONNECTED)
	{
		return _wiFi->RSSI();
	}
	else{
		return 0;
	}
}

void Web::handleNotFound()
{
	String message = "Not Found\n\n";
	message += "URI: ";
	message += _webServer.uri();
	message += "\nMethod: ";
	message += (_webServer.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += _webServer.args();
	message += "\n";
	for (byte i = 0; i < _webServer.args(); i++)
	{
		message += " " + _webServer.argName(i) + ": " + _webServer.arg(i) + "\n";
	}
	_webServer.send(404, "text/plain", message);
}
ESP8266WebServer* Web::createWebServer()
{
    
	//_webServer = ESP8266WebServer(80);
	_webServer.onNotFound(std::bind(&Web::handleNotFound, this));
	_webServer.begin();
	Serial.println(F("HTTP server started"));

	return &_webServer;
}

PubSubClient* Web::createMqttClient(const char* broker, uint16_t port, const char* mqttUsername, const char* mqttPassword, const char* id, bool* connected)
{
	return createMqttClient(broker, port, mqttUsername, mqttPassword, id, nullptr, connected);
}
PubSubClient* Web::createMqttClient(const char* broker, uint16_t port, const char* mqttUsername, const char* mqttPassword, const char* id, std::function<void(char*, byte*, uint16_t)> callback, bool* connected)
{
	Serial.print(F("Connecting to MQTT broker: "));
	Serial.print(broker);
	Serial.print(F(":"));
	Serial.print(port);
	Serial.print(F("; username: "));
	Serial.print(mqttUsername);
	Serial.print(F("; password: "));
	Serial.print(mqttPassword);
	Serial.print(F("; id: "));
	Serial.print(id);
	Serial.println(F("..."));


	_mqttClient = PubSubClient(_tcpClient);
	_mqttClient.setServer(broker, port);
	*connected = _mqttClient.connect(id, mqttUsername, mqttPassword);

	if (*connected)
	{
		Serial.println(F("Connected to MQTT broker."));
		
		if (callback != nullptr)
		{
			_mqttClient.setCallback(callback);
			Serial.println(F("MQTT callback set."));
		}
	}
	else
	{
		Serial.print(F("Connection to MQTT broker could not be estabilished; status: "));
		Serial.println(_mqttClient.state());
    
	}

	return &_mqttClient;
}
