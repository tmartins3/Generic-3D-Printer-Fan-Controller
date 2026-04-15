# Fan Combination Test Plan

Tests the state machine, fan outputs, mode labels, and menu visibility for all
seven valid fan combinations (any combination of one to three fans, excluding
the no-fan case).

All tests are performed through the serial debug interface at 115200 baud using
simulated sensor temperatures (`debug mode`). No physical sensor connections are
required.

---

## Bug fixed before testing

`StateMachine::_applyHeatingFanSpeeds()` previously drove the exhaust fan in
HEATING mode regardless of the `exhaustFanPresent` flag (recirc and heating fans
had the guard; exhaust did not). This has been corrected — all three fans now
respect their presence flags in all states.

---

## Default settings reference

These are the firmware defaults used as the baseline for all tests.
Run `get` on a freshly flashed board to verify before starting.

| Parameter | Value | Serial key |
|-----------|-------|-----------|
| Recirc start bed temp (RFSBT) | 45 °C | `rfsbt` |
| Recirc start speed | 30 % | — |
| Mode decision time (MDT) | 1 min | `mdt` (shortened for testing) |
| Hot bed threshold | 60 °C | `threshold` |
| Hot — heating fan speed | 50 % | — |
| Hot — recirc fan speed | 50 % | — |
| Hot — exhaust fan speed | 15 % | — |
| Cold — recirc fan speed | 20 % | — |
| Cold — exhaust fan min | 15 % | — |
| Cold — exhaust fan max | 100 % | — |
| Cold — max chamber temp | 38 °C | — |

---

## Common test setup

Run these commands at the start of every fan combination test:

```
set debug on
set mdt 1
set bed 20.0
set chamber 25.0
set manual off
set mode auto
```

This puts the controller in AUTO mode with simulated sensors, MDT shortened to
1 minute, bed cold (IDLE), and no manual override active.

---

## Fan combination matrix

Seven combinations are tested. The table summarises which fans are marked
present for each combination.

| Combination | ID | Exhaust | Heating | Recirc |
|-------------|-----|---------|---------|--------|
| Exhaust only | E | ✓ | — | — |
| Heating only | H | — | ✓ | — |
| Recirc only | R | — | — | ✓ |
| Exhaust + Heating | EH | ✓ | ✓ | — |
| Exhaust + Recirc | ER | ✓ | — | ✓ |
| Heating + Recirc | HR | — | ✓ | ✓ |
| All three | EHR | ✓ | ✓ | ✓ |

---

## Expected fan speeds per state per combination

Fan speeds shown as percentage. `PID` means PID-controlled (between 15–100 %,
converging toward 38 °C setpoint — exact value depends on chamber temperature
and PID accumulation). `0` means the fan is either absent or off by design.

| Combination | IDLE | RECIRC | HEATING | COOLING |
|-------------|------|--------|---------|---------|
| **E** — Exhaust:15 Recirc:0 Heating:0 | `0/0/0` | `0/0/0` | `15/0/0` | `PID/0/0` |
| **H** — Exhaust:0 Recirc:0 Heating:50 | `0/0/0` | `0/0/0` | `0/0/50` | `0/0/0` |
| **R** — Exhaust:0 Recirc:30 Heating:0 | `0/0/0` | `0/30/0` | `0/50/0` | `0/20/0` |
| **EH** | `0/0/0` | `0/0/0` | `15/0/50` | `PID/0/0` |
| **ER** | `0/0/0` | `0/30/0` | `15/50/0` | `PID/20/0` |
| **HR** | `0/0/0` | `0/30/0` | `0/50/50` | `0/20/0` |
| **EHR** | `0/0/0` | `0/30/0` | `15/50/50` | `PID/20/0` |

Column format: `Exhaust% / Recirc% / Heating%`

---

## Expected mode labels per combination

| Combination | Root item HEATING state | Root item COOLING state | Footer HEATING | Footer COOLING |
|-------------|------------------------|------------------------|----------------|----------------|
| E | HOT | COOLING | HOT | COOLING |
| H | HEATING | COOL | HEATING | COOL |
| R | HOT | COOL | HOT | COOL |
| EH | HEATING | COOLING | HEATING | COOLING |
| ER | HOT | COOLING | HOT | COOLING |
| HR | HEATING | COOL | HEATING | COOL |
| EHR | HEATING | COOLING | HEATING | COOLING |

> Labels change automatically when fan presence flags are toggled —
> no reboot required.

---

## Expected menu visibility per combination

Items listed are **hidden** (not visible) for each combination.

| Combination | Hidden menu items |
|-------------|-----------------|
| E | Heating Fan Speed (root), Recirculation Fan Spd (root), Start Recirc Fan Speed, Startup Bed Temp, Hot — Heating Fan Speed, Hot — Recirculation Fan, Cold — Recirculation Fan |
| H | Exhaust Fan Speed (root), Recirculation Fan Spd (root), Start Recirc Fan Speed, Startup Bed Temp, Hot — Exhaust Fan Speed, Cold — Exhaust Fan Max, Cold — Exhaust Fan Min, Cold — Max Chamber Temp, Cold — Recirculation Fan |
| R | Heating Fan Speed (root), Exhaust Fan Speed (root), Hot — Heating Fan Speed, Hot — Exhaust Fan Speed, Cold — Exhaust Fan Max, Cold — Exhaust Fan Min, Cold — Max Chamber Temp |
| EH | Recirculation Fan Spd (root), Start Recirc Fan Speed, Startup Bed Temp, Hot — Recirculation Fan, Cold — Recirculation Fan |
| ER | Heating Fan Speed (root), Hot — Heating Fan Speed |
| HR | Exhaust Fan Speed (root), Hot — Exhaust Fan Speed, Cold — Exhaust Fan Max, Cold — Exhaust Fan Min, Cold — Max Chamber Temp |
| EHR | *(nothing hidden — all items visible)* |

---

## Test procedures

Each section below covers one fan combination. Follow the steps in order.
Record `status` output at each checkpoint and compare against expected values.

---

### Combination E — Exhaust fan only

#### Setup

```
set debug on
set mdt 1
set bed 20.0
set chamber 25.0
set manual off
set mode auto
```

Then in the menu: set **Exhaust Fan Present = YES**, **Heating Fan Present = NO**,
**Recirc Fan Present = NO**. Verify with `get`.

---

#### TC-E.1 — IDLE state

**Action:** `status`

**Expected:**
```
 State   : IDLE
 Bed     : 20.0 C [SIM]
 Chamber : 25.0 C [SIM]
 Exhaust :   0 %      0 RPM
 Recirc  :   0 %      0 RPM
 Heating :   0 %      0 RPM
 Op mode : AUTO
```

**Pass criteria:** State = IDLE, all fans at 0 %.

---

#### TC-E.2 — RECIRC entry

**Action:**
```
set bed 50.0
```
Wait ~1 s, then: `status`

**Expected:**
```
 State   : RECIRCULATING
 Bed     : 50.0 C [SIM]
 Exhaust :   0 %
 Recirc  :   0 %
 Heating :   0 %
```

**Pass criteria:** State = RECIRCULATING. All fans at 0 % (recirc fan not present).

---

#### TC-E.3 — HEATING entry (bed above threshold, MDT expired)

**Action:** Set bed above threshold, wait for MDT (1 min):
```
set bed 65.0
```
Wait 60 s, then: `status`

**Expected:**
```
 State   : HEATING
 Bed     : 65.0 C [SIM]
 Exhaust :  15 %
 Recirc  :   0 %
 Heating :   0 %
```

**Mode label check:** Root menu Operating Mode item shows `HOT`.
Footer shows `HOT <chamber temp> C`.

**Pass criteria:** State = HEATING. Exhaust = 15 %, others = 0 %.
Root label and footer show `HOT` (not `HEATING`) because heating fan is absent.

---

#### TC-E.4 — COOLING entry (bed below threshold, MDT expired)

Return to IDLE first:
```
set bed 20.0
```
Wait for IDLE, then restart the cycle:
```
set bed 50.0
```
Wait 60 s (MDT), bed is 50 °C < 60 °C threshold → COOLING entered.
Set chamber above PID setpoint to observe fan response:
```
set chamber 42.0
```
Then: `status`

**Expected:**
```
 State   : COOLING
 Bed     : 50.0 C [SIM]
 Chamber : 42.0 C [SIM]
 Exhaust :  >15 %
 Recirc  :   0 %
 Heating :   0 %
```

**Mode label check:** Root item and footer show `COOLING`.

**Pass criteria:** State = COOLING. Exhaust between 15–100 % (PID active).
Recirc = 0 %, Heating = 0 %.

---

#### TC-E.5 — Return to IDLE

**Action:**
```
set bed 20.0
```
Wait ~1 s, then: `status`

**Expected:**
```
 State   : IDLE
 Exhaust :   0 %
 Recirc  :   0 %
 Heating :   0 %
```

**Pass criteria:** State = IDLE, all fans at 0 %.

---

### Combination H — Heating fan only

Setup: **Heating Fan Present = YES**, **Exhaust Fan Present = NO**,
**Recirc Fan Present = NO**.

---

#### TC-H.1 — IDLE state

**Expected:** State = IDLE, all fans 0 %.

---

#### TC-H.2 — RECIRC entry

```
set bed 50.0
```

**Expected:** State = RECIRCULATING, all fans 0 % (no recirc fan present).

---

#### TC-H.3 — HEATING entry

```
set bed 65.0
```
Wait 60 s. `status`

**Expected:**
```
 State   : HEATING
 Exhaust :   0 %
 Recirc  :   0 %
 Heating :  50 %
```

**Mode label:** Root item and footer show `HEATING`.

**Pass criteria:** Heating = 50 %, others = 0 %.

---

#### TC-H.4 — COOLING entry

Return to IDLE (`set bed 20.0`, wait), then:
```
set bed 50.0
```
Wait 60 s. `status`

**Expected:**
```
 State   : COOLING
 Exhaust :   0 %
 Recirc  :   0 %
 Heating :   0 %
```

**Mode label:** Root item and footer show `COOL` (not `COOLING`, exhaust absent).

**Pass criteria:** All fans 0 %. Label = `COOL`.

---

#### TC-H.5 — Return to IDLE

```
set bed 20.0
```

**Expected:** State = IDLE, all fans 0 %.

---

### Combination R — Recirc fan only

Setup: **Heating Fan Present = NO**, **Exhaust Fan Present = NO**,
**Recirc Fan Present = YES**.

---

#### TC-R.1 — IDLE state

**Expected:** State = IDLE, all fans 0 %.

---

#### TC-R.2 — RECIRC entry

```
set bed 50.0
```

**Expected:**
```
 State   : RECIRCULATING
 Exhaust :   0 %
 Recirc  :  30 %
 Heating :   0 %
```

**Pass criteria:** Recirc = 30 % (recircStartSpeed). Others = 0 %.

---

#### TC-R.3 — HEATING entry

```
set bed 65.0
```
Wait 60 s.

**Expected:**
```
 State   : HEATING
 Exhaust :   0 %
 Recirc  :  50 %
 Heating :   0 %
```

**Mode label:** `HOT` (no heating fan).

**Pass criteria:** Recirc = 50 % (hot.recircFanSpeed). Others = 0 %.

---

#### TC-R.4 — COOLING entry

Return to IDLE, restart cycle with bed at 50 °C, wait MDT.

**Expected:**
```
 State   : COOLING
 Exhaust :   0 %
 Recirc  :  20 %
 Heating :   0 %
```

**Mode label:** `COOL` (no exhaust fan).

**Pass criteria:** Recirc = 20 % (cold.recircFanSpeed). Others = 0 %.

---

#### TC-R.5 — Return to IDLE

```
set bed 20.0
```

**Expected:** State = IDLE, all fans 0 %.

---

### Combination EH — Exhaust + Heating fans

Setup: **Exhaust = YES**, **Heating = YES**, **Recirc = NO**.

---

#### TC-EH.1 — IDLE: all fans 0 %

#### TC-EH.2 — RECIRC

```
set bed 50.0
```

**Expected:** RECIRCULATING, all fans 0 % (no recirc fan).

---

#### TC-EH.3 — HEATING

```
set bed 65.0
```
Wait 60 s.

**Expected:**
```
 State   : HEATING
 Exhaust :  15 %
 Recirc  :   0 %
 Heating :  50 %
```

**Mode label:** `HEATING`.

---

#### TC-EH.4 — COOLING

Return to IDLE, restart with bed 50 °C, wait MDT.
```
set chamber 42.0
```

**Expected:**
```
 State   : COOLING
 Exhaust :  >15 %
 Recirc  :   0 %
 Heating :   0 %
```

**Mode label:** `COOLING`.

---

#### TC-EH.5 — Return to IDLE: `set bed 20.0`

---

### Combination ER — Exhaust + Recirc fans

Setup: **Exhaust = YES**, **Heating = NO**, **Recirc = YES**.

---

#### TC-ER.1 — IDLE: all fans 0 %

#### TC-ER.2 — RECIRC

```
set bed 50.0
```

**Expected:** RECIRCULATING, Recirc = 30 %, others 0 %.

---

#### TC-ER.3 — HEATING

```
set bed 65.0
```
Wait 60 s.

**Expected:**
```
 State   : HEATING
 Exhaust :  15 %
 Recirc  :  50 %
 Heating :   0 %
```

**Mode label:** `HOT` (no heating fan).

---

#### TC-ER.4 — COOLING

Return to IDLE, restart with bed 50 °C, wait MDT.
```
set chamber 42.0
```

**Expected:**
```
 State   : COOLING
 Exhaust :  >15 %
 Recirc  :  20 %
 Heating :   0 %
```

**Mode label:** `COOLING`.

---

#### TC-ER.5 — Return to IDLE: `set bed 20.0`

---

### Combination HR — Heating + Recirc fans

Setup: **Exhaust = NO**, **Heating = YES**, **Recirc = YES**.

---

#### TC-HR.1 — IDLE: all fans 0 %

#### TC-HR.2 — RECIRC

```
set bed 50.0
```

**Expected:** RECIRCULATING, Recirc = 30 %, others 0 %.

---

#### TC-HR.3 — HEATING

```
set bed 65.0
```
Wait 60 s.

**Expected:**
```
 State   : HEATING
 Exhaust :   0 %
 Recirc  :  50 %
 Heating :  50 %
```

**Mode label:** `HEATING`.

---

#### TC-HR.4 — COOLING

Return to IDLE, restart with bed 50 °C, wait MDT.

**Expected:**
```
 State   : COOLING
 Exhaust :   0 %
 Recirc  :  20 %
 Heating :   0 %
```

**Mode label:** `COOL` (no exhaust fan).

---

#### TC-HR.5 — Return to IDLE: `set bed 20.0`

---

### Combination EHR — All three fans

Setup: **Exhaust = YES**, **Heating = YES**, **Recirc = YES**.

---

#### TC-EHR.1 — IDLE: all fans 0 %

#### TC-EHR.2 — RECIRC

```
set bed 50.0
```

**Expected:** RECIRCULATING, Recirc = 30 %, others 0 %.

---

#### TC-EHR.3 — HEATING

```
set bed 65.0
```
Wait 60 s.

**Expected:**
```
 State   : HEATING
 Exhaust :  15 %
 Recirc  :  50 %
 Heating :  50 %
```

**Mode label:** `HEATING`.

---

#### TC-EHR.4 — COOLING

Return to IDLE, restart with bed 50 °C, wait MDT.
```
set chamber 42.0
```

**Expected:**
```
 State   : COOLING
 Exhaust :  >15 %
 Recirc  :  20 %
 Heating :   0 %
```

**Mode label:** `COOLING`.

---

#### TC-EHR.5 — Return to IDLE: `set bed 20.0`

---

## Additional test cases

### TC-MANUAL — Manual fan control override

Applies to all combinations. Verify manual control overrides the state machine.

```
set debug on
set bed 65.0
set mode auto
```
Wait for HEATING, then:
```
set manual on
set heating 80
set exhaust 60
set recirc 40
```

**Expected `status`:**
```
 State   : HEATING      (state machine frozen)
 Exhaust :  60 %
 Recirc  :  40 %
 Heating :  80 %
 Manual fans : ON
```

Disable and verify state machine resumes:
```
set manual off
```

**Expected:** Fans return to values dictated by current state within 500 ms.

---

### TC-SENSOR-FAIL — Chamber sensor failure in COOLING (Exhaust or EHR only)

Only relevant for combinations that include the exhaust fan.

```
set debug off
set mode cooling
```

Disconnect or disable the chamber sensor. Within the next PID cycle (2 s):

**Expected `status`:**
```
 State   : COOLING
 Exhaust : 100 %        (exhaustFanMax — failsafe)
 *** CHAMBER SENSOR ERROR ***
```

**Pass criteria:** Exhaust at exhaustFanMax (100 %). Error flag reported.

---

### TC-FORCED-MODE — Forced HEATING / COOLING bypass MDT

```
set debug on
set bed 20.0
set mode heating
```

**Expected immediately (no MDT wait):**
```
 State   : HEATING
```

```
set mode cooling
```

**Expected immediately:**
```
 State   : COOLING
```

Return to AUTO:
```
set mode auto
set bed 20.0
```

**Expected:** IDLE (bed is cold).

---

### TC-LABEL — Mode label changes with fan presence flags

With all fans present (EHR):
```
 State   : HEATING  → root item shows HEATING, footer shows HEATING
 State   : COOLING  → root item shows COOLING, footer shows COOLING
```

Toggle exhaust fan absent (in menu: Exhaust Fan Present = NO):
```
 State   : COOLING  → root item shows COOL, footer shows COOL
```

Toggle heating fan absent (in menu: Heating Fan Present = NO):
```
 State   : HEATING  → root item shows HOT, footer shows HOT
```

Re-enable both fans — labels restore without reboot.

---

## Test result log template

| TC ID | Combination | Expected state | Expected fans (E/R/H %) | Mode label | Result | Notes |
|-------|-------------|---------------|------------------------|------------|--------|-------|
| TC-E.1 | E | IDLE | 0/0/0 | — | | |
| TC-E.2 | E | RECIRC | 0/0/0 | — | | |
| TC-E.3 | E | HEATING | 15/0/0 | HOT | | |
| TC-E.4 | E | COOLING | PID/0/0 | COOLING | | |
| TC-E.5 | E | IDLE | 0/0/0 | — | | |
| TC-H.1 | H | IDLE | 0/0/0 | — | | |
| TC-H.2 | H | RECIRC | 0/0/0 | — | | |
| TC-H.3 | H | HEATING | 0/0/50 | HEATING | | |
| TC-H.4 | H | COOLING | 0/0/0 | COOL | | |
| TC-H.5 | H | IDLE | 0/0/0 | — | | |
| TC-R.1 | R | IDLE | 0/0/0 | — | | |
| TC-R.2 | R | RECIRC | 0/30/0 | — | | |
| TC-R.3 | R | HEATING | 0/50/0 | HOT | | |
| TC-R.4 | R | COOLING | 0/20/0 | COOL | | |
| TC-R.5 | R | IDLE | 0/0/0 | — | | |
| TC-EH.1 | EH | IDLE | 0/0/0 | — | | |
| TC-EH.2 | EH | RECIRC | 0/0/0 | — | | |
| TC-EH.3 | EH | HEATING | 15/0/50 | HEATING | | |
| TC-EH.4 | EH | COOLING | PID/0/0 | COOLING | | |
| TC-EH.5 | EH | IDLE | 0/0/0 | — | | |
| TC-ER.1 | ER | IDLE | 0/0/0 | — | | |
| TC-ER.2 | ER | RECIRC | 0/30/0 | — | | |
| TC-ER.3 | ER | HEATING | 15/50/0 | HOT | | |
| TC-ER.4 | ER | COOLING | PID/20/0 | COOLING | | |
| TC-ER.5 | ER | IDLE | 0/0/0 | — | | |
| TC-HR.1 | HR | IDLE | 0/0/0 | — | | |
| TC-HR.2 | HR | RECIRC | 0/30/0 | — | | |
| TC-HR.3 | HR | HEATING | 0/50/50 | HEATING | | |
| TC-HR.4 | HR | COOLING | 0/20/0 | COOL | | |
| TC-HR.5 | HR | IDLE | 0/0/0 | — | | |
| TC-EHR.1 | EHR | IDLE | 0/0/0 | — | | |
| TC-EHR.2 | EHR | RECIRC | 0/30/0 | — | | |
| TC-EHR.3 | EHR | HEATING | 15/50/50 | HEATING | | |
| TC-EHR.4 | EHR | COOLING | PID/20/0 | COOLING | | |
| TC-EHR.5 | EHR | IDLE | 0/0/0 | — | | |
| TC-MANUAL | Any | HEATING | 60/40/80 | — | | |
| TC-SENSOR-FAIL | E/EH/ER/EHR | COOLING | 100/−/− | — | | |
| TC-FORCED-MODE | Any | HEATING/COOLING | per combo | — | | |
| TC-LABEL | EHR→E→H | varies | — | HOT/COOL | | |
