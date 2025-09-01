#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ===== USER CONFIG (edit for live runs) =====
const char* ssid = "";           // set for live WiFi tests, leave empty to force simulated weather
const char* password = "";       // set for live WiFi tests

const char* OWM_API_KEY = "";    // OpenWeather API key (leave empty to force simulated weather)
const char* LATITUDE = "12.9716";
const char* LONGITUDE = "77.5946";
const char* OWM_URL_BASE = "http://api.openweathermap.org/data/2.5/onecall";

const float RAIN_THRESHOLD = 0.5; // 50% probability threshold
const unsigned long WEATHER_INTERVAL_MS = 30UL * 60UL * 1000UL; // refresh every 30 min

// pins
const int soilPin = 34;   // ADC1 channel (pot middle)
const int valvePin = 16;  // LED representing valve
const int ledPin = 2;     // safe indicator LED
const int buzzerPin = 17; // buzzer

// calibration defaults — update after you measure
int raw_dry = 3900;  // pot at "dry" extreme in Wokwi (adjust)
int raw_wet = 500;   // pot at "wet" extreme in Wokwi (adjust)

// smoothing
const int SMOOTH_WINDOW = 5;
int smoothBuf[SMOOTH_WINDOW];
int smoothIdx = 0;

// thresholds (percent)
const float START_PERCENT = 20.0; // start watering when <= 20%
const float STOP_PERCENT  = 80.0; // stop when >= 80%

// safety timers
const unsigned long MAX_WATER_MS = 4UL * 60UL * 60UL * 1000UL; // 4 hours
const unsigned long MIN_INTERVAL_MS = 30UL * 60UL * 1000UL; // 30 minutes

// state
bool rainDisabled = false;
bool watering = false;
unsigned long waterStartMs = 0;
unsigned long lastStopMs = 0;
unsigned long lastWeatherFetch = 0;

// fallback simulated JSON (hourly pop values for 24 hours)
const char* SIMULATED_ONECALL = R"EOF(
{
  "hourly": [
    {"pop":0.05},{"pop":0.10},{"pop":0.05},{"pop":0.02},
    {"pop":0.01},{"pop":0.00},{"pop":0.00},{"pop":0.00},
    {"pop":0.00},{"pop":0.00},{"pop":0.00},{"pop":0.00},
    {"pop":0.00},{"pop":0.00},{"pop":0.00},{"pop":0.00},
    {"pop":0.00},{"pop":0.00},{"pop":0.00},{"pop":0.02},
    {"pop":0.10},{"pop":0.30},{"pop":0.60},{"pop":0.40}
  ]
}
)EOF";

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (strlen(ssid) == 0) return; // user left empty -> don't try
  Serial.print("Connecting to WiFi ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    Serial.print('.');
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP:"); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed (using simulated weather).");
  }
}

bool fetchWeatherJSON(String &outJson) {
  // if no API key provided or no WiFi creds, use simulated JSON
  if (strlen(OWM_API_KEY) == 0 || strlen(ssid) == 0) {
    outJson = String(SIMULATED_ONECALL);
    Serial.println("Using simulated weather JSON (no API key or WiFi).");
    return true;
  }

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    outJson = String(SIMULATED_ONECALL);
    Serial.println("WiFi not connected -> using simulated weather JSON.");
    return true;
  }

  String url = String(OWM_URL_BASE) + "?lat=" + LATITUDE + "&lon=" + LONGITUDE + "&exclude=minutely,current&appid=" + OWM_API_KEY;
  Serial.print("Fetching: "); Serial.println(url);

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    outJson = http.getString();
    http.end();
    return true;
  } else {
    Serial.print("HTTP error: "); Serial.println(code);
    http.end();
  }

  outJson = String(SIMULATED_ONECALL);
  Serial.println("Fetch failed -> falling back to simulated JSON.");
  return true;
}

float parseMaxPopNext24(const String &json) {
  StaticJsonDocument<16*1024> doc; // 16 KB
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("JSON parse error: "); Serial.println(err.c_str());
    return 0.0;
  }
  float maxpop = 0.0;
  JsonArray hourly = doc["hourly"];
  int cnt = 0;
  for (JsonObject h : hourly) {
    if (cnt >= 24) break;
    float pop = 0.0;
    if (h.containsKey("pop")) pop = h["pop"].as<float>();
    if (pop > maxpop) maxpop = pop;
    cnt++;
  }
  return maxpop;
}

float readSmoothedPercent() {
  int raw = analogRead(soilPin); // 0..4095 on ESP32
  smoothBuf[smoothIdx] = raw;
  smoothIdx = (smoothIdx + 1) % SMOOTH_WINDOW;
  long sum = 0;
  for (int i=0;i<SMOOTH_WINDOW;i++) sum += smoothBuf[i];
  int avg = sum / SMOOTH_WINDOW;

  int r = avg;
  if (r > raw_dry) r = raw_dry;
  if (r < raw_wet) r = raw_wet;

  float pct = 0.0;
  if (raw_dry != raw_wet) {
    pct = (float)(raw_dry - r) / (float)(raw_dry - raw_wet) * 100.0;
  }
  if (pct < 0.0) pct = 0.0;
  if (pct > 100.0) pct = 100.0;
  return pct;
}

void refreshWeatherDecision() {
  String json;
  if (!fetchWeatherJSON(json)) {
    Serial.println("Weather fetch failed; using simulated JSON.");
    json = String(SIMULATED_ONECALL);
  }
  float maxpop = parseMaxPopNext24(json);
  Serial.print("Max precipitation probability next 24h: ");
  Serial.println(maxpop, 3);
  rainDisabled = (maxpop >= RAIN_THRESHOLD);
  if (rainDisabled) Serial.println("Decision: RAIN_YES -> watering disabled.");
  else Serial.println("Decision: RAIN_NO -> watering allowed.");
  lastWeatherFetch = millis();
}

void startWatering() {
  digitalWrite(valvePin, HIGH);
  digitalWrite(buzzerPin, HIGH);
  watering = true;
  waterStartMs = millis();
  Serial.println("WATERING START");
}

void stopWatering(const char* reason) {
  digitalWrite(valvePin, LOW);
  digitalWrite(buzzerPin, LOW);
  watering = false;
  lastStopMs = millis();
  Serial.print("WATERING STOP - ");
  Serial.println(reason);
}

void setup() {
  Serial.begin(115200);
  pinMode(valvePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(valvePin, LOW);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  // fill smoothing buffer with initial reading
  for (int i=0;i<SMOOTH_WINDOW;i++) smoothBuf[i] = analogRead(soilPin);

  Serial.println("Smart Irrigation (Wokwi-ready) starting...");
  refreshWeatherDecision();
}

void loop() {
  if (millis() - lastWeatherFetch > WEATHER_INTERVAL_MS) {
    refreshWeatherDecision();
  }

  float pct = readSmoothedPercent();
  Serial.print("Soil moisture % (smoothed): ");
  Serial.println(pct, 2);

  digitalWrite(ledPin, (pct >= STOP_PERCENT) ? HIGH : LOW);

  if (rainDisabled) {
    if (watering) stopWatering("rain_predicted");
  } else {
    if (!watering) {
      unsigned long sinceStop = millis() - lastStopMs;
      if (pct <= START_PERCENT && sinceStop >= MIN_INTERVAL_MS) {
        startWatering();
      }
    } else {
      if (pct >= STOP_PERCENT) {
        stopWatering("reached_stop_threshold");
      } else if ((millis() - waterStartMs) >= MAX_WATER_MS) {
        stopWatering("max_time_reached");
      }
    }
  }

  delay(1500);
}
