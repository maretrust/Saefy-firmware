#include "ota.h"
#include "web.h"
//html.h è la pagina di presentazione del acesspoint
#include "html.h"
#include "util.h"
#include "successPage.h"

#define AP_HOME_PAGE "/"
#define AP_SUCCESS_PAGE "/configwifi"
#define WIFI_TIMEOUT_S 30
#define PROVISIONING_IP_1 10
#define PROVISIONING_IP_2 0
#define PROVISIONING_IP_3 0
#define PROVISIONING_IP_4 1

IPAddress IPAp = IPAddress(PROVISIONING_IP_1, PROVISIONING_IP_2, PROVISIONING_IP_3, PROVISIONING_IP_4);

const char* mqttServer = "192.168.0.137";
const int mqttPort = 1883;
const char* mqttUsername = "";
const char* mqttPassword = "";

char *topic = "/SAEFYCONFIG";

const char *ssid = "dotcom";
const char *passwordAp = "dotcom2018";

String versionFirmware="1.0"; 
String idDevice="";

String ssidWifiClient ="";
String passwordWifiClient = "";

bool mqttConnected = false;

PubSubClient* mqttClient = nullptr;

//working mode setup
extern int freq = 60;

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
cmdMqtt _currentCmdMqtt = no;

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
      freq = value.toInt();
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
  
  //solo se il deviceId corrisponde
  if(id==idDevice)
  {
    Serial.println("Change config .........");
    changeConfig(cmd, value);
  }
}



void getConfig(){
  mqttClient = WEB.createMqttClient(mqttServer, mqttPort, mqttUsername, mqttPassword, idDevice.c_str(), callbackMqtt, &mqttConnected);
  if (mqttConnected)
    {
      mqttClient->publish(topic, "config?");
      mqttClient->subscribe(topic);
      //mqttClient->disconnect();
    }
}


void setup() {
  delay(1000);
  int deviceId = getIdDevice();
  idDevice = String(deviceId); 
  Serial.begin(115200);  
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
	    //nello stato di Startup avvio l'access point con l'utilizzo di funzione della
	    //libreria web.h
      Serial.println("Avvio AP");
	    WEB.createAccessPoint(IPAp, ssid, passwordAp);
		  _esp8266WebServer = WEB.createWebServer();
      //passo al secondo stato
      _currentLoopState = EnteringProvisioningMode;
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
     
    }
    break;
  }//switch
}//loop 
