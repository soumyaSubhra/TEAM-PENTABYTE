const int sensorPin = A0;
const int relayPin  = 8;   
const int ledSafe   = 6;   
const int buzzerPin = 7;   
int raw_dry = 900;  
int raw_wet = 200;   

const float START_PERCENT = 20.0;  
const float STOP_PERCENT  = 80.0;  


const unsigned long MAX_WATER_MS = 10UL * 60UL * 1000UL; // 10 minutes max (demo)
const unsigned long MIN_INTERVAL_MS = 10UL * 1000UL;    // 30 seconds between cycles

// state
bool rainDisabled = false;
bool forceOn = false;
bool forceOff = false;
bool watering = false;
unsigned long waterStartTime = 0;
unsigned long lastStopTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(relayPin, OUTPUT);
  pinMode(ledSafe, OUTPUT);
  pinMode(buzzerPin, OUTPUT); 
  digitalWrite(relayPin, LOW);
  digitalWrite(ledSafe, LOW);
  noTone(buzzerPin);
  Serial.println("Irrigation WITH ARD UNO (Tinkercad) with piezo started.");
  Serial.println("Commands: RAIN_YES / RAIN_NO / FORCE_ON / FORCE_OFF / CAL:dry:wet");
  //Serial.println("Use potentiometer to simulate moisture (dry -> high raw, wet -> low raw).");
}

float readMoisturePercent() {
  int raw = analogRead(sensorPin); // 0..1023
  // Constrain between wet and dry calibration
  if (raw > raw_dry) raw = raw_dry;
  if (raw < raw_wet) raw = raw_wet;
  float pct = 0.0;
  if (raw_dry != raw_wet) {
    pct = (float)(raw_dry - raw) / (float)(raw_dry - raw_wet) * 100.0;
  }
  if (pct < 0.0) pct = 0.0;
  if (pct > 100.0) pct = 100.0;
  return pct;
}

void handleSerial() {
  while (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() == 0) continue;
    if (s.equalsIgnoreCase("RAIN_YES")) {
      rainDisabled = true;
      Serial.println("Gateway -> RAIN_YES (watering disabled)");
    } else if (s.equalsIgnoreCase("RAIN_NO")) {
      rainDisabled = false;
      Serial.println("Gateway -> RAIN_NO (watering allowed)");
    } else if (s.equalsIgnoreCase("FORCE_ON")) {
      forceOn = true; forceOff = false;
      Serial.println("Manual override: FORCE_ON");
    } else if (s.equalsIgnoreCase("FORCE_OFF")) {
      forceOff = true; forceOn = false;
      Serial.println("Manual override: FORCE_OFF");
    } else if (s.startsWith("CAL:")) {
      int a = s.indexOf(':');
      int b = s.indexOf(':', a + 1);
      if (b > a) {
        int dry = s.substring(a + 1, b).toInt();
        int wet = s.substring(b + 1).toInt();
        if (dry > wet) {
          raw_dry = dry;
          raw_wet = wet;
          Serial.print("Calibration set: dry=");
          Serial.print(raw_dry);
          Serial.print(" wet=");
          Serial.println(raw_wet);
        } else {
          Serial.println("Invalid CAL values: dry must be > wet");
        }
      } else {
        Serial.println("CAL format: CAL:dry:wet");
      }
    } else {
      Serial.print("Unknown command: "); Serial.println(s);
    }
  }
}

void startWatering() {
  digitalWrite(relayPin, HIGH);
  
  tone(buzzerPin, 2000); // 2000 Hz tone
  watering = true;
  waterStartTime = millis();
  Serial.println("WATERING STARTED!!!");
}

void stopWatering(const char *reason) {
  digitalWrite(relayPin, LOW);
  noTone(buzzerPin);
  watering = false;
  lastStopTime = millis();
  Serial.print("WATERING STOPPED (");
  Serial.print(reason);
  Serial.println(")!!!");
}

void loop() {
  handleSerial();

  float pct = readMoisturePercent();
  Serial.print("Moisture %: ");
  Serial.println(pct);

  // Safe LED shows when moisture >= STOP_PERCENT
  if (pct >= STOP_PERCENT) digitalWrite(ledSafe, HIGH);
  else digitalWrite(ledSafe, LOW);

  // Manual override handling
  if (forceOff && watering) {
    stopWatering("force_off");
  }

  if (forceOn && !watering) {
    startWatering();
    // persists until FORCE_OFF
  }

  if (!forceOn && !forceOff) {
    if (rainDisabled) {
      if (watering) stopWatering("rain_disabled");
    } else {
      if (!watering) {
        unsigned long sinceStop = millis() - lastStopTime;
        if (pct <= START_PERCENT && sinceStop >= MIN_INTERVAL_MS) {
          startWatering();
        }
      } else {
        if (pct >= STOP_PERCENT) {
          stopWatering("reached_stop_threshold");
        } else if ((millis() - waterStartTime) >= MAX_WATER_MS) {
          stopWatering("max_time_reached");
        }
      }
    }
  }

  delay(1000); // sample every 1 second for simulation
}
