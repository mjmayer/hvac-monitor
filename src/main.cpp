// You can do conditional includes:
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "secrets.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is connected to GPIO2 (D4 on NodeMCU)
// Change this to your GPIO pin if different
#define ONE_WIRE_BUS 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  sensors.begin();
}

void loop()
{
  // Request temperature measurement
  sensors.requestTemperatures();

  // Fetch temperature in Celsius
  float tempC = sensors.getTempCByIndex(0);

  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.println(" °C");

  delay(2000); // Wait 2 seconds between reads
}
