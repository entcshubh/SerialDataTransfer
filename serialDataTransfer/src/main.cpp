#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define RELAY_PIN D1

// Wi-Fi credentials
const char* ssid = "shubh";
const char* password = "00000007";

// NTP setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // IST (+5:30)

// Command input buffer
String inputString = "";

// Relay and timer variables
bool relayState = false;
int onHour = -1, onMinute = -1;
int offHour = -1, offMinute = -1;

// Watchdog
unsigned long lastOkMillis = 0;
const unsigned long WATCHDOG_TIMEOUT = 300000; // 5 min

// Wi-Fi reconnect attempt
unsigned long lastWifiCheck = 0;

// ==== FUNCTION PROTOTYPES ====
void connectWiFi();
void processCommand(String cmd);
void parseTime(String timeStr, int &hour, int &minute);
void printTime(int h, int m);
void checkSchedule();

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println("System Booting...");
  Serial.println("Starting Wi-Fi...");
  WiFi.begin(ssid, password);

  connectWiFi();
}

void loop() {
  // Watchdog check
  if (millis() - lastOkMillis > WATCHDOG_TIMEOUT) {
    Serial.println("Watchdog Timeout! Restarting...");
    ESP.restart();
  }

  // Handle Wi-Fi reconnection
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiCheck >= 10000) {
      lastWifiCheck = millis();
      Serial.println("Wi-Fi Disconnected! Trying to reconnect...");
      connectWiFi();
    }
  }

  // If Wi-Fi is connected, update time & run schedule
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
    checkSchedule();
  }

  // Always allow manual commands from Serial
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) {
        processCommand(inputString);
        inputString = "";
      }
    } else {
      inputString += inChar;
    }
  }
}

void connectWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    timeClient.begin();
    timeClient.update();
    Serial.print("Current Time Synced: ");
    printTime(timeClient.getHours(), timeClient.getMinutes());
  } else {
    Serial.println("\nWi-Fi Connection Failed!");
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "on") {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = true;
    Serial.println("Relay manually turned ON");
  } 
  else if (cmd == "off") {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
    Serial.println("Relay manually turned OFF");
  } 
  else if (cmd.startsWith("on at")) {
    parseTime(cmd.substring(5), onHour, onMinute);
    Serial.print("Relay ON time set to: ");
    printTime(onHour, onMinute);
  } 
  else if (cmd.startsWith("off at")) {
    parseTime(cmd.substring(6), offHour, offMinute);
    Serial.print("Relay OFF time set to: ");
    printTime(offHour, offMinute);
  } 
  else if (cmd == "ok") {
    lastOkMillis = millis();
    Serial.println("OK received. Watchdog reset.");
  } 
  else {
    Serial.println("Invalid command");
  }
}

void parseTime(String timeStr, int &hour, int &minute) {
  timeStr.trim();
  timeStr.replace(".", ":");
  int colonIndex = timeStr.indexOf(':');
  if (colonIndex != -1) {
    hour = timeStr.substring(0, colonIndex).toInt();
    minute = timeStr.substring(colonIndex + 1).toInt();
  }
}

void printTime(int h, int m) {
  if (h < 10) Serial.print("0");
  Serial.print(h);
  Serial.print(":");
  if (m < 10) Serial.print("0");
  Serial.println(m);
}

void checkSchedule() {
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();

  if (currentHour == onHour && currentMinute == onMinute && !relayState) {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = true;
    Serial.println("Relay turned ON via schedule");
  }

  if (currentHour == offHour && currentMinute == offMinute && relayState) {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
    Serial.println("Relay turned OFF via schedule");
  }
}
