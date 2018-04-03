#include "ota.h"
#include "web.h"
//html.h è la pagina di presentazione del acesspoint
#include "html.h"
#include "util.h"
#include "successPage.h"
#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>

//configurazione ap
#define AP_HOME_PAGE "/"
#define AP_SUCCESS_PAGE "/configwifi"
#define WIFI_TIMEOUT_S 30

//definizione pin
#define TEMP_SENSOR 14

//persistenza dato
#define SSID_LENGTH 32
#define PASSWORD_LENGTH 32
#define RATE_LENGTH 16

#define PROBE_RESOLUTION_STRING_DEC 2
#define STRING_LENGTH 512

IPAddress IPAp = IPAddress(10, 0, 0, 1);

const char* mqttServer = "192.168.20.209";
const int mqttPort = 1883;
const char* mqttUsername = "";
const char* mqttPassword = "";

char* configtopic = "/SAEFYCONFIG";
char* tempTopic = "/Temperature";

//configurazione ap
const char *ssid = "dotcomSaefy";
const char *passwordAp = "dotcom2018";

int versionFirmware=10; 
String idDevice="";

String ssidWifiClient ="";
String passwordWifiClient = "";

bool mqttConnected = false;

PubSubClient* mqttClient = nullptr;
//init librerie x i sensori
OneWire wireProtocol(TEMP_SENSOR);
DallasTemperature DS18B20(&wireProtocol);

float tempCorrection = 0;
float temperature = 0.0;

 
char charVal[10];               //temporarily holds data from vals
 
unsigned long postLast = 0;
int postInterval = 100000; // Post every minute

//persistenza dati 
const int eepromAddressWifi = 0;
const int eepromAddressRate = 65;

struct saefyConfigWifi
{
    char ssid[SSID_LENGTH];
    char password[PASSWORD_LENGTH];
}configurationWifi;

struct saefyConfigRate
{
   char rate[RATE_LENGTH];
}configurationRate;

//diversi stati per definire le fasi di funzionamento
enum LoopState
{
	Startup,
	EnteringProvisioningMode,
  ProvisioningMode,
	ConnectWifi,
  ConfigUpdate,
	WorkingMode,
  UpdateMode,
};
//comandi  da inviare via mqtt
enum cmdMqtt
{
	no,
  rate,
	update,
  reset,
};

ESP8266WebServer* _esp8266WebServer;
//comincio con lo stato Startup
LoopState _currentLoopState = Startup;


void handle(){
 	String indexHTMLString = String(indexHTML);
	indexHTMLString.replace("DEVICE_ID", idDevice);
	indexHTMLString.replace("FIRMWARE_VER", String(versionFirmware)); 
	_esp8266WebServer->send(200, "text/html", indexHTMLString);
}

//la funzione intercetta il posta anella pagina di configurazione
void handleConfigWifi(){
  // if (_esp8266WebServer->hasArg("ssid") && _esp8266WebServer->hasArg("password"))
	// 	{
      String successHTMLString = String(successHTML);
      ssidWifiClient = _esp8266WebServer->arg("ssid").c_str();
      passwordWifiClient = _esp8266WebServer->arg("password").c_str();
		
			Serial.println("Data posted from web page:");
			Serial.print("ssid: ");
			Serial.println(ssidWifiClient);
			Serial.print("password: ");
			Serial.println(passwordWifiClient);
      //successHTMLString
      _esp8266WebServer->send(200, "text/html", successHTMLString);
			//riprendo i dati e vado avanti
      _currentLoopState = ConnectWifi;
    //}//if
}//handleCONFIGWIFI

void connectWifi(){
  	//chiudio il server
		_esp8266WebServer->stop();
    bool wiFiConnectionOK = false;
		bool tcpServerConnectionOK = false;
    //tento la conessione
    
		WEB.connectToWiFi(ssidWifiClient.c_str(), passwordWifiClient.c_str(), WIFI_TIMEOUT_S);
		if (WEB.isConnectedToWiFi())
		{
     
      char* s = const_cast<char*>(ssidWifiClient.c_str());
      char* p = const_cast<char*>(passwordWifiClient.c_str());
      //persisto i dati
      SaveConfigWifi(s,p);

			wiFiConnectionOK = true;
    
			Serial.println("CONNESSO WIFI");
      _currentLoopState = UpdateMode;
		
		}//If
    else{
      //ritorno indietro all'access point
      _currentLoopState = Startup;
    }
}

void changeConfig(String cmd, String value){
    char* c = const_cast<char*>(cmd.c_str());
    if(strcmp(c, "rate") == 0)
    {
      postInterval = value.toInt();
      //salvo in memoria il rate
      SaveConfigRate(const_cast<char*>(value.c_str()));
      Serial.println("set rate ");
      Serial.println(value);
    } 
    else if(strcmp(c, "update") == 0)
    {
      _currentLoopState = UpdateMode;
    }
    else if(strcmp(c, "reset") == 0)
    {
      resetEsp();
    }
    else if(strcmp(c, "clearwifi") == 0)
    {
      Serial.println("reset wifi");
      clearWifiCredential();
    }
     else if(strcmp(c, "clear") == 0)
    {
      Serial.println("clear eeprom");
      clearEEprom();
    }

}//change config

/*
Procedura di callback, viene invocata quando presente un nuovo msg nel topic config
*/
void callbackMqtt(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  char msgPayload[] = "";
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    msgPayload[i]=(char)payload[i];
    Serial.print((char)payload[i]);
  }
  String msg = String(msgPayload);
  String cmd = getCmd(msg);
  String value = getValue(msg);
  String id = getIdDeviceMsg(msg);
  Serial.print("id: ");
  Serial.println(id);
  // Serial.println(msg);
  // Serial.println(cmd);
  // Serial.println(value);
  
  //solo se il deviceId corrisponde e il cmd 
  if(id.equals(idDevice))
  {
    Serial.println("Change config .........");
    changeConfig(cmd, value);
  }
}


/*
Creo il client mqtt e mi registro al topic di configurazione
*/
void getConfig(){
  mqttClient = WEB.createMqttClient(mqttServer, mqttPort, mqttUsername, mqttPassword, idDevice.c_str(), callbackMqtt, &mqttConnected);
  if (mqttConnected)
    {
      mqttClient->publish(configtopic, "config?");
      mqttClient->subscribe(configtopic);
      //mqttClient->disconnect();
    }
}

void getMeasurementsPayload(byte probeId, char* probeName, float probeValue, char* string)
{
	//{"DeviceId":11, "ProbeId":1, "Temperature":5}

	strcpy(string, "{");

	strcat(string, "\"DeviceId\":");
	char deviceIdString[1 + 8 * sizeof(uint32)];
	utoa(EspClass().getChipId(), deviceIdString, 10);
	strcat(string, deviceIdString);

	strcat(string, ", ");

	strcat(string, "\"ProbeId\":");
	char probeIdString[1 + 8 * sizeof(byte)];
	utoa(probeId, probeIdString, 10);
	strcat(string, probeIdString);

	strcat(string, ", ");

	strcat(string, "\"");
	strcat(string, probeName);
	strcat(string, "\":");
	char probeValueString[1 + 8 * sizeof(float)];
	dtostrf(probeValue, 0, PROBE_RESOLUTION_STRING_DEC, probeValueString);
	strcat(string, probeValueString);

	strcat(string, "}");
}

void publishTemperature(){
  postLast = millis();
  Serial.println("MQTT:\tPosting temperature...");
  char measurementsPayload[STRING_LENGTH];
  getMeasurementsPayload(0, "Temperature", temperature, measurementsPayload);
  Serial.println(measurementsPayload);
  //char* tPayload = dtostrf(temperature, 1, 2, charVal);
  mqttClient->publish(tempTopic, measurementsPayload);
}//publishTemperature

void getTemperature() {
    DS18B20.requestTemperatures();
    temperature = DS18B20.getTempCByIndex(0);
    temperature = temperature + tempCorrection;
}

//persistenza dati *******

void SaveConfigWifi(char* ssid, char* psw)
{
    strcpy(configurationWifi.ssid, ssid);
    strcpy(configurationWifi.password, psw);
    EEPROM.begin(512);
    delay(10);
    EEPROM.put(eepromAddressWifi, configurationWifi);
    yield();
    EEPROM.end();
}

void SaveConfigRate(char* rate)
{
    strcpy(configurationRate.rate, rate);
    EEPROM.begin(512);
    delay(10);
    EEPROM.put(eepromAddressRate, configurationRate);
    yield();
    EEPROM.end();
}

void readConfigWifi(){
  EEPROM.begin(512);
  EEPROM.get(eepromAddressWifi, configurationWifi);
  EEPROM.end();
}

void readConfigRate(){
  EEPROM.begin(512);
  EEPROM.get(eepromAddressRate, configurationRate);
  EEPROM.end();
}

void clearWifiCredential(){
  SaveConfigWifi("","");
}

void clearEEprom(){
  for (int i = 0; i < 1000; ++i) {
    EEPROM.write(i, -1);
  }
  EEPROM.commit();
}

//persistenza dati *******
void setup() {
  delay(1000);
  Serial.begin(115200); 
  int deviceId = getIdDevice();
  idDevice = String(deviceId);  
  //leggo il valore in memoria del rate
  readConfigRate(); 
  postInterval = atoi(configurationRate.rate);
 
  DS18B20.begin();
  Serial.println("setup ds18b20 sensor...");
  delay(1000);
}

void loop() {
  delay(1000);
  Serial.print("Loop state: ");
  Serial.println(_currentLoopState);
  //loop per controllo canale config
  if(mqttConnected){
    mqttClient->loop();
  }
  switch (_currentLoopState)
	{
    case Startup:
    {
      readConfigWifi();
      Serial.println("wifi: ");
      Serial.println(configurationWifi.ssid);
      Serial.println(configurationWifi.password);
      WEB.connectToWiFi(configurationWifi.ssid, configurationWifi.password, WIFI_TIMEOUT_S);
		  if (WEB.isConnectedToWiFi())
		  {
        Serial.println("Sono gia conesso al wifi");
        delay(4000);
        //se sono già consesso passo all'upddate
        _currentLoopState = UpdateMode;
      }
      else
      {
        //nello stato di Startup avvio l'access point con l'utilizzo di funzione della
        //libreria web.h
        Serial.println("Avvio AP");
        WEB.createAccessPoint(IPAp, ssid, passwordAp);
        _esp8266WebServer = WEB.createWebServer();
        //passo al secondo stato
        _currentLoopState = EnteringProvisioningMode;
      }
    }
    break;
    case EnteringProvisioningMode:
    {
        //entro nello stato del handle client
        Serial.print("Handle client ");
        _esp8266WebServer->on(AP_HOME_PAGE,handle);
        _esp8266WebServer->on(AP_SUCCESS_PAGE,HTTP_POST,handleConfigWifi);
        _currentLoopState = ProvisioningMode;
       
    }
    break;
    case ProvisioningMode:
    {
      _esp8266WebServer->handleClient();
    }
    break;
    case ConnectWifi:
    {
      connectWifi();
    }
    break;
    case UpdateMode:
    {
      checkForUpdates(idDevice,versionFirmware);
      _currentLoopState = ConfigUpdate;
      if (mqttConnected)
      {
        mqttClient->disconnect();
      }
    }
    break;
    case ConfigUpdate:
    {
      //mqtt config download
      getConfig();
      _currentLoopState = WorkingMode;
    }
    break;
    case WorkingMode:
    {
      Serial.print("post last: ");
      Serial.println(postLast);
      Serial.print("interval: ");
      Serial.println(postInterval);
      if (millis() - postLast < postInterval)
      {
        Serial.println("wait...");
        break;
      }
      else{
        Serial.println("read temp...");
        getTemperature();
        publishTemperature();
      }
    }
    break;
  }//switch
}//loop 
