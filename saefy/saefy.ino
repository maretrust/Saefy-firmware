#include "ota.h"
#include "web.h"
//html.h è la pagina di presentazione del acesspoint
#include "html.h"
#include "util.h"
#include "successPage.h"
#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Ticker.h>
// #include "led.h"

//configurazione ap
#define AP_HOME_PAGE "/"
#define AP_SUCCESS_PAGE "/configwifi"
#define WIFI_TIMEOUT_S 30

//definizione pin
#define TEMP_SENSOR 14
#define LEDB 13
#define LEDR 12
#define BUTTON 0

//timers
Ticker buttonTimer;
unsigned long pressedCount = 0;

//persistenza dato
#define SSID_LENGTH 32
#define PASSWORD_LENGTH 32
#define RATE_LENGTH 16

#define PROBE_RESOLUTION_STRING_DEC 2
#define STRING_LENGTH 512

IPAddress IPAp = IPAddress(10, 0, 0, 1);



//extern Web WEB;

const char* mqttServer = "mqtt.saefy.eu";
const int mqttPort = 5783;
const char* mqttUsername = "atlantis";
const char* mqttPassword = "rBfsQj53H9c471";

char* configtopic = "/SAEFYCONFIG";
String topicDevice = "SAEFY/ATLANTIS/";

//configurazione ap
char ssid[20] = "SF";
const char *passwordAp = "dotcom2018";

int versionFirmware = 12;
String idDevice = "";

String ssidWifiClient = "";
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
unsigned long timeLast;
long postInterval = 100000; // Post every minute
int minInterval = 9000;
bool stopRead = false;

long rssi = -1;

//persistenza dati
const int eepromAddressWifi = 0;
const int eepromAddressRate = 65;
const int eepromAddressPsc = 95;

struct saefyConfigWifi
{
  char ssid[SSID_LENGTH];
  char password[PASSWORD_LENGTH];
} configurationWifi;

struct saefyConfigRate
{
  char rate[RATE_LENGTH];
} configurationRate;

//diversi stati per definire le fasi di funzionamento
enum LoopState
{
  Startup,
  AccessPoint,
  EnteringProvisioningMode,
  ProvisioningMode,
  ConnectWifi,
  catchWifi,
  ConfigUpdate,
  WorkingMode,
  printMem,
  UpdateMode,
};

ESP8266WebServer* _esp8266WebServer;
//comincio con lo stato Startup
LoopState _currentLoopState = Startup;


void handle() {
  String indexHTMLString = String(indexHTML);
  indexHTMLString.replace("DEVICE_ID", idDevice);
  indexHTMLString.replace("FIRMWARE_VER", String(versionFirmware));
  _esp8266WebServer->send(200, "text/html", indexHTMLString);
}

//la funzione intercetta il posta anella pagina di configurazione
void handleConfigWifi() {
  // if (_esp8266WebServer->hasArg("ssid") && _esp8266WebServer->hasArg("password"))
  //  {
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

void connectWifi() {
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
    SaveConfigWifi(s, p);

    wiFiConnectionOK = true;

    Serial.println("CONNESSO WIFI");
    ledOn();
    _currentLoopState = UpdateMode;

  }//If
  else {
    //ritorno indietro all'access point
    _currentLoopState = Startup;
  }
}

void recuperaWifi() {
  bool wiFiConnectionOK = false;
  bool tcpServerConnectionOK = false;
  //tento la conessione
  readConfigWifi();
  WEB.connectToWiFi(configurationWifi.ssid, configurationWifi.password, WIFI_TIMEOUT_S);
  if (WEB.isConnectedToWiFi())
  {

    wiFiConnectionOK = true;

    Serial.println("CONNESSO WIFI");
    ledOn();
    //  connectMqtt();
    // _currentLoopState = WorkingMode;
    _currentLoopState = ConfigUpdate;

  }//If
}

void changeConfig(String cmd, String value) {
  char* c = const_cast<char*>(cmd.c_str());
  char* v = const_cast<char*>(value.c_str());
  if (strcmp(c, "rate") == 0)
  {
    postInterval = value.toInt();
    
    if(postInterval < minInterval){
      postInterval = minInterval;
      itoa(postInterval, v, 10);
    }
    //salvo in memoria il rate
    SaveConfigRate(const_cast<char*>(v));
    Serial.println("set rate ");
    Serial.println(v);
  }
  else if (strcmp(c, "update") == 0)
  {
    _currentLoopState = UpdateMode;
  }
  else if (strcmp(c, "reset") == 0)
  {
    resetEsp();
  }
  else if (strcmp(c, "clearwifi") == 0)
  {
    Serial.println("reset wifi");
    //clearWifiCredential();
    _currentLoopState = AccessPoint;
  }
  else if (strcmp(c, "clear") == 0)
  {
    Serial.println("clear eeprom");
    clearEEprom();
  }
  else if (strcmp(c, "printmem") == 0)
  {
    _currentLoopState = printMem;
  }
  else if (strcmp(c, "stop") == 0)
  {
    if (strncmp(v, "true", 4) == 0)
    {
      stopRead = true;
    }
    else if (strncmp(v, "false", 5) == 0)
    {
      stopRead = false;
    }
  }

}//change config

/*
  Procedura di callback, viene invocata quando presente un nuovo msg nel topic config
*/
void callbackMqtt(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.println("");
  char msgPayload[] = "";
  for (int i = 0; i < length; i++) {
    msgPayload[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  String msg = String(msgPayload);
  String cmd = getCmd(msg);
  String value = getValue(msg);
  String id = getIdDeviceMsg(msg);
  Serial.println("id: ");
  Serial.println(id);
  // Serial.println(msg);
  Serial.println(cmd);
  Serial.println(value);

  //solo se il deviceId corrisponde e il cmd
  if (id.equals(idDevice))
  {
    Serial.println("Change config .........");
    changeConfig(cmd, value);
  }
}

/*
  Creo il client mqtt e mi registro al topic di configurazione
*/
void connectMqtt() {
  Serial.println("Mqtt connect11111: ");
  mqttClient = WEB.createMqttClient(mqttServer, mqttPort, mqttUsername, mqttPassword, idDevice.c_str(), callbackMqtt, &mqttConnected);

  if (mqttConnected)
  {

    mqttClient->publish(configtopic, idDevice.c_str());
    mqttClient->subscribe(configtopic);
    //mqttClient->disconnect();
  }
}

void getMeasurementsPayload(byte probeId, char* probeName, float probeValue, char* string)
{
  //{"DeviceId":11, "ProbeId":1, "Temperature":5}

  strcpy(string, "{");

  strcat(string, "\"DeviceId\":");

  strcat(string, const_cast<char*>(idDevice.c_str()));

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

  strcat(string, ", ");
  strcat(string, "\"R\":");
  char cstr[16];
  itoa(postInterval,  cstr, 10);

  strcat(string, cstr);
  strcat(string, ", ");

  /*strcat(string, "\"S\":");
  strcat(string, const_cast<char*>(stopRead ? "true" : "false"));
  strcat(string, ", ");
  */

  
  strcat(string, "\"F\":");
  char fstr[4];
  itoa(versionFirmware,  fstr, 10);
  strcat(string, fstr);
  strcat(string, ", ");

  strcat(string, "\"SW\":");
  char crssi[16];
  ltoa(rssi,  crssi, 10);
  strcat(string, crssi);

  strcat(string, "}");
}

void publishTemperature() {
  postLast = millis();
  Serial.println("MQTT:\tPosting temperature...");
  char measurementsPayload[STRING_LENGTH];
  getMeasurementsPayload(0, "Temperature", temperature, measurementsPayload);
  Serial.println(measurementsPayload);
  //char* tPayload = dtostrf(temperature, 1, 2, charVal);
  String tD = topicDevice + idDevice;
  mqttClient->publish(const_cast<char*>(tD.c_str()), measurementsPayload);
  Serial.println("post topic: ");
  Serial.println(const_cast<char*>(tD.c_str()));
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
  EEPROM.commit();
  EEPROM.end();
}

void SaveConfigRate(char* rate)
{
  strcpy(configurationRate.rate, rate);
  EEPROM.begin(512);
  delay(10);
  EEPROM.put(eepromAddressRate, configurationRate);
  yield();
  EEPROM.commit();
  EEPROM.end();
}

void readConfigWifi() {
  EEPROM.begin(512);
  EEPROM.get(eepromAddressWifi, configurationWifi);
  EEPROM.end();
}

void readConfigRate() {
  EEPROM.begin(512);
  EEPROM.get(eepromAddressRate, configurationRate);
  EEPROM.end();
}
/*
void clearWifiCredential() {
  SaveConfigWifi('X', 'X');
  //clearEEprom();
}
*/

void clearEEprom() {
  EEPROM.begin(512);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.end();
}

//persistenza dati *******

//led
void ledBlink() {
  digitalWrite(LEDR, false);
  digitalWrite(LEDB, true);
  delay(1000);
  digitalWrite(LEDB, false);
}


void ledOn() {
  digitalWrite(LEDB, true);
  digitalWrite(LEDR, false);
}

void ledRed() {
  digitalWrite(LEDR, true);
  digitalWrite(LEDB, false);
}

//button
void button() {
  if (!digitalRead(BUTTON)) {
    pressedCount++;
  }
  else {
    if (pressedCount > 50) {
      Serial.println("Več kot 5 sekund je mimo, brišem eeprom!");
      //clearEEprom();
      //setup();
     // _currentLoopState = Startup;
      _currentLoopState = AccessPoint;
     // delay(1000);
      pressedCount = 0;
    }

    pressedCount = 0;
  }
}

//**********

void reafMem(int ind) {
  byte val = byte(EEPROM.read(ind));
  Serial.print(ind);
  Serial.print("\t");
  Serial.print(val);
  Serial.print("\t");
  ind = ind + 1;
  if (ind % 60 == 0)
    Serial.println("\t");
}
//********
void setup() {
  delay(1000);
  WEB = Web();

  //disable watchdogtimer
  //ESP.wdtDisable();
  Serial.begin(115200);
  idDevice = getIdDevice();
  Serial.print("IdDevice");
  Serial.println(idDevice);
  //leggo il valore in memoria del rate
  readConfigRate();
  postInterval = atoi(configurationRate.rate);
  
  if(postInterval < minInterval){
    postInterval = minInterval;
  }

  DS18B20.begin();
  Serial.println("setup ds18b20 sensor...");

  //configLED();
  pinMode(LEDB, OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  buttonTimer.attach(0.1, button);

  //* mqttClient = nullptr;

  delay(1000);
}


void loop() {
  //ESP.wdtFeed();
  Serial.print("Loop state: ");
  Serial.println(_currentLoopState);

  rssi = WEB.getStrengthSignal();
  Serial.print("Strength signal: ");
  Serial.println(rssi);
  delay(1000);
  //loop per controllo canale config
  Serial.print("Mqtt version 12: ");
  Serial.println(mqttConnected);
 

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
          ledOn();
          Serial.println("Sono gia conesso al wifi");
          delay(1000);
          //se sono già consesso passo all'upddate
          _currentLoopState = UpdateMode;
        }
      }
      break;
    case AccessPoint:
    {
        //nello stato di Startup avvio l'access point con l'utilizzo di funzione della
        Serial.println("Avvio AP");
        //ssid = "SF";
        memset(ssid, 0, sizeof(ssid));
        
        strcpy(ssid,"SF");
        Serial.print("prima ");
        Serial.print(ssid);
        char* id = const_cast<char*>(idDevice.c_str());
        strcat(ssid, id);
        Serial.print("dopo ");
        Serial.print(ssid);
        WEB.createAccessPoint(IPAp, ssid, passwordAp);
        _esp8266WebServer = WEB.createWebServer();
        //passo al secondo stato
        _currentLoopState = EnteringProvisioningMode;
        delay(1000);
    }
    break;
    case EnteringProvisioningMode:
      {
        //entro nello stato del handle client
        Serial.print("Handle client ");
        _esp8266WebServer->on(AP_HOME_PAGE, handle);
        _esp8266WebServer->on(AP_SUCCESS_PAGE, HTTP_POST, handleConfigWifi);
        _currentLoopState = ProvisioningMode;

      }
      break;
    case ProvisioningMode:
      {
        ledRed();
        _esp8266WebServer->handleClient();
        yield();
      }
      break;
    case ConnectWifi:
      {
        connectWifi();
      }
      break;
    case catchWifi:
      {
        Serial.println("catch connection ...");
        // mqttConnected = false;
        recuperaWifi();

      }
      break;
    case UpdateMode:
      {
        checkForUpdates(idDevice, versionFirmware);
        _currentLoopState = ConfigUpdate;
      }
      break;
    case ConfigUpdate:
      {
        //  WEB = Web();
        connectMqtt();

        //setup();
        //_currentLoopState = catchWifi;
        _currentLoopState = WorkingMode;
      }
      break;
    case WorkingMode:
      {
        //rssi = WEB.getStrengthSignal();
        if (rssi == 0) {
          setup();
          _currentLoopState = catchWifi;
        }
        else {
          if (!mqttClient->connected()) {
            Serial.print("Connected mqtt: ");
            Serial.println(mqttConnected);
            _currentLoopState = ConfigUpdate;
          }
          else{
            mqttClient->loop();
          }
          if (!stopRead) {
            ledBlink();
            Serial.print("Time: ");
            timeLast = millis();
            Serial.println(timeLast);
            Serial.print("Past: ");
            Serial.println(postLast);
            if (timeLast - postLast < postInterval)
            {
              Serial.println("wait...");
              break;
            }
            else {
              Serial.print("wifi...");
              Serial.println(WEB.isConnectedToWiFi());

              getTemperature();
              publishTemperature();

            }
          }//stopRead
          ledOn();
        }//rssi=0
      }
      break;
  }//switch
}//loop

