#include <Arduino.h>
#include "led.h"


const int LED=13;
const int LED_RELE=12;
const int BUTTON=1;
const int LED_BLINK_INTERVAL=1000;

void blinkBLUE(){
    digitalWrite(LED, true);   
    delay(1000);                       
    digitalWrite(LED, false);
            
}

void blinkRED(){
    pinMode(LED_RELE, OUTPUT);
    digitalWrite(LED_RELE, true);   
    delay(LED_BLINK_INTERVAL);                       
    digitalWrite(LED_RELE, false);
}

void blinkON(){
    Serial.println("Leo on");
    digitalWrite(LED, true);  
}

void blinkOFF(){
    pinMode(LED, OUTPUT);
    digitalWrite(LED, false);  
}