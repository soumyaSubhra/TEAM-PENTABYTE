# TEAM-PENTABYTE  
# Precision Irrigation — Tinkercad Simulation (Arduino Uno)

**Team Name:** Team Pentabyte  
**Project:** Precision irrigation simulation (Arduino Uno + Tinkercad)  

---

## Why Arduino Uno Instead of ESP32?

Originally, our plan was to use **ESP32** for this simulation as it offers Wi-Fi connectivity and real IoT deployment possibilities.  
However, due to **Wokwi server downtime** at the time of development, we could not simulate the ESP32 online.  
To ensure the project demo continued smoothly, we switched to **Arduino Uno** on Tinkercad for simulation purposes.  

We have still provided the **ESP32 code** in this repository for reference if you want to run it later when the server is back online.

---

## Files Included  

| File Name               | Description                                                                 |
|-------------------------|-----------------------------------------------------------------------------|
| `Sourcecode.ino`     | Arduino Uno sketch for Tinkercad simulation (used in final demo).           |
| `ESP32sourcecode.ino`   | ESP32 version of the code (reference only, for real hardware deployment).   |
| `gateway_simulator.py`   | Python script simulating weather gateway with commands (`RAIN_YES`, etc.).   |
| `README.md`              | Project documentation with wiring, ET basics, and demo flow.                |

---

## How the Simulation Works (Arduino Uno)

- Potentiometer on **A0** emulates a soil moisture sensor.  
- **D8** LED → Valve state (ON = watering).  
- **D6** LED → SAFE state (moisture ≥ stop threshold).  
- **D7** → Buzzer ON when watering.  

Serial commands:
- `RAIN_YES` → Disable watering (rain forecast high).  
- `RAIN_NO` → Enable watering.  
- `FORCE_ON` / `FORCE_OFF` → Manual override.  
- `CAL:dry:wet` → Calibration (e.g., `CAL:900:200`).  

---

## Tinkercad Setup

1. Place Arduino Uno, breadboard, potentiometer, LEDs, and buzzer. Wire as per diagram in repo.  
2. Upload `irrigation_uno.ino` in Tinkercad code editor.  
3. Use Serial Monitor at **9600 baud** for gateway commands.  

---

## Evapotranspiration (ET) Basics

- Formula: **ETc = ET0 × Kc**  
  - ET0 → Weather-based reference ET (mm/day)  
  - Kc → Crop coefficient  
- Water requirement (liters/day) = ETc (mm/day) × area (m²)  

**Example:**  
If ET0 = 5 mm/day and Kc = 0.8 → ETc = 4 mm/day  
For 10 m² area → 40 L/day needed.  
With 4 L/hr emitters → 10 hours total irrigation (split into cycles).

---

## Demo Flow

1. Start simulation and open Serial Monitor.  
2. Send `CAL:900:200` for calibration (optional).  
3. Send `RAIN_NO` → Move pot dry (≤20%) → Valve ON → Move to wet (≥80%) → Valve OFF.  
4. Send `RAIN_YES` → Valve stays OFF even if soil is dry.  
5. Use `FORCE_ON` / `FORCE_OFF` for manual control.  

---

## ESP32 Reference Code (Not Simulated Due to Wokwi Downtime)

```cpp
// irrigation_esp32.ino
// ESP32 version for later real hardware deployment

#define SENSOR_PIN 34
#define VALVE_PIN  5
#define SAFE_LED   18
#define BUZZER_PIN 19

int raw_dry = 900;
int raw_wet = 200;
float START_PERCENT = 20.0;
float STOP_PERCENT  = 80.0;
bool rain_block = false;

void setup() {
  Serial.begin(9600);
  pinMode(VALVE_PIN, OUTPUT);
  pinMode(SAFE_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  int raw = analogRead(SENSOR_PIN);
  float moisture = map(raw, raw_dry, raw_wet, 0, 100);

  if (!rain_block) {
    if (moisture <= START_PERCENT) {
      digitalWrite(VALVE_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
    } 
    else if (moisture >= STOP_PERCENT) {
      digitalWrite(VALVE_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(SAFE_LED, HIGH);
    } 
    else {
      digitalWrite(SAFE_LED, LOW);
    }
  }
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd == "RAIN_YES") rain_block = true;
    else if (cmd == "RAIN_NO") rain_block = false;
  }

  delay(500);
}
