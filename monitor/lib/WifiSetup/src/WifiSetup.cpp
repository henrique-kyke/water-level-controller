#include <Arduino.h>
#include "ESP8266WiFi.h"
#include "WifiSetup.h"

WifiSetup::WifiSetup(const char * ssid, const char * pass){
    _ssid = ssid;
    _pass = pass;
    Serial.println(ssid);
}   

WifiSetup::~WifiSetup(){
    _ssid = nullptr;
    _pass = nullptr;
}

const char * WifiSetup::ssid(){
    return _ssid;
}

bool WifiSetup::connected(){
    return wifi.status() == WL_CONNECTED;
}

void WifiSetup::begin(){
    wifi.begin(_ssid, _pass);
    wifi.setAutoConnect(true);
    wifi.setAutoReconnect(true);
    wifi.printDiag(Serial);
    delay(2000);
}



