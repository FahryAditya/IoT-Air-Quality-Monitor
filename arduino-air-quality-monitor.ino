/*
  arduino-air-quality-monitor.ino
  IoT Air Quality Monitor (ESP8266)
  - MQ-135 gas sensor on A0
  - DHT22 on D2 (GPIO4)
  - OLED SSD1306 I2C (SDA=D2/SD1, SCL=D1/SCL) -- on NodeMCU SDA=D2, SCL=D1
  - ESP8266 creates WiFi Access Point "AQMonitor_xxxx"
  - Web server: http://192.168.4.1/ shows live page and /data provides JSON
  Notes: MQ-135 needs warm-up and calibration; see README for calibration steps.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ======= CONFIG =======
#define DHTPIN D2           // D2 (GPIO4) on NodeMCU
#define DHTTYPE DHT22

#define MQ_PIN A0           // Analog pin for MQ-135
#define OLED_RESET -1       // Reset pin not used
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

const char *ap_ssid_prefix = "AQMonitor_";
const char *ap_password = "change_me01"; // optional (>=8 chars) or set to NULL for open AP
uint16_t ap_channel = 6;

// Sampling intervals
const unsigned long SENSOR_INTERVAL = 2000; // ms
const unsigned long OLED_UPDATE_INTERVAL = 1000;

// MQ-135 calibration constants (approximate; must be calibrated)
const float RLOAD = 10.0; // kilo-ohm load resistor used in divider (example 10k)
const float VREF = 3.3;   // ADC reference voltage (NodeMCU A0 maps 0-1V via divider on board -> value scaled already)
const float ADC_MAX = 1023.0;

// Basic mapping thresholds (very rough): lower is better air quality for MQ-135 raw -> map to AQI-like scale
// You should calibrate sensor in clean air to compute Ro and use gas curve for specific gases.
// This code uses relative values for demo.

// ======= OBJECTS =======
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);

// ======= STATE =======
unsigned long lastSensorMillis = 0;
unsigned long lastOLEDMillis = 0;

float lastTemp = NAN;
float lastHum  = NAN;
int lastMQRaw  = 0;
float lastGasIndex = 0.0;

// ======= FUNCTIONS =======
String ipToString(const IPAddress &ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void handleRoot();
void handleData();
void startAP();
float readMQ(); // returns raw ADC value (0-1023)
float computeGasIndex(int raw); // returns 0..500 AQI-like index (very rough)

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== Arduino Air Quality Monitor ===");

  // init sensors
  dht.begin();

  // OLED init
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 0x3C is common I2C addr
    Serial.println("SSD1306 allocation failed");
    // proceed without display
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("AQ Monitor booting...");
    display.display();
  }

  // Start WiFi AP
  startAP();

  // Setup web routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");

  // warm-up note
  Serial.println("Warm-up: allow MQ-135 to preheat (2-24 hours recommended depending on sensor)");
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorMillis >= SENSOR_INTERVAL) {
    lastSensorMillis = now;
    // read sensors
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int mq = (int)readMQ();

    // handle possible read errors
    if (!isnan(t)) lastTemp = t;
    if (!isnan(h)) lastHum = h;
    lastMQRaw = mq;
    lastGasIndex = computeGasIndex(mq);

    Serial.printf("Temp: %.2f C, Hum: %.2f %%, MQraw: %d, GasIdx: %.1f\n", lastTemp, lastHum, lastMQRaw, lastGasIndex);
  }

  if (now - lastOLEDMillis >= OLED_UPDATE_INTERVAL) {
    lastOLEDMillis = now;
    // update display if available
    if (display.width() > 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.printf("Temp: %.1f C\n", isnan(lastTemp) ? NAN : lastTemp);
      display.printf("Hum:  %.1f %%\n", isnan(lastHum) ? NAN : lastHum);
      display.printf("MQ:   %d\n", lastMQRaw);
      display.printf("AQI~: %.0f\n", lastGasIndex);
      display.display();
    }
  }
}

// ======== Web Server Handlers ========
void handleRoot() {
  // Simple dynamic HTML page with auto-refresh
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>AQ Monitor</title>";
  html += "<style>body{font-family:Arial,Helvetica,sans-serif;padding:12px} .card{padding:10px;border-radius:8px;background:#f2f2f2;margin-bottom:8px}</style>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "</head><body>";
  html += "<h2>IoT Air Quality Monitor</h2>";
  html += "<div class='card'><b>Temperature:</b> ";
  html += isnan(lastTemp) ? "N/A" : String(lastTemp,1) + " °C";
  html += "</div>";
  html += "<div class='card'><b>Humidity:</b> ";
  html += isnan(lastHum) ? "N/A" : String(lastHum,1) + " %";
  html += "</div>";
  html += "<div class='card'><b>MQ-135 raw:</b> " + String(lastMQRaw) + "</div>";
  html += "<div class='card'><b>Estimated AQI-like index:</b> " + String(lastGasIndex,0) + " (lower better)</div>";
  html += "<p><a href='/data'>Get JSON</a></p>";
  html += "<p>Connect your phone to WiFi: <b>" + String(ap_ssid_prefix) + WiFi.softAPIP().toString() + "</b></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"temperature\":" + (isnan(lastTemp) ? String("null") : String(lastTemp,2)) + ",";
  json += "\"humidity\":" + (isnan(lastHum) ? String("null") : String(lastHum,2)) + ",";
  json += "\"mq_raw\":" + String(lastMQRaw) + ",";
  json += "\"aq_index\":" + String(lastGasIndex,2) + ",";
  json += "\"time_ms\":" + String(millis());
  json += "}";
  server.send(200, "application/json", json);
}

// ======== Sensor helpers ========
float readMQ() {
  // On NodeMCU, A0 returns 0..1023 proportional to 0..1V input (board has divider)
  // MQ-135 raw ADC reading (0..1023)
  int raw = analogRead(MQ_PIN); // 0..1023
  return (float)raw;
}

float computeGasIndex(int raw) {
  // Very rough conversion to an AQI-like index 0..500
  // Calibrate baseline in clean air to determine Ro; here we'll make a relative mapping:
  // lower raw -> cleaner air (depending on wiring), but many MQ sensors increase resistance => lower raw
  // To be safe, create a mapping inversely so larger raw -> worse (user must calibrate)
  // We'll invert and scale:
  float normalized = raw / ADC_MAX; // 0..1
  float inverted = normalized; // keep same direction; user can invert if needed
  // map normalized (0..1) to index (0..300) roughly
  float idx = inverted * 300.0;
  // Clip
  if (idx < 0) idx = 0;
  if (idx > 500) idx = 500;
  return idx;
}

// ======== WiFi AP ========
void startAP() {
  // create a unique SSID using chip ID
  uint32_t id = ESP.getChipId();
  String ssid = String(ap_ssid_prefix) + String(id & 0xFFFF, HEX);
  Serial.printf("Starting AP: %s\n", ssid.c_str());
  WiFi.mode(WIFI_AP);
  // If password shorter than 8, AP will fail; allow open network by passing NULL
  if (strlen(ap_password) >= 8) {
    WiFi.softAP(ssid.c_str(), ap_password, ap_channel);
  } else {
    WiFi.softAP(ssid.c_str(), NULL, ap_channel);
  }
  IPAddress myIP = WiFi.softAPIP(); // default 192.168.4.1
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}
