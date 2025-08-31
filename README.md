# TEAM-PENTABYTE
# Precision Irrigation — Tinkercad Simulation (Arduino Uno)

**Team Name:** <Team Pentabyte>  
**Project:** Precision irrigation simulation (Arduino Uno + Tinkercad)  
**Purpose:** Simulate automatic irrigation using a soil moisture sensor and a weather gateway decision (rain probability). This repo is intended for simulation-only demonstration.

## What’s included
- `irrigation_uno.ino` — Arduino Uno sketch, Tinkercad-ready. Uses a potentiometer (A0) to simulate soil moisture.
- `gateway_simulator.py` — Optional Python script that simulates a gateway that decides `RAIN_YES`/`RAIN_NO`.
- This README with wiring, demo steps, and ET explanation.

## How the simulation works
- Potentiometer on **A0** emulates a capacitive soil moisture sensor (map raw to %).
- **D8** LED simulates the valve state (ON = watering).
- **D6** LED shows SAFE state (moisture >= stop threshold).
- **D7** simulates a buzzer when watering.

The Arduino listens to simple serial commands:
- `RAIN_YES` — disable automatic watering (simulate rain forecast >50%).
- `RAIN_NO`  — allow automatic watering.
- `FORCE_ON` / `FORCE_OFF` — manual overrides.
- `CAL:dry:wet` — set calibration raw values (e.g., `CAL:900:200`).

## Tinkercad setup (short)
1. Place Arduino Uno, breadboard, potentiometer, LEDs, buzzer. Wire as described in repository.
2. Open `irrigation_uno.ino` in Tinkercad code editor, upload/run simulation.
3. Use Serial Monitor (9600 baud) to send gateway commands and see logs.

## Evapotranspiration (ET) — brief (include in report)
- `ETc = ET0 × Kc` (mm/day)
  - ET0: reference evapotranspiration (weather-driven)
  - Kc: crop coefficient (depends on crop & growth stage)
- Convert ETc to liters: `ETc(mm/day) × area(m²) = liters/day`
- Use ETc to size irrigation events or adjust stop/start thresholds on hot days.

**Worked example** (put in appendix of report):  
If ET0=5 mm/day and Kc=0.8 → ETc=4 mm/day. For a 10 m² zone → 40 L/day. With 4 L/hr emitters you need 10 hours total (or split cycles).

## Demo flow (exact)
1. Start Tinkercad simulation, open Serial Monitor (9600).  
2. Calibrate (optional): `CAL:900:200` (adjust to your pot extremes).  
3. Send `RAIN_NO`. Move pot to simulate dry (<= 20%). Valve LED should turn ON. Move pot to wet (>= 80%) — valve stops.  
4. Send `RAIN_YES` and show valve stays OFF even if pot is dry.  
5. Show manual override with `FORCE_ON` / `FORCE_OFF`.

## Next steps (if you later move to hardware)
- Replace potentiometer with a real capacitive soil moisture sensor (map raw → VWC).  
- Replace Valve LED with a relay/MOSFET controlling a 12V valve (common ground if using MOSFET). Always use proper flyback protection for coils.  
- Replace manual Serial gateway with a Python script that fetches OpenWeather and sends `RAIN_YES`/`RAIN_NO` via serial.

## Files
- `irrigation_uno.ino` — main Arduino sketch
- `gateway_simulator.py` — optional gateway demo script

