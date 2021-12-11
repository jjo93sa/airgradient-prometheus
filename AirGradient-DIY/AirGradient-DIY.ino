/**
 * This sketch connects an AirGradient DIY sensor to a WiFi network, and runs a
 * tiny HTTP server to serve air quality metrics to Prometheus.
 */

#include <AirGradient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <Wire.h>
#include "SSD1306Wire.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Adafruit_SGP30.h"


AirGradient ag = AirGradient();
Adafruit_BME280 bme; // I2C
Adafruit_SGP30 sgp;  // I2C

// Config ----------------------------------------------------------------------

// Optional.
const char* deviceId = "";

// Hardware options for AirGradient DIY sensor.
const bool hasPM = true;
const bool hasCO2 = true;
const bool hasSHT = false;
const bool hasBME = true;
const bool hasSGP = true;

// WiFi and IP connection info.
const char* ssid = "PleaswChangeMe";
const char* password = "PleaseChangeMe";
const int port = 9926;

// Uncomment the line below to configure a static IP address.
// #define staticip
#ifdef staticip
IPAddress static_ip(192, 168, 0, 0);
IPAddress gateway(192, 168, 0, 0);
IPAddress subnet(255, 255, 255, 0);
#endif

// The frequency of measurement updates.
const int updateFrequency = 5000;

// For housekeeping.
long lastUpdate;
int counter = 0;

// Config End ------------------------------------------------------------------

SSD1306Wire display(0x3c, SDA, SCL);
ESP8266WebServer server(port);

void setup() {
  Serial.begin(9600);

  // Init Display.
  display.init();
  display.flipScreenVertically();
  showTextRectangle("Init", String(ESP.getChipId(),HEX),true);

  // Enable enabled sensors.
  if (hasPM) ag.PMS_Init();
  if (hasCO2) ag.CO2_Init();
  if (hasSHT) ag.TMP_RH_Init(0x44);
  if (hasBME) bme.begin(0x76, &Wire);
  if (hasSGP) sgp.begin();

  // Set static IP address if configured.
  #ifdef staticip
  WiFi.config(static_ip,gateway,subnet);
  #endif

  // Set WiFi mode to client (without this it may try to act as an AP).
  WiFi.mode(WIFI_STA);
  // Setup and wait for WiFi.
  WiFi.begin(ssid, password);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    showTextRectangle("Trying to", "connect...", true);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  server.on("/", HandleRoot);
  server.on("/metrics", HandleRoot);
  server.onNotFound(HandleNotFound);

  server.begin();
  Serial.println("HTTP server started at ip " + WiFi.localIP().toString() + ":" + String(port));
  showTextRectangle("Listening To", WiFi.localIP().toString() + ":" + String(port),true);
}

void loop() {
  long t = millis();

  server.handleClient();
  updateScreen(t);
}

String GenerateMetrics() {
  String message = "";
  String idString = "{id=\"" + String(deviceId) + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";
  float temperature = 0.0;
  float humidity = 0.0;

  if (hasPM) {
    int stat = ag.getPM1_Raw();

    message += "# HELP pm01 Particulate Matter PM1.0, in micrograms per cubic metre\n";
    message += "# TYPE pm01 gauge\n";
    message += "pm01";
    message += idString;
    message += String(stat);
    message += "\n";

    stat = ag.getPM2_Raw();

    message += "# HELP pm02 Particulate Matter PM2.5, in micrograms per cubic metre\n";
    message += "# TYPE pm02 gauge\n";
    message += "pm02";
    message += idString;
    message += String(stat);
    message += "\n";

    stat = ag.getPM10_Raw();

    message += "# HELP pm10 Particulate Matter PM10, in micrograms per cubic metre\n";
    message += "# TYPE pm10 gauge\n";
    message += "pm10";
    message += idString;
    message += String(stat);
    message += "\n";
  }

  if (hasCO2) {
    int stat = ag.getCO2_Raw();

    message += "# HELP rco2 CO2 value, in ppm\n";
    message += "# TYPE rco2 gauge\n";
    message += "rco2";
    message += idString;
    message += String(stat);
    message += "\n";
  }

  if (hasSHT) {
    TMP_RH stat = ag.periodicFetchData();

    message += "# HELP atmp Temperature, in degrees Celsius\n";
    message += "# TYPE atmp gauge\n";
    message += "atmp";
    message += idString;
    message += String(stat.t);
    message += "\n";

    message += "# HELP rhum Relative humidity, in percent\n";
    message += "# TYPE rhum gauge\n";
    message += "rhum";
    message += idString;
    message += String(stat.rh);
    message += "\n";

    temperature = stat.t;
    humidity = stat.rh;
  }

  if (hasBME) {
    temperature = bme.readTemperature();
    message += "# HELP atmp Temperature, in degrees Celsius\n";
    message += "# TYPE atmp gauge\n";
    message += "atmp";
    message += idString;
    message += String(temperature);
    message += "\n";

    message += "# HELP pres Pressure, in hPa\n";
    message += "# TYPE pres gauge\n";
    message += "pres";
    message += idString;
    message += String(bme.readPressure() / 100.0F);
    message += "\n";

    humidity = bme.readHumidity();
    message += "# HELP rhum Relative humidity, in percent\n";
    message += "# TYPE rhum gauge\n";
    message += "rhum";
    message += idString;
    message += String(humidity);
    message += "\n";
  }

  if (hasSGP) {
    sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));

    if ( sgp.IAQmeasure()) {
      message += "# HELP tvoc total volatile organic compounds, in ppb\n";
      message += "# TYPE tvoc gauge\n";
      message += "tvoc";
      message += idString;
      message += String(sgp.TVOC);
      message += "\n";

      message += "# HELP eco2 equivalent CO2 value, in ppm\n";
      message += "# TYPE eco2 gauge\n";
      message += "eco2";
      message += idString;
      message += String(sgp.eCO2);
      message += "\n";
    }
  }


  if (WiFi.status() == WL_CONNECTED) {
    char wifi_dbm_reading[12];
    ltoa(WiFi.RSSI(), wifi_dbm_reading, 10);

    message += "# HELP RSSI Level, in decibel-milliwatts\n";
    message += "# TYPE RSSI gauge\n";
    message += "rssi";
    message += idString;
    message += String(wifi_dbm_reading);
    message += "\n";
  }

  return message;
}

void HandleRoot() {
  server.send(200, "text/plain", GenerateMetrics() );
}

void HandleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}

// DISPLAY
void showTextRectangle(String ln1, String ln2, boolean small) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (small) {
    display.setFont(ArialMT_Plain_16);
  } else {
    display.setFont(ArialMT_Plain_24);
  }
  display.drawString(32, 16, ln1);
  display.drawString(32, 36, ln2);
  display.display();
}

void updateScreen(long now) {
  if ((now - lastUpdate) > updateFrequency) {
    // Take a measurement at a fixed interval.
    switch (counter) {
      case 0:
        if (hasPM) {
          int stat = ag.getPM1_Raw();
          showTextRectangle("PM1",String(stat),false);
        }
        break;
      case 1:
        if (hasPM) {
          int stat = ag.getPM2_Raw();
          showTextRectangle("PM2",String(stat),false);
        }
        break;
      case 2:
        if (hasPM) {
          int stat = ag.getPM10_Raw();
          showTextRectangle("PM10",String(stat),false);
        }
        break;
      case 3:
        if (hasCO2) {
          int stat = ag.getCO2_Raw();
          showTextRectangle("CO2", String(stat), false);
        }
        break;
      case 4:
        if (hasSHT) {
          TMP_RH stat = ag.periodicFetchData();
          showTextRectangle("TMP", String(stat.t, 1) + "C", false);
        }
        break;
      case 5:
        if (hasSHT) {
          TMP_RH stat = ag.periodicFetchData();
          showTextRectangle("HUM", String(stat.rh) + "%", false);
        }
        break;
      case 6:
        if (WiFi.status() == WL_CONNECTED) {
          char wifi_dbm_reading[12];
          ltoa(WiFi.RSSI(), wifi_dbm_reading, 10);
          showTextRectangle("RSSI", String(wifi_dbm_reading) + "dBm", true);
        }
        break;
    }
    counter++;
    if (counter > 6) counter = 0;
    lastUpdate = millis();
  }
}

/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}
