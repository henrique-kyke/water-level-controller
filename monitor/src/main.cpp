#include <U8g2lib.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "WifiSetup.h"

enum PumpStateEnum {ON, OFF};
void initDisplay();
void initWiFi();
void setupMqttClient();
void displayConnectionStatus(int delayInMillis);
void showResLevel(const char *resName, uint level, uint startY);
void displayShortMessage(String header, String message, long delayMilis);
void changePumpState();
void turnPump(PumpStateEnum state);
void printWiFiStatus();
void displayLevel(int delayInMillis);
void drawLevelChart(uint startX, uint startY, uint& relativeHeight);
int wifiStrengthLevel();
void callback(char* topic, byte* payload, unsigned int length);
void displayReceivedAt(int delayInMillis);

const char* ssid = "HAPS_EXT";
const char* password = "Ruby_2.5.0";
// const char* mqtt_server = "684ecefc61054643be107a7c6d184496.s1.eu.hivemq.cloud";
const char* mqtt_server = "v9503cae.ala.us-east-1.emqxsl.com";
const uint16_t mqtt_server_port = 8883; 
const char* mqttUser = "henrique.santana";
const char* mqttPassword = "045OTOjT%jPfTid5";
const char* mainProbeTopic = "water-level/main-probe";
const char* auxProbeTopic = "water-level/aux-probe";
const char* pumpTopic = "water-level/pump";
const char* res_1 = "Main:";
const char* res_2 = "Aux :";
const int utcOffsetInSeconds = -3*3600;

#define MAX_COLLUMN_NUMBER 5
#define COLLUMN_HEIGHT_STEP 3
#define COLUMN_WIDTH 15
#define MAX_HEIGHT (MAX_COLLUMN_NUMBER * COLLUMN_HEIGHT_STEP)

#define WIFI_MESSAGE_HEADER "Wifi connection"
#define MQTT_CALLBACK_HEADER "MQTT callback"
#define MQTT_BROCKER_HEADER "MQTT broker"
WifiSetup wsetup(ssid, password);
// ESP8266WiFiClass WiFi;
WiFiClientSecure espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

uint mainLevel = 0;
uint auxLevel = 0;
bool pumpShouldBeOn = false;
bool pumpIsOn = false;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
unsigned long interval = 30000;
String lastMainProbeCommunication = "";
String lastAuxProbeCommunication = "";
long currentLoopTime = 0;
PumpStateEnum currentPumpState = OFF;
/* force pump shutdown, to prevent overflow in main reservoir 
or pump damage on aux reservoir low level*/
PumpStateEnum lastPumpState = ON;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C

display(U8G2_R0, /*clock=*/12, /*data=*/14, U8X8_PIN_NONE);

void setup(void) {
  Serial.begin(115200);
  initDisplay();
  initWiFi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_server_port);
  client.setCallback(callback);
  timeClient.begin();
}

void loop(void) {
  display.clearBuffer();
  Serial.printf("Main level: %d\n", mainLevel);
  Serial.printf("Aux level: %d\n", auxLevel);
  if(wsetup.connected()){
      if(!client.connected()){ 
      setupMqttClient();
    } else {
      client.loop();
      displayLevel(1);
      changePumpState();
    }
  } else {
    initWiFi();
  }
  
  
}

void initDisplay() {
  display.begin();
  display.setFont(u8g2_font_helvR08_tf);
  display.clearBuffer();
}

void initWiFi() {
  if (wsetup.connected()) {
    displayConnectionStatus(2000);
  } else {
    displayShortMessage(WIFI_MESSAGE_HEADER, strcat("Connecting to WiFi to ", ssid), 2000);
    Serial.print("Connecting to WiFi to ");
    Serial.println(ssid);
    wsetup.begin();
    Serial.print("Unable to connect to ");
    Serial.println(wsetup.ssid());
    displayShortMessage(WIFI_MESSAGE_HEADER, strcat("Unable to connect to ", ssid) , 2000);
  }
}

void setupMqttClient() {
  
  const char *client_id = "monitor";
    
  if(client.connect(client_id, mqttUser, mqttPassword)){
    Serial.println("Conected to MQTT broker");
    client.subscribe(mainProbeTopic);
    client.subscribe(auxProbeTopic);
    displayShortMessage(MQTT_BROCKER_HEADER, "Connected to MQTT broker", 2000);
  } else {
    Serial.println(client.state());
    Serial.println("Not connected to MQTT broker. It will try again later.");
    displayShortMessage(MQTT_BROCKER_HEADER,"Not connected to MQTT broker. It will try again later.", 2000);
  }
}

bool mainLevelIsLow() {
  return mainLevel < 4;
}

bool mainLevelIsHigh() {
  return mainLevel == 5;
}

bool auxLevelCanSupply() {
  return auxLevel > 2;
}

bool hasRecentCommunication() {
  return true;
}

void changePumpState() {  
  if(mainLevelIsLow() && auxLevelCanSupply() && hasRecentCommunication()) {
    currentPumpState = ON;
  } else if (mainLevelIsHigh()|| !auxLevelCanSupply() || !hasRecentCommunication()) {
    currentPumpState = OFF;
  }
  if (currentPumpState != lastPumpState){
    turnPump(currentPumpState);
  }
  lastPumpState = currentPumpState;
}

void turnPump(PumpStateEnum state){
  JsonDocument doc;
  char buffer[256];
  doc["turnPump"] = state == ON ? "ON" : "OFF";
  size_t size = serializeJson(doc, buffer);
  client.publish(pumpTopic, buffer, true);
}

void printWiFiStatus(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >=interval){
    switch (WiFi.status()){
      case WL_NO_SSID_AVAIL:
        Serial.println("Configured SSID cannot be reached");
        break;
      case WL_CONNECTED:
        Serial.println("Connection successfully established");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("Connection failed");
        break;
    }
    Serial.printf("Connection status: %d\n", WiFi.status());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    previousMillis = currentMillis;
  }
}

void displayLevel(int delayInMillis){
  display.clearBuffer();
  display.drawStr(0, 12, "Controle de nivel");
  showResLevel(res_1, mainLevel, 35);
  showResLevel(res_2, auxLevel, 60);
  display.sendBuffer();
  delay(delayInMillis);
}


void displayConnectionStatus(int delayInMillis) {
  display.clearBuffer();
  display.drawStr(0, 12, WIFI_MESSAGE_HEADER);
  display.drawStr(0, 35, "IP : " );
  display.drawStr(20, 35, WiFi.localIP().toString().c_str());
  showResLevel("STR:", wifiStrengthLevel(), 55);
  display.sendBuffer();
  delay(delayInMillis);
}

void displayReceivedAt(int delayInMillis) {
  display.clearBuffer();
  display.drawStr(0, 12, "Message received at:");
  display.drawStr(0, 35, "Main: ");
  display.drawStr(30, 35, lastMainProbeCommunication.c_str());
  display.drawStr(0, 60, "Aux : ");
  display.drawStr(30, 60, lastAuxProbeCommunication.c_str());
  display.sendBuffer();
  delay(delayInMillis);
}
int wifiStrengthLevel() {
  int rssi = WiFi.RSSI();
  if(!rssi)
    return 0;
  if(rssi > -50)
    return 5;
  if(rssi > -65)
    return 4;
  if(rssi > -75)
    return 3;
  if(rssi > -85)
    return 2;
  return 1;
}

void showResLevel(const char *resName, uint level, uint startY) {
  display.drawStr(0, startY, resName);

  uint startX = 42;

  for (uint x = 1; x < level + 1; x++) {
    drawLevelChart(startX, startY - 14, x);
    startX += (COLUMN_WIDTH + 2);
  }
}

void showResReadingTime(const char *label, char *message, uint startY) {
  char s [100];
  sprintf(s, label, message);
  display.drawStr(0, startY, s);
}


void drawLevelChart(uint startX, uint startY, uint& relativeHeight) {
  uint height = relativeHeight * COLLUMN_HEIGHT_STEP;
  uint posY = startY + MAX_HEIGHT - height;
  display.drawBox(startX, posY, COLUMN_WIDTH, height);
}

void displayShortMessage(String header, String message, long delayMilis) {
  display.clearBuffer();
  display.drawStr(0, 12, header.c_str());
  display.drawStr(0, 48, message.c_str());
  display.sendBuffer();
  delay(delayMilis);
}

void callback(char *topic, byte *payload, unsigned int length) {
  timeClient.update();
  lastMainProbeCommunication = timeClient.getEpochTime();
  Serial.printf("Topic: %s\n", topic);
  String message = "";
  for (uint i = 0; i < length; i++){
    message += (char) payload[i];
  }
  JsonDocument doc; 
  deserializeJson(doc, message);
  String level = doc["level"];
  String level_msg = "Level: " + level;
  String messageSentAt = doc["timestamp"];
  Serial.println(message);
  Serial.println(level_msg);
  if (strcmp(topic, mainProbeTopic) == 0){
    Serial.println(mainProbeTopic);
    mainLevel = level.toInt();
    lastMainProbeCommunication = messageSentAt;
  } else if (strcmp(topic, auxProbeTopic) == 0){
    auxLevel = level.toInt();
    lastAuxProbeCommunication = messageSentAt;
  } else {
    Serial.println("Topic does not match");
  } 
}