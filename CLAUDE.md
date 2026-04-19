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

## Coding style and strategy
- Always update this document if new functionality is added
- When naming file, classes, variables. Then the name consists of two parts, always start the name with the primary descrption. An example: Do not name as: TemperatureLogger but LoggerTemperature as the "Logger" word is the main defining role of the object/file.
- prioritise simplicity always, secondy separation of concerns. All software should consist of separate "services" invoked from the main logic/function.
- Add comments and use good naming
- Standard Arduino code conventions
- Exception: use camelCase or CamelCase (Java style)
- Divide into reasonably sized files by functionality
- Use C++ classes where applicable

## Development process
### general rules
- if features are added or removed or functionality is changed, always update README and PROJECT.md
- When desinging the software think in terms of "services"/modules/objects. follow object orientted design principles.
- always find the simplest solution first then make it just as complicated as required for the software and to achieve good separation of concerns and modularity
- always comment the the code if it is not self evident
- strive for simple solutions, start with the simplest that could work, then complicate just enough to make it elegant and follow good practices and our guidlines

### process
- always plan your implementation first, analyse it from the following perspectives:
 - does it follow our separation of concerns and modularity principles?
 - does it require updates  to project.md and README
 - does it create new dependence and/or cross references between modules?
- update the plan if needed
- implement the plan
- update project.md and README if needed
- Give me a report of what you did and were new dependences created.
- ask for push to git