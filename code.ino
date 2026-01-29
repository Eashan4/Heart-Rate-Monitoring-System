#include <ESP32.h>
#include <ESP32.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include "DHT.h"

#define DHTTYPE DHT22
#define DHTPIN 14  // D5 = GPIO14
#define REPORTING_PERIOD_MS 1000

/* WiFi Credentials */
const char* ssid = "ABCDEF";
const char* password = "23456789";

DHT dht(DHTPIN, DHTTYPE);
PulseOximeter pox;
uint32_t tsLastReport = 0;
ESP8266WebServer server(80);

// Sensor Variables
float temperature = 0.0, humidity = 0.0, BPM = 0.0, SpO2 = 0.0;
bool validBPM = false, validSpO2 = false;

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect. Restarting...");
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("Initializing Sensors..."));
  dht.begin();
  connectWiFi();

  Serial.print("Initializing Pulse Oximeter... ");
  if (!pox.begin()) {
    Serial.println("FAILED!");
    while (true);
  }
  Serial.println("SUCCESS");
  pox.setIRLedCurrent(MAX30100_LED_CURR_50MA);

  server.on("/", handle_OnConnect);
  server.on("/data", sendSensorData);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP Server started.");
}

void loop() {
  server.handleClient();
  pox.update();

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    float currentBPM = pox.getHeartRate();
    float currentSpO2 = pox.getSpO2();

    // Heuristic check for finger presence using reasonable BPM/SpO2 ranges
    if (currentBPM > 40 && currentBPM < 180) {
      BPM = currentBPM;
      validBPM = true;
    } else {
      BPM = 0.0;
      validBPM = false;
    }

    if (currentSpO2 > 70 && currentSpO2 <= 100) {
      SpO2 = currentSpO2;
      validSpO2 = true;
    } else {
      SpO2 = 0.0;
      validSpO2 = false;
    }

    Serial.printf("Temp: %.2f°C | Humidity: %.2f%% | BPM: %s | SpO2: %s\n",
                  temperature, humidity,
                  validBPM ? String(BPM, 2).c_str() : "--",
                  validSpO2 ? String(SpO2, 2).c_str() : "--");

    tsLastReport = millis();
  }
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML());
}

void sendSensorData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"BPM\":" + (validBPM ? String(BPM, 2) : "\"--\"") + ",";
  json += "\"SpO2\":" + (validSpO2 ? String(SpO2, 2) : "\"--\"");
  json += "}";
  server.send(200, "application/json", json);
}

void handle_NotFound() {
  server.send(404, "text/plain", "404 - Not Found");
}