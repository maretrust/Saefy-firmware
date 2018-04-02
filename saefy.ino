#include "ota.h"
#include "web.h"
//html.h è la pagina di presentazione del acesspoint
#include "html.h"
#include "util.h"
#include "successPage.h"
#include "DHT.h"
#include <EEPROM.h>

//configurazione ap
#define AP_HOME_PAGE "/"
#define AP_SUCCESS_PAGE "/configwifi"
#define WIFI_TIMEOUT_S 30

//definizione pin
#define DHTTYPE DHT22 // DHT11 or DHT22
#define DHTPIN  14

//persistenza dato
#define SSID_LENGTH 32
#define PASSWORD_LENGTH 32
#define RATE_LENGTH 16


IPAddress IPAp = IPAddress(10, 0, 0, 1);

const char* mqttServer = "192.168.1.65";
const int mqttPort = 1883;
const char* mqttUsername = "";
const char* mqttPassword = "";

char* configtopic = "/SAEFYCONFIG";
char* tempTopic = "/Temperature";

//configurazione ap
const char *ssid = "dotcom";
const char *passwordAp = "dotcom2018";

String versionFirmware="1.0"; 
String idDevice="";

String ssidWifiClient ="";
String passwordWifiClient = "";

bool mqttConnected = false;

PubSubClient* mqttClient = nullptr;

//working mode setup

DHT dht(DHTPIN, DHTTYPE);
unsigned long dhtLast = 0;
const float avgForce = 10.0;  // Force of which the averaging function is using to affect the reading
const float avgSkip = 0.5; // Skip the averaging function if the reading difference is bigger than this since last reading
 
float avgHumidity, avgTemperature;
 
char charVal[10];               //temporarily holds data from vals
 
unsigned long postLast = 0;
extern int postInterval = 100000; // Post every minute

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
	indexHTMLString.replace("FIRMWARE_VER", versionFirmware); 
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
  Serial.println(id);
  Serial.println(msg);
  Serial.println(cmd);
  Serial.println(value);
  
  //solo se il deviceId corrisponde e il cmd 
  if(id==idDevice && cmd!="")
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

void publishTemperature(){
  postLast = millis();
  Serial.println("MQTT:\tPosting temperature...");
  char* tPayload = dtostrf(avgTemperature, 1, 2, charVal);
  mqttClient->publish(tempTopic, tPayload);
}//publishTemperature

void readDHT()
{

  float tempHumi = dht.readHumidity();
  float tempTemp = dht.readTemperature();
 
  if (isnan(tempHumi) || isnan(tempTemp))
  {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
 
  if (abs(tempTemp - avgTemperature) > avgSkip)
  {
    avgTemperature = tempTemp;
  }
  else
  {
    avgTemperature = (avgTemperature *  (100.0-avgForce)/100.0) + ((avgForce/100.0) * tempTemp);
  }
 
  if (abs(tempHumi - avgHumidity) > avgSkip)
  {
    avgHumidity = tempHumi;
  }
  else
  {
    avgHumidity = (avgHumidity * ((100.0-avgForce)/100.0)) + ((avgForce/100.0) * tempHumi);
  }
 
  Serial.print("DHT22:\tHumidity: ");
  Serial.print(avgHumidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(avgTemperature);
  Serial.println(" *C ");
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
//persistenza dati *******
void setup() {
  delay(1000);
  Serial.begin(115200); 
  int deviceId = getIdDevice();
  idDevice = String(deviceId);  
  //leggo il valore in memoria del rate
  readConfigRate(); 
  postInterval = atoi(configurationRate.rate);
 
  dht.begin();
  Serial.println("Delaying startup to allow DHT sensor to be started properly...");
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
      checkForUpdates();
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
      if (millis() - postLast < postInterval)
      {
        Serial.println("wait...");
        break;
      }
      else{
        Serial.println("read temp...");
        readDHT();
        publishTemperature();
      }
    }
    break;
  }//switch
}//loop 
