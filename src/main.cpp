// You can do conditional includes:
#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include "secrets.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// Uncomment the following line to enable debug logging
#define DEBUG_LOGGING

// Data wire is connected to GPIO2 (D4 on NodeMCU)
// Change this to your GPIO pin if different
#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Initialize web server on port 80
#ifdef ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

// Define the addresses of the sensors
DeviceAddress sensorBeforeCoil = {0x28, 0x58, 0x34, 0x50, 0x00, 0x00, 0x00, 0xD2};
DeviceAddress sensorAfterCoil = {0x28, 0x32, 0x5A, 0x51, 0x00, 0x00, 0x00, 0xE2};
DeviceAddress sensorAttic = {0x28, 0x0D, 0x70, 0x54, 0x00, 0x00, 0x00, 0xEF};

void handleMetrics()
{
  sensors.requestTemperatures();

  float tempBefore = sensors.getTempC(sensorBeforeCoil);
  float tempAfter = sensors.getTempC(sensorAfterCoil);
  float tempAttic = sensors.getTempC(sensorAttic);

  String response;
  response += "hvac_temperature_celsius{location=\"before_coil\"} ";
  response += String(tempBefore);
  response += "\n";

  response += "hvac_temperature_celsius{location=\"after_coil\"} ";
  response += String(tempAfter);
  response += "\n";

  response += "hvac_temperature_celsius{location=\"attic\"} ";
  response += String(tempAttic);
  response += "\n";

  server.send(200, "text/plain", response);
}

void setup()
{
#ifdef DEBUG_LOGGING
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    yield(); // Allow other tasks to run
#ifdef DEBUG_LOGGING
    Serial.print(".");
#endif
  }

  while (WiFi.localIP() == IPAddress(0, 0, 0, 0))
  {
    delay(100);
  }
#ifdef DEBUG_LOGGING
  Serial.println("");
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP().toString());
#endif

  sensors.begin();
#ifdef DEBUG_LOGGING
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount());
  Serial.println(" DS18B20 sensor(s).");

  DeviceAddress addr;
  for (uint8_t i = 0; i < sensors.getDeviceCount(); i++)
  {
    if (sensors.getAddress(addr, i))
    {
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.print(" Address: ");
      for (uint8_t j = 0; j < 8; j++)
      {
        if (addr[j] < 16)
          Serial.print("0");
        Serial.print(addr[j], HEX);
      }
      Serial.println();
    }
  }
#endif
  server.on("/metrics", handleMetrics);
  server.begin();
#ifdef DEBUG_LOGGING
  Serial.println("HTTP server started.");
#endif
}

void loop()
{
  server.handleClient();
#ifdef DEBUG_LOGGING
  // Optionally print readings to Serial Monitor every 10 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000)
  {
    // Ask all sensors to take a temperature reading
    // This is required when calling getTempC() or getTempF()
    sensors.requestTemperatures();

    float tempBefore = sensors.getTempC(sensorBeforeCoil);
    float tempAfter = sensors.getTempC(sensorAfterCoil);
    float tempAttic = sensors.getTempC(sensorAttic);

    Serial.print("Before Coil: ");
    Serial.print(tempBefore);
    Serial.println(" °C");

    Serial.print("After Coil: ");
    Serial.print(tempAfter);
    Serial.println(" °C");

    Serial.print("Attic: ");
    Serial.print(tempAttic);
    Serial.println(" °C");

    lastPrint = millis();
  }
#endif
}
