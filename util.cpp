#include "Esp.h"

int getIdDevice()
{
    return ESP.getFlashChipId();
}