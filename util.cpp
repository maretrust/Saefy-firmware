#include "Esp.h"
#include <EEPROM.h>
char del = ':';

int getIdDevice()
{
    return ESP.getFlashChipId();
}

void resetEsp()
{
    ESP.reset();
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
