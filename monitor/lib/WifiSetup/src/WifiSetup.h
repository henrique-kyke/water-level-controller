#ifndef WifiSetup_H
#define WifiSetup_H
#include "Arduino.h"
#include "ESP8266WiFi.h"


class WifiSetup
{
    private:
        const char * _ssid;
        const char * _pass;
        ESP8266WiFiClass wifi;
    
    public:
        WifiSetup(const char* ssid, const char* pass);
        ~WifiSetup();
        const char* ssid();
        bool connected();
        void begin();
};




#endif  //