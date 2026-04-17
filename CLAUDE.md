# FanController2

ESP32-based 3D printer enclosure fan controller (PlatformIO + Arduino framework).

## Build & Upload

```bash
pio run                 # compile
pio run -t upload       # flash to board
```

## Serial Access

The board is on `/dev/cu.usbserial-0001` at 115200 baud. Use Python to read/write since `pio device monitor` requires a TTY:

```python
# Read serial output for N seconds
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)
end = time.time() + 6
while time.time() < end:
    line = ser.readline().decode('utf-8', errors='replace').strip()
    if line:
        print(line)
ser.close()
"

# Send a command and read the response
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=1)
ser.write(b'status\n')
end = time.time() + 3
while time.time() < end:
    line = ser.readline().decode('utf-8', errors='replace').strip()
    if line:
        print(line)
ser.close()
"
```

## Serial Commands

- `help` — list all commands
- `status` — live state, temps, fans
- `get` — dump all settings
- `set <key> <value>` — change a setting (e.g. `set debug on`, `set bed 50`)
- `log list|fetch|delete` — manage log files

## HTTP Status Page

When WiFi is connected, `http://<device-ip>/` serves a plain-text status page with settings and log files. The IP is printed at boot: `[WiFi] WiFi success: <ip>`.
