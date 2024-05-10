#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <time.h>
#include <TZ.h>

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

const char* ssid = "HAPS";
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


#define MAX_COLLUMN_NUMBER 5
#define COLLUMN_HEIGHT_STEP 3
#define COLUMN_WIDTH 15
#define MAX_HEIGHT (MAX_COLLUMN_NUMBER * COLLUMN_HEIGHT_STEP)

#define WIFI_CONNECTION_HEADER "Wifi connection"
#define MQTT_CALLBACK_HEADER "MQTT callback"
#define MQTT_BROCKER_HEADER "MQTT broker"

ESP8266WiFiClass WiFi;
WiFiClientSecure espClient;
PubSubClient client(espClient);

uint mainLevel = 0;
uint auxLevel = 0;
bool pumpShouldBeOn = false;
bool pumpIsOn = false;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
unsigned long interval = 30000;
PumpStateEnum currentPumpState = OFF;
/* force pump shutdown, to prevent overflow in main reservoir 
or pump damage on aux reservoir low level*/
PumpStateEnum lastPumpState = ON;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C

display(U8G2_R0, /*clock=*/12, /*data=*/14, U8X8_PIN_NONE);

void setup(void) {
  Serial.begin(9600);
  initDisplay();
  initWiFi();
  setupMqttClient();
}

void loop(void) {
  printWiFiStatus();
  if (WiFi.status() != WL_CONNECTED){
    initWiFi();
    delay(3000);
  }
  Serial.print("MQTT Client is connected: ");
  Serial.println(client.connected());
  if(client.connected()){
    displayConnectionStatus(2000);
    display.clearBuffer();
    Serial.printf("Main level: %d\n", mainLevel);
    Serial.printf("Aux level: %d\n", auxLevel);
    displayLevel(5000);
    changePumpState();
    // displayLastReading();
    client.loop();
  } else {
    setupMqttClient();
  }
}

void initDisplay() {
  display.begin();
  display.setFont(u8g2_font_helvR08_tf);
  display.clearBuffer();
}

void initWiFi() {
  displayShortMessage(WIFI_CONNECTION_HEADER, "Disconnected", 2000);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("MWC");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    displayShortMessage(WIFI_CONNECTION_HEADER, "Trying to connect to " + WiFi.SSID(), 5000);
  }
  Serial.println(WiFi.localIP());
  //The ESP8266 tries to reconnect automatically when the connection is lost
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  displayShortMessage(WIFI_CONNECTION_HEADER, "IP: " + WiFi.localIP().toString(), 1000);
}

void setupMqttClient() {
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_server_port);
  client.setCallback(callback);
  
  String client_id = "main-";
  client_id += String(WiFi.macAddress());
  
  while (!client.connected()) {
    if(client.connect(client_id.c_str(), mqttUser, mqttPassword)){
      displayShortMessage(MQTT_BROCKER_HEADER, "Connecting to brocker", 5000);
    } else {
      displayShortMessage(MQTT_BROCKER_HEADER, "Fail to connect", 2000);
      displayShortMessage(MQTT_BROCKER_HEADER, "Retrying...", 2000);
      Serial.println(client.state());
    }
  }
  client.subscribe(mainProbeTopic);
  client.subscribe(auxProbeTopic);
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
  client.publish(pumpTopic, buffer, size);
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
  display.drawStr(0, 12, WIFI_CONNECTION_HEADER);
  display.drawStr(0, 35, "IP : " );
  display.drawStr(20, 35, WiFi.localIP().toString().c_str());
  showResLevel("STR:", wifiStrengthLevel(), 55);
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

void displayLastReading(){
  Serial.println("[displayLastReadin] begin");
  display.clearBuffer();
  display.drawStr(0, 12, "Controle de nivel");
  // char* mainlr_message =  lastMainReadTime != 0L ? lastMainReadTime : "Waiting reading";
  // char* auxlr_message = lastAuxReadTime != 0L ? lastAuxReadTime : "Waiting reading";
  // showResReadingTime(res_1, mainlr_message, 35);
  // showResReadingTime(res_2, auxlr_message, 60);
  
  display.sendBuffer();
  delay(2000);
  Serial.println("[displayLastReadin] end");
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
  Serial.printf("Topic: %s\n", topic);
  String message = "";
  for (uint i = 0; i < length; i++){
    message += (char) payload[i];
  }
  JsonDocument doc; 
  deserializeJson(doc, message);
  String level = doc["level"];
  String level_msg = "Level: " + level;
  if (strcmp(topic, mainProbeTopic) == 0){
    Serial.println(mainProbeTopic);
    mainLevel = level.toInt();
  } else if (strcmp(topic, auxProbeTopic) == 0){
    auxLevel = level.toInt();
  } else {
    Serial.println("Topic does not match");
  } 
}