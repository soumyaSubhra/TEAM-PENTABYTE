#!/usr/bin/env python3

import os, time, argparse, requests, sys

try:
    import serial
except Exception:
    serial = None

OPENWEATHER_ONECALL = "https://api.openweathermap.org/data/2.5/onecall"
OPENWEATHER_GEOCODE = "http://api.openweathermap.org/geo/1.0/direct"

def get_args():
    p = argparse.ArgumentParser()
    group = p.add_mutually_exclusive_group(required=True)
    group.add_argument("--city", help="City name (e.g. 'Bengaluru,IN'). Uses OpenWeather geocoding.")
    group.add_argument("--lat", type=float, help="Latitude (decimal degrees)")
    p.add_argument("--lon", type=float, help="Longitude (decimal degrees; required with --lat)")
    p.add_argument("--port", help="Serial port (e.g. COM3 or /dev/ttyUSB0). If omitted the script prints commands")
    p.add_argument("--baud", type=int, default=115200, help="Serial baud (default 115200)")
    p.add_argument("--interval", type=int, default=1800, help="Seconds between checks (default 1800 sec = 30 min)")
    p.add_argument("--threshold", type=float, default=0.5, help="rain probability threshold (0..1)")
    p.add_argument("--send-only-on-change", action="store_true", help="Only send when decision changes")
    p.add_argument("--units", choices=["metric","imperial","standard"], default="metric")
    return p.parse_args()

def geocode_city(api_key, city):
    params = {"q": city, "limit": 1, "appid": api_key}
    r = requests.get(OPENWEATHER_GEOCODE, params=params, timeout=15)
    r.raise_for_status()
    data = r.json()
    if not data:
        raise ValueError("Geocoding returned no results for: " + city)
    return float(data[0]["lat"]), float(data[0]["lon"])

def fetch_onecall(api_key, lat, lon, units="metric"):
    params = {"lat": lat, "lon": lon, "exclude": "minutely,current,daily,alerts", "appid": api_key, "units": units}
    r = requests.get(OPENWEATHER_ONECALL, params=params, timeout=20)
    r.raise_for_status()
    return r.json()

def compute_maxpop_hourly(onecall_json, hours=24):
    hourly = onecall_json.get("hourly", [])
    pops = []
    for hr in hourly[:hours]:
        pop = hr.get("pop", 0.0)
        pops.append(float(pop))
    return max(pops) if pops else 0.0

def open_serial(port, baud):
    if serial is None:
        raise RuntimeError("pyserial not installed (pip install pyserial)")
    ser = serial.Serial(port, baud, timeout=2)
    time.sleep(2.0)  # allow Arduino reset
    return ser

def send_cmd(ser, cmd, do_print=True):
    if do_print: print("SEND:", cmd.strip())
    if ser:
        ser.write(cmd.encode())
        ser.flush()

def main():
    args = get_args()
    api_key = os.environ.get("OPENWEATHER_API_KEY")
    if not api_key:
        print("ENV missing: set OPENWEATHER_API_KEY", file=sys.stderr); sys.exit(1)

    if args.city:
        lat, lon = geocode_city(api_key, args.city)
        print(f"Geocoded {args.city} -> lat={lat}, lon={lon}")
    else:
        if args.lon is None:
            print("Error: --lon required with --lat", file=sys.stderr); sys.exit(1)
        lat, lon = args.lat, args.lon

    ser = None
    if args.port:
        try:
            ser = open_serial(args.port, args.baud)
            print(f"Opened serial {args.port} at {args.baud}")
        except Exception as e:
            print("Serial open failed:", e, file=sys.stderr)
            print("Proceeding in print-only mode.")
            ser = None

    last_decision = None
    print("Gateway running. Interval:", args.interval, "sec. Threshold:", args.threshold)

    try:
        while True:
            j = None
            try:
                j = fetch_onecall(api_key, lat, lon, units=args.units)
            except Exception as e:
                print("Weather fetch error:", e)
                time.sleep(min(60, args.interval))
                continue
            maxpop = compute_maxpop_hourly(j, hours=24)
            print("maxpop (next24h) =", maxpop)
            decision = "RAIN_YES" if maxpop >= args.threshold else "RAIN_NO"
            if args.send_only_on_change:
                if decision != last_decision:
                    send_cmd(ser, decision + "\n", do_print=True)
                    last_decision = decision
                else:
                    print("Decision unchanged:", decision)
            else:
                send_cmd(ser, decision + "\n", do_print=True)
                last_decision = decision
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("Stopping by user.")
    finally:
        if ser: ser.close()

if __name__ == "__main__":
    main()
