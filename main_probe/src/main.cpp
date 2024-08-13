#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const uint8_t level_1 = D4;
const uint8_t level_2 = D5;
const uint8_t level_3 = D6;
const uint8_t level_4 = D7;

const char* ssid = "HAPS_EXT";
const char* password = "Ruby_2.5.0";
// const char* mqtt_server = "684ecefc61054643be107a7c6d184496.s1.eu.hivemq.cloud";
const char* mqtt_server = "v9503cae.ala.us-east-1.emqxsl.com";
const uint16_t mqtt_server_port = 8883; 
const char* mqttUser = "henrique.santana";
const char* mqttPassword = "045OTOjT%jPfTid5";
const char* mainProbeTopic = "water-level/main-probe";

// put function declarations here:
int lastSentLevel = 0;
int countDifferentLevel = 0; 
int level = 0;
int l1;
int l2;
int l3;
int l4;

const int utcOffsetInSeconds = -3*3600;

int readLevel();
void printLevel(int level);
void initWiFi();
void setupMqttClient();
void printConnectionState();
void sendStatus();
String formatDateTime();

ESP8266WiFiClass WiFi;
WiFiClientSecure espClient;
PubSubClient client(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
  Serial.begin(115200);
  pinMode(level_1, INPUT);
  pinMode(level_2, INPUT);
  pinMode(level_3, INPUT);
  pinMode(level_4, INPUT);

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_server_port);
  WiFi.mode(WIFI_STA);
  initWiFi();  
  timeClient.begin();
}

void loop() {
  printLevel(readLevel());
  if (WiFi.status() == WL_CONNECTED) {
    printConnectionState();
    if(client.connected()) {
      sendStatus();
    } else {
      setupMqttClient();
    } 
  } else {
    initWiFi();
  }  
  delay(5000);
}

int readLevel(){
  level = 0;

  l1 = digitalRead(level_1);
  l2 = digitalRead(level_2);
  l3 = digitalRead(level_3);
  l4 = digitalRead(level_4);

  if (l4 == 1){
    level = 4;
  } else if (l3 == 1){
    level = 3;
  } else if (l2 == 1){
    level = 2;
  } else if (l1 == 1){
    level = 1;
  }
  return level;
}

void printLevel(int level){
  if(level != lastSentLevel){
    countDifferentLevel += 1;
  } else {
    countDifferentLevel = 0;
  }

  if(countDifferentLevel > 4 || lastSentLevel == 0){
    lastSentLevel = level;
  }  
  Serial.print("Level: ");  
  Serial.println(lastSentLevel);
  
}

void initWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    printConnectionState();
  } else {
    Serial.print("Connecting to WiFi to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    delay(2000);
    Serial.print("Unable to connect to ");
    Serial.println(ssid);
  }
}

void setupMqttClient() {
  
  const char *client_id = "main-probe";
    
  if(client.connect(client_id, mqttUser, mqttPassword)){
    Serial.println(client.state());
    Serial.println("Conected to MQTT broker");
  } else {
    Serial.println(client.state());
    Serial.println("Not conected to MQTT broker. It will try again later.");
  }
}

void printConnectionState() {
    Serial.print("Connected to: "); 
    Serial.println(ssid); 
    Serial.print("IP: "); 
    Serial.println(WiFi.localIP()); 
}

void sendStatus() {

  JsonDocument doc;
  String buffer;
  doc["sensor_1"] = l1;
  doc["sensor_2"] = l2;
  doc["sensor_3"] = l3;
  doc["sensor_4"] = l4;
  doc["level"] = level;
  doc["timestamp"] = formatDateTime();
  serializeJson(doc, buffer);
  Serial.print("Message to be sent:");
  Serial.println(buffer);
  if(client.publish(mainProbeTopic, buffer.c_str(), true)){
    Serial.println("Message was successfully sent.");
  } else {
    Serial.println("An error occurs during message sent.");
  }
}

String formatDateTime(){
  timeClient.update();
  time_t epoch = timeClient.getEpochTime();
  String time = timeClient.getFormattedTime();
  struct tm *ptm = gmtime ((time_t *)&epoch);
  int year = ptm->tm_year+1900;
  int month = ptm->tm_mon+1;
  int day = ptm->tm_mday;
  String currentDate = String(year) + "-" + String(month) + "-"+ String(day) +" "+time;

  return currentDate;
}
  