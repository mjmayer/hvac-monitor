#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include "esp_task_wdt.h"
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include "secrets.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// Uncomment the following line to enable debug logging
// #define DEBUG_LOGGING

// Uncomment the following line to enable WiFi functionality
#define WIFI_ENABLED

// Data wire is connected to GPIO2 (D4 on NodeMCU)
// Change this to your GPIO pin if different
#define ONE_WIRE_BUS 4

// FAN
#define HVAC_PIN18 18
// 4-way valve
#define HVAC_PIN21 21

// Watchdog timeout in seconds
#define WATCHDOG_TIMEOUT 15

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#ifdef WIFI_ENABLED
// Initialize web server on port 80
#ifdef ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif
#endif
// Define the addresses of the sensors
DeviceAddress sensorBeforeCoil = {0x28, 0x58, 0x34, 0x50, 0x00, 0x00, 0x00, 0xD2};
DeviceAddress sensorAfterCoil = {0x28, 0x32, 0x5A, 0x51, 0x00, 0x00, 0x00, 0xE2};
DeviceAddress sensorAttic = {0x28, 0x0D, 0x70, 0x54, 0x00, 0x00, 0x00, 0xEF};

unsigned long lastSampleTime = 0;
const unsigned long sampleInterval = 5000; // 5 seconds

// Cached temperature values from sensors.
// These values are updated every 5 seconds based on the sampleInterval.
float tempBeforeC = NAN, tempAfterC = NAN, tempAtticC = NAN;
// Cached digital input states representing whether the fan and reversing valve are energized.
// These states are updated every 5 seconds based on the optocoupler input sampling logic.
bool fanEnergized = false, reversingValveEnergized = false;
unsigned long startTime;
static unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000;  // 10 seconds
const int RECONNECT_DELAY_MS = 500;             // Delay between reconnect attempts in milliseconds
const int MAX_RECONNECT_ATTEMPTS = 20;          // Maximum number of reconnect attempts
const unsigned long wifiConnectTimeout = 15000; // 15 seconds timeout

void handleHealth()
{
  unsigned long uptimeSec = (millis() - startTime) / 1000;
  server.send(200, "text/plain", "OK\nUptime: " + String(uptimeSec) + "s");
}

// Function to sample optocoupler input and determine if it's active
bool isSignalEnergized(int pin, const char *name = nullptr)
{
  const int sampleCount = 20;
  const int minActiveCount = 5; // Lower threshold for testing
  const int sampleDelayMs = 5;

  int lowCount = 0;

  for (int i = 0; i < sampleCount; i++)
  {
    int state = digitalRead(pin);
    if (state == LOW)
    {
      lowCount++;
    }
    delay(sampleDelayMs);
  }

#ifdef DEBUG_LOGGING
  if (name)
  {
    Serial.print(name);
    Serial.print(" - LOW samples: ");
  }
  else
  {
    Serial.print("Pin ");
    Serial.print(pin);
    Serial.print(" - LOW samples: ");
  }
  Serial.println(lowCount);
#endif

  return (lowCount >= minActiveCount);
}

void sampleHVAC()
{
  sensors.requestTemperatures();
  tempBeforeC = sensors.getTempC(sensorBeforeCoil);
  tempAfterC = sensors.getTempC(sensorAfterCoil);
  tempAtticC = sensors.getTempC(sensorAttic);

  fanEnergized = isSignalEnergized(HVAC_PIN18);
  reversingValveEnergized = isSignalEnergized(HVAC_PIN21);
}

void handleMetrics()
{
  float tempBeforeF = tempBeforeC * 9.0 / 5.0 + 32.0;
  float tempAfterF = tempAfterC * 9.0 / 5.0 + 32.0;
  float tempAtticF = tempAtticC * 9.0 / 5.0 + 32.0;

  String response;
  response += "hvac_temperature_celsius{location=\"before_coil\"} " + String(tempBeforeC) + "\n";
  response += "hvac_temperature_celsius{location=\"after_coil\"} " + String(tempAfterC) + "\n";
  response += "hvac_temperature_celsius{location=\"attic\"} " + String(tempAtticC) + "\n";
  response += "hvac_temperature_fahrenheit{location=\"before_coil\"} " + String(tempBeforeF) + "\n";
  response += "hvac_temperature_fahrenheit{location=\"after_coil\"} " + String(tempAfterF) + "\n";
  response += "hvac_temperature_fahrenheit{location=\"attic\"} " + String(tempAtticF) + "\n";
  response += "hvac_signal_state{signal=\"compressor\"} " + String(fanEnergized ? "1" : "0") + "\n";
  response += "hvac_signal_state{signal=\"reversing_valve\"} " + String(reversingValveEnergized ? "1" : "0") + "\n";

#ifdef WIFI_ENABLED
  server.send(200, "text/plain", response);
#endif
}

void setup()
{
#ifdef DEBUG_LOGGING
  Serial.begin(115200);
  delay(1000);
#endif

#ifdef WIFI_ENABLED
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long wifiConnectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < wifiConnectTimeout)
  {
    delay(100);
    yield(); // Allow other tasks to run
#ifdef DEBUG_LOGGING
    Serial.print(".");
#endif
  }
  if (WiFi.status() != WL_CONNECTED)
  {
#ifdef DEBUG_LOGGING
    Serial.println("\nWiFi connection timed out.");
#endif
    // Optionally handle connection failure here (e.g., restart, continue without WiFi, etc.)
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
#ifdef WIFI_ENABLED
  server.on("/metrics", handleMetrics);
  server.begin();
  Serial.println("HTTP server started.");
#endif
  pinMode(HVAC_PIN18, INPUT);
  pinMode(HVAC_PIN21, INPUT);
  startTime = millis();
  server.on("/health", handleHealth);
  // Timeout after WATCHDOG_TIMEOUT seconds if not reset
#ifdef ESP32
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
  esp_task_wdt_add(NULL);
#endif
}

void loop()
{
#ifdef WIFI_ENABLED

  // Check WiFi connection and reconnect if disconnected
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt >= reconnectInterval)
  {
    lastReconnectAttempt = millis();
#ifdef DEBUG_LOGGING
    Serial.println("WiFi disconnected. Attempting to reconnect...");
#endif
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_RECONNECT_ATTEMPTS)
    {
      delay(RECONNECT_DELAY_MS);
      attempts++;
#ifdef DEBUG_LOGGING
      Serial.print(".");
#endif
      yield(); // Allow other tasks to run
    }

    if (WiFi.status() == WL_CONNECTED)
    {
#ifdef DEBUG_LOGGING
      Serial.println("\nWiFi reconnected successfully.");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP().toString());
#endif
    }
    else
    {
#ifdef DEBUG_LOGGING
      Serial.println("\nFailed to reconnect to WiFi. Will try again in next loop.");
#endif
    }
  }

  server.handleClient();
#endif
  if (millis() - lastSampleTime > sampleInterval)
  {
    sampleHVAC();
    lastSampleTime = millis();
  }
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

#ifdef DEBUG_LOGGING
  if (isSignalEnergized(HVAC_PIN18, "HVAC_PIN18"))
  {
    Serial.println("Compressor is ENERGIZED (Fan ON)");
  }
  else
  {
    Serial.println("Compressor is NOT ENERGIZED (Fan OFF)");
  }

  if (isSignalEnergized(HVAC_PIN21, "HVAC_PIN21"))
  {
    Serial.println("4-way valve is ENERGIZED (heating mode)");
  }
  else
  {
    Serial.println("4-way valve is NOT ENERGIZED (cooling mode)");
  }

  delay(500);
#endif
#ifdef ESP32
  esp_task_wdt_reset();
#endif
}
