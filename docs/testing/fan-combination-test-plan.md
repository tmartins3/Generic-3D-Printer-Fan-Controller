# Fan Combination Test Plan

Automated tests for the state machine, fan outputs, mode labels, manual override,
forced modes, settings round-trip, and fan presence interactions. All tests run
through the serial debug interface at 115200 baud using simulated sensor
temperatures (`debug mode`). No physical sensor connections are required.

Run with: `python3 docs/testing/test_runner.py`

---

## Serial commands used

| Command | Purpose |
|---------|---------|
| `status` | Read current state, temps, fan speeds, flags |
| `get` | Dump all settings |
| `set debug on/off` | Enable/disable simulated sensors |
| `set chamber <°C>` | Set simulated chamber temperature |
| `set bed <°C>` | Set simulated bed temperature |
| `set manual on/off` | Enable/disable manual fan override |
| `set heating <0-100>` | Manual heating fan speed % |
| `set exhaust <0-100>` | Manual exhaust fan speed % |
| `set recirc <0-100>` | Manual recirc fan speed % |
| `set mode auto/heating/cooling` | Set operating mode |
| `set mdt <min>` | Set mode decision time |
| `set rfsbt <°C>` | Set recirc start bed temp |
| `set threshold <°C>` | Set hot chamber bed threshold |
| `set hfp on/off` | Set heating fan present |
| `set efp on/off` | Set exhaust fan present |
| `set rfp on/off` | Set recirc fan present |
| `set light on/off` | Chamber light control |

---

## Test categories

### 1. Fan combination matrix (7 combos x 5 states = 35 tests)

For each of the 7 fan combinations (E, H, R, EH, ER, HR, EHR), test all
state transitions: IDLE -> RECIRC -> HEATING -> IDLE -> RECIRC -> COOLING -> IDLE.
Verify fan speeds match expected values per combination per state.

### 2. Manual fan override (TC-MANUAL)

Verify `set manual on` overrides state machine fan speeds, and `set manual off`
restores state-machine-controlled speeds.

### 3. Forced modes (TC-FORCED)

Verify `set mode heating` and `set mode cooling` enter the corresponding state
immediately without waiting for MDT. Verify `set mode auto` returns to normal
state machine behaviour.

### 4. Settings round-trip (TC-SETTINGS)

Verify `set` commands persist and `get` reflects the new values. Covers all
settable parameters including fan presence flags.

### 5. State transition edge cases (TC-EDGE)

- Bed drops below RFSBT during HEATING -> returns to IDLE
- Bed drops below RFSBT during COOLING -> returns to IDLE
- Bed rises above RFSBT from IDLE -> enters RECIRC

### 6. Chamber light (TC-LIGHT)

Verify `set light on` and `set light off` toggle correctly.

---

## Expected fan speeds per state per combination

Fan speeds shown as `Exhaust/Recirc/Heating` percentage.

| Combo | IDLE | RECIRC | HEATING | COOLING |
|-------|------|--------|---------|---------|
| **E** | 0/0/0 | 0/0/0 | 15/0/0 | PID/0/0 |
| **H** | 0/0/0 | 0/0/0 | 0/0/50 | 0/0/0 |
| **R** | 0/0/0 | 0/30/0 | 0/50/0 | 0/20/0 |
| **EH** | 0/0/0 | 0/0/0 | 15/0/50 | PID/0/0 |
| **ER** | 0/0/0 | 0/30/0 | 15/50/0 | PID/20/0 |
| **HR** | 0/0/0 | 0/30/0 | 0/50/50 | 0/20/0 |
| **EHR** | 0/0/0 | 0/30/0 | 15/50/50 | PID/20/0 |

PID = between exhaustFanMin (15%) and exhaustFanMax (100%), depends on chamber temp.

---

## Test result log

Results are generated automatically by `test_runner.py` and saved to
`docs/testing/test-results-YYYY-MM-DD-HHMMSS.md`.
