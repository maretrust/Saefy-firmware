#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>


//url repository firmware
const char* fwUrlBase = "http://192.168.20.209:8888/fota/";

// String getMAC()
// {
//   uint8_t mac[6];
//   char result[14];
//   snprintf( result, sizeof( result ), "%02x%02x%02x%02x%02x%02x", mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );
//  // return String( result );
//  return "100";
// }

void checkForUpdates(String deviceID, int fwVersion) {
  
  String fwURL = String( fwUrlBase );
  fwURL.concat( deviceID );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".version" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "device id: " );
  Serial.println( deviceID );
  Serial.print( "Firmware version URL: " );
  Serial.println( fwVersionURL );
  
  HTTPClient httpClient;
  httpClient.begin( fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    //riprendo la versione scritta nel file idDevice.version
    String newFWVersion = httpClient.getString(); 
    Serial.print( "Current firmware version: " );
    Serial.println( fwVersion );
    Serial.print( "Available firmware version: " );
    Serial.println( newFWVersion );

    int newVersion = newFWVersion.toInt();
   
    if( newVersion > fwVersion ) {
      Serial.println( "Preparing to update" );

      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );

      Serial.print( "cerco " );
      Serial.println(fwImageURL);
      delay(2000);
      
      t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

      switch(ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
        case HTTP_UPDATE_OK:
          Serial.println("[update] Update ok."); // may not called we reboot the ESP
          break;
      }
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  }
  httpClient.end();
}//check for updates