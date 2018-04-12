#include "Esp.h"
#include <EEPROM.h>
#include "string.h"
char del = ':';

String getIdDevice()
{
    //return String(ESP.getChipId(), HEX);
    uint32_t chipid=ESP.getChipId();
    char clientid[25];
    snprintf(clientid,25,"%08X",chipid);
    String id =  String(clientid);
    int number = (int) strtol( id.c_str(), NULL, 16);
    return String(number);
}

void resetEsp()
{
    ESP.reset();
    ESP.restart();
}

String getCmd(String msg){
    int posDel = msg.lastIndexOf(':');
    int initDel = msg.indexOf(':');
    String cmd = msg.substring(initDel+1, posDel);
    return cmd;

}

String getValue(String msg){
    int posDel = msg.lastIndexOf(':');
    String value = msg.substring(posDel+1,msg.length());
    return value;
}


String getIdDeviceMsg(String msg){
    int posDel = msg.indexOf(':');
    String id = msg.substring(0, posDel);
    return id;
}
