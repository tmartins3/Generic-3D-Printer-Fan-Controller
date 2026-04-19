#!/usr/bin/env python3
"""
Automated test runner for FanController2 fan combination tests.

Communicates with the ESP32 over serial to:
  - Set fan presence flags, debug temps, operating modes
  - Read status output and verify state + fan speeds
  - Run the full 7-combination state matrix plus edge cases

Usage:
    python3 docs/testing/test_runner.py [--port /dev/cu.usbserial-0001] [--mdt 1]

Requires: pyserial (pip install pyserial)
"""

import serial
import time
import re
import sys
import argparse
from datetime import datetime
from dataclasses import dataclass, field
from typing import Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DEFAULT_PORT = "/dev/cu.usbserial-0001"
BAUD_RATE = 115200
MDT_MINUTES = 1          # shortened mode decision time for testing
MDT_WAIT_SEC = 65        # wait slightly longer than MDT to be safe
SETTLE_SEC = 2           # time to let state machine settle after a command
STATUS_POLL_SEC = 0.5    # time between status reads during waits

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------
@dataclass
class StatusReading:
    """Parsed output from the 'status' serial command."""
    state: str = ""
    bed_temp: float = 0.0
    chamber_temp: float = 0.0
    exhaust_pct: int = 0
    recirc_pct: int = 0
    heating_pct: int = 0
    debug_mode: str = ""
    manual_fans: str = ""
    chamber_light: str = ""
    op_mode: str = ""
    chamber_sensor_error: bool = False
    raw: str = ""

@dataclass
class TestResult:
    """Result of a single test case."""
    test_id: str
    description: str
    passed: bool
    expected: str
    actual: str
    notes: str = ""

# ---------------------------------------------------------------------------
# Serial helpers
# ---------------------------------------------------------------------------
class SerialConnection:
    def __init__(self, port: str, baud: int = BAUD_RATE):
        self.ser = serial.Serial(port, baud, timeout=1)
        time.sleep(0.5)  # let serial settle
        self.drain()

    def drain(self):
        """Read and discard all pending data."""
        while self.ser.in_waiting:
            self.ser.read(self.ser.in_waiting)
            time.sleep(0.05)

    def send(self, cmd: str, settle: float = 0.3):
        """Send a command and wait for response."""
        self.drain()
        self.ser.write((cmd + "\n").encode())
        time.sleep(settle)

    def read_lines(self, timeout: float = 2.0) -> list[str]:
        """Read all available lines within timeout."""
        lines = []
        end = time.time() + timeout
        while time.time() < end:
            line = self.ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                lines.append(line)
            elif lines:
                # Got some lines, small gap — check if more coming
                time.sleep(0.1)
                if not self.ser.in_waiting:
                    break
        return lines

    def send_and_read(self, cmd: str, settle: float = 0.3, timeout: float = 2.0) -> list[str]:
        """Send command and return all response lines."""
        self.send(cmd, settle)
        return self.read_lines(timeout)

    def close(self):
        self.ser.close()


def parse_status(lines: list[str]) -> StatusReading:
    """Parse the output of the 'status' command into a StatusReading."""
    s = StatusReading(raw="\n".join(lines))
    for line in lines:
        line = line.strip()
        # State : IDLE
        m = re.match(r"State\s*:\s*(\w+)", line)
        if m:
            s.state = m.group(1)
        # Bed : 20.0 C [SIM]
        m = re.match(r"Bed\s*:\s*([\d.]+)\s*C", line)
        if m:
            s.bed_temp = float(m.group(1))
        # Chamber : 25.0 C [SIM]
        m = re.match(r"Chamber\s*:\s*([\d.]+)\s*C", line)
        if m:
            s.chamber_temp = float(m.group(1))
        # Exhaust :  15 %   0 RPM
        m = re.match(r"Exhaust\s*:\s*(\d+)\s*%", line)
        if m:
            s.exhaust_pct = int(m.group(1))
        # Recirc :  30 %   0 RPM
        m = re.match(r"Recirc\s*:\s*(\d+)\s*%", line)
        if m:
            s.recirc_pct = int(m.group(1))
        # Heating :  50 %   0 RPM
        m = re.match(r"Heating\s*:\s*(\d+)\s*%", line)
        if m:
            s.heating_pct = int(m.group(1))
        # Debug mode : ON
        m = re.match(r"Debug mode\s*:\s*(\w+)", line)
        if m:
            s.debug_mode = m.group(1)
        # Manual fans : OFF
        m = re.match(r"Manual fans\s*:\s*(\w+)", line)
        if m:
            s.manual_fans = m.group(1)
        # Chamber light : OFF
        m = re.match(r"Chamber light\s*:\s*(\w+)", line)
        if m:
            s.chamber_light = m.group(1)
        # Op mode : AUTO
        m = re.match(r"Op mode\s*:\s*(\w+)", line)
        if m:
            s.op_mode = m.group(1)
        # *** CHAMBER SENSOR ERROR ***
        if "CHAMBER SENSOR ERROR" in line:
            s.chamber_sensor_error = True
    return s


def get_status(conn: SerialConnection) -> StatusReading:
    """Send 'status' and parse the response."""
    lines = conn.send_and_read("status", settle=0.5, timeout=3.0)
    return parse_status(lines)


def setup_common(conn: SerialConnection, mdt: int = MDT_MINUTES):
    """Reset to common test baseline."""
    cmds = [
        "set debug on",
        f"set mdt {mdt}",
        "set bed 20.0",
        "set chamber 25.0",
        "set manual off",
        "set mode auto",
        "set threshold 60",
        "set rfsbt 45",
    ]
    for cmd in cmds:
        conn.send_and_read(cmd, settle=0.3)
    time.sleep(SETTLE_SEC)


def set_fan_presence(conn: SerialConnection, exhaust: bool, heating: bool, recirc: bool):
    """Set fan presence flags via serial."""
    conn.send_and_read(f"set efp {'on' if exhaust else 'off'}", settle=0.3)
    conn.send_and_read(f"set hfp {'on' if heating else 'off'}", settle=0.3)
    conn.send_and_read(f"set rfp {'on' if recirc else 'off'}", settle=0.3)
    time.sleep(0.5)


def wait_for_state(conn: SerialConnection, target_state: str, timeout: float = MDT_WAIT_SEC) -> StatusReading:
    """Poll status until target state is reached or timeout."""
    end = time.time() + timeout
    last_status = None
    while time.time() < end:
        last_status = get_status(conn)
        if last_status.state == target_state:
            return last_status
        time.sleep(STATUS_POLL_SEC)
    return last_status  # return last reading even if state not reached


# ---------------------------------------------------------------------------
# Test assertions
# ---------------------------------------------------------------------------
def check_fans(status: StatusReading, exp_e: int, exp_r: int, exp_h: int,
               e_is_pid: bool = False) -> tuple[bool, str]:
    """
    Check fan percentages. If e_is_pid=True, exhaust must be between 15-100%.
    Returns (passed, description).
    """
    issues = []

    if e_is_pid:
        if status.exhaust_pct < 15 or status.exhaust_pct > 100:
            issues.append(f"Exhaust={status.exhaust_pct}% (expected PID 15-100%)")
    else:
        if status.exhaust_pct != exp_e:
            issues.append(f"Exhaust={status.exhaust_pct}% (expected {exp_e}%)")

    if status.recirc_pct != exp_r:
        issues.append(f"Recirc={status.recirc_pct}% (expected {exp_r}%)")

    if status.heating_pct != exp_h:
        issues.append(f"Heating={status.heating_pct}% (expected {exp_h}%)")

    actual = f"E={status.exhaust_pct}% R={status.recirc_pct}% H={status.heating_pct}%"
    if issues:
        return False, actual + " ISSUES: " + "; ".join(issues)
    return True, actual


# ---------------------------------------------------------------------------
# Fan combination test matrix
# ---------------------------------------------------------------------------
# (combo_name, exhaust_present, heating_present, recirc_present)
FAN_COMBOS = [
    ("E",   True,  False, False),
    ("H",   False, True,  False),
    ("R",   False, False, True),
    ("EH",  True,  True,  False),
    ("ER",  True,  False, True),
    ("HR",  False, True,  True),
    ("EHR", True,  True,  True),
]

# Expected fan speeds per combo per state: (exhaust, recirc, heating, exhaust_is_pid)
EXPECTED_FANS = {
    # combo: { state: (exhaust%, recirc%, heating%, is_pid) }
    "E":   {"IDLE": (0,0,0,False), "RECIRCULATING": (0,0,0,False), "HEATING": (15,0,0,False), "COOLING": (0,0,0,True)},
    "H":   {"IDLE": (0,0,0,False), "RECIRCULATING": (0,0,0,False), "HEATING": (0,0,50,False), "COOLING": (0,0,0,False)},
    "R":   {"IDLE": (0,0,0,False), "RECIRCULATING": (0,30,0,False), "HEATING": (0,50,0,False), "COOLING": (0,20,0,False)},
    "EH":  {"IDLE": (0,0,0,False), "RECIRCULATING": (0,0,0,False), "HEATING": (15,0,50,False), "COOLING": (0,0,0,True)},
    "ER":  {"IDLE": (0,0,0,False), "RECIRCULATING": (0,30,0,False), "HEATING": (15,50,0,False), "COOLING": (0,20,0,True)},
    "HR":  {"IDLE": (0,0,0,False), "RECIRCULATING": (0,30,0,False), "HEATING": (0,50,50,False), "COOLING": (0,20,0,False)},
    "EHR": {"IDLE": (0,0,0,False), "RECIRCULATING": (0,30,0,False), "HEATING": (15,50,50,False), "COOLING": (0,20,0,True)},
}


def run_combo_tests(conn: SerialConnection, combo_name: str,
                    exhaust: bool, heating: bool, recirc: bool,
                    mdt: int) -> list[TestResult]:
    """Run the 5-state cycle for one fan combination."""
    results = []

    print(f"\n--- Combo {combo_name} (E={'Y' if exhaust else 'N'} H={'Y' if heating else 'N'} R={'Y' if recirc else 'N'}) ---")

    # Setup
    setup_common(conn, mdt)
    set_fan_presence(conn, exhaust, heating, recirc)
    time.sleep(SETTLE_SEC)

    exp = EXPECTED_FANS[combo_name]

    # --- TC-X.1: IDLE ---
    tid = f"TC-{combo_name}.1"
    print(f"  {tid}: IDLE...", end=" ", flush=True)
    s = get_status(conn)
    state_ok = s.state == "IDLE"
    fans_ok, fans_desc = check_fans(s, *exp["IDLE"])
    passed = state_ok and fans_ok
    results.append(TestResult(
        test_id=tid, description=f"{combo_name} IDLE state",
        passed=passed,
        expected=f"State=IDLE E={exp['IDLE'][0]}% R={exp['IDLE'][1]}% H={exp['IDLE'][2]}%",
        actual=f"State={s.state} {fans_desc}",
        notes="" if passed else f"state_ok={state_ok}"
    ))
    print("PASS" if passed else "FAIL")

    # --- TC-X.2: RECIRC ---
    tid = f"TC-{combo_name}.2"
    print(f"  {tid}: RECIRC...", end=" ", flush=True)
    conn.send_and_read("set bed 50.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    state_ok = s.state == "RECIRCULATING"
    fans_ok, fans_desc = check_fans(s, *exp["RECIRCULATING"])
    passed = state_ok and fans_ok
    results.append(TestResult(
        test_id=tid, description=f"{combo_name} RECIRCULATING state",
        passed=passed,
        expected=f"State=RECIRCULATING E={exp['RECIRCULATING'][0]}% R={exp['RECIRCULATING'][1]}% H={exp['RECIRCULATING'][2]}%",
        actual=f"State={s.state} {fans_desc}",
        notes="" if passed else f"state_ok={state_ok}"
    ))
    print("PASS" if passed else "FAIL")

    # --- TC-X.3: HEATING (bed above threshold, wait MDT) ---
    tid = f"TC-{combo_name}.3"
    print(f"  {tid}: HEATING (waiting {MDT_WAIT_SEC}s for MDT)...", end=" ", flush=True)
    conn.send_and_read("set bed 65.0", settle=0.3)
    s = wait_for_state(conn, "HEATING", MDT_WAIT_SEC)
    state_ok = s.state == "HEATING"
    fans_ok, fans_desc = check_fans(s, *exp["HEATING"])
    passed = state_ok and fans_ok
    results.append(TestResult(
        test_id=tid, description=f"{combo_name} HEATING state",
        passed=passed,
        expected=f"State=HEATING E={exp['HEATING'][0]}% R={exp['HEATING'][1]}% H={exp['HEATING'][2]}%",
        actual=f"State={s.state} {fans_desc}",
        notes="" if passed else f"state_ok={state_ok}"
    ))
    print("PASS" if passed else "FAIL")

    # --- Return to IDLE for COOLING test ---
    conn.send_and_read("set bed 20.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    wait_for_state(conn, "IDLE", 10)

    # --- TC-X.4: COOLING (bed above RFSBT but below threshold, wait MDT) ---
    tid = f"TC-{combo_name}.4"
    print(f"  {tid}: COOLING (waiting {MDT_WAIT_SEC}s for MDT)...", end=" ", flush=True)
    conn.send_and_read("set bed 50.0", settle=0.3)
    conn.send_and_read("set chamber 42.0", settle=0.3)  # above PID setpoint
    s = wait_for_state(conn, "COOLING", MDT_WAIT_SEC)
    state_ok = s.state == "COOLING"
    fans_ok, fans_desc = check_fans(s, *exp["COOLING"])
    passed = state_ok and fans_ok
    results.append(TestResult(
        test_id=tid, description=f"{combo_name} COOLING state",
        passed=passed,
        expected=f"State=COOLING E={'PID' if exp['COOLING'][3] else exp['COOLING'][0]}% R={exp['COOLING'][1]}% H={exp['COOLING'][2]}%",
        actual=f"State={s.state} {fans_desc}",
        notes="" if passed else f"state_ok={state_ok}"
    ))
    print("PASS" if passed else "FAIL")

    # --- TC-X.5: Return to IDLE ---
    tid = f"TC-{combo_name}.5"
    print(f"  {tid}: Return to IDLE...", end=" ", flush=True)
    conn.send_and_read("set bed 20.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = wait_for_state(conn, "IDLE", 10)
    state_ok = s.state == "IDLE"
    fans_ok, fans_desc = check_fans(s, *exp["IDLE"])
    passed = state_ok and fans_ok
    results.append(TestResult(
        test_id=tid, description=f"{combo_name} return to IDLE",
        passed=passed,
        expected="State=IDLE E=0% R=0% H=0%",
        actual=f"State={s.state} {fans_desc}",
        notes="" if passed else f"state_ok={state_ok}"
    ))
    print("PASS" if passed else "FAIL")

    return results


# ---------------------------------------------------------------------------
# Additional tests
# ---------------------------------------------------------------------------

def test_manual_override(conn: SerialConnection, mdt: int) -> list[TestResult]:
    """TC-MANUAL: Manual fan override."""
    results = []
    print("\n--- Manual Fan Override ---")

    # Setup with all fans present, get into HEATING
    setup_common(conn, mdt)
    set_fan_presence(conn, True, True, True)
    conn.send_and_read("set bed 65.0", settle=0.3)
    wait_for_state(conn, "HEATING", MDT_WAIT_SEC)

    # Enable manual override — set speeds first, then enable manual mode,
    # then wait for the state machine to pick it up (runs every 500ms)
    tid = "TC-MANUAL.1"
    print(f"  {tid}: Manual ON with custom speeds...", end=" ", flush=True)
    conn.send_and_read("set heating 80", settle=0.3)
    conn.send_and_read("set exhaust 60", settle=0.3)
    conn.send_and_read("set recirc 40", settle=0.3)
    conn.send_and_read("set manual on", settle=0.3)
    time.sleep(SETTLE_SEC + 2)  # extra time for SM to enter MANUAL and apply speeds
    s = get_status(conn)
    fans_ok = (s.exhaust_pct == 60 and s.recirc_pct == 40 and s.heating_pct == 80)
    manual_ok = s.manual_fans == "ON"
    state_ok = s.state == "MANUAL"
    passed = fans_ok and manual_ok and state_ok
    results.append(TestResult(
        test_id=tid, description="Manual override active",
        passed=passed,
        expected="State=MANUAL E=60% R=40% H=80% Manual=ON",
        actual=f"State={s.state} E={s.exhaust_pct}% R={s.recirc_pct}% H={s.heating_pct}% Manual={s.manual_fans}",
    ))
    print("PASS" if passed else "FAIL")

    # Disable manual and verify state machine resumes
    tid = "TC-MANUAL.2"
    print(f"  {tid}: Manual OFF, state machine resumes...", end=" ", flush=True)
    conn.send_and_read("set manual off", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    manual_ok = s.manual_fans == "OFF"
    # After manual off, should go to IDLE (since mode goes back to auto with bed at 65)
    # Actually bed is 65 and mode is auto, so it depends on state
    # The state machine re-enters IDLE when manual is turned off, then bed > rfsbt triggers recirc
    passed = manual_ok and s.state in ("IDLE", "RECIRCULATING", "HEATING")
    results.append(TestResult(
        test_id=tid, description="Manual override disabled, SM resumes",
        passed=passed,
        expected="Manual=OFF, State=IDLE/RECIRCULATING/HEATING",
        actual=f"Manual={s.manual_fans} State={s.state}",
    ))
    print("PASS" if passed else "FAIL")

    return results


def test_forced_modes(conn: SerialConnection, mdt: int) -> list[TestResult]:
    """TC-FORCED: Forced HEATING and COOLING modes bypass MDT."""
    results = []
    print("\n--- Forced Modes ---")

    setup_common(conn, mdt)
    set_fan_presence(conn, True, True, True)

    # Forced HEATING
    tid = "TC-FORCED.1"
    print(f"  {tid}: Forced HEATING (no MDT wait)...", end=" ", flush=True)
    conn.send_and_read("set mode heating", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.state == "HEATING"
    results.append(TestResult(
        test_id=tid, description="Forced HEATING immediate entry",
        passed=passed,
        expected="State=HEATING",
        actual=f"State={s.state}",
    ))
    print("PASS" if passed else "FAIL")

    # Forced HEATING fan speeds
    tid = "TC-FORCED.2"
    print(f"  {tid}: Forced HEATING fan speeds...", end=" ", flush=True)
    fans_ok, fans_desc = check_fans(s, 15, 50, 50)
    results.append(TestResult(
        test_id=tid, description="Forced HEATING fan speeds (EHR)",
        passed=fans_ok,
        expected="E=15% R=50% H=50%",
        actual=fans_desc,
    ))
    print("PASS" if fans_ok else "FAIL")

    # Forced COOLING
    tid = "TC-FORCED.3"
    print(f"  {tid}: Forced COOLING (no MDT wait)...", end=" ", flush=True)
    conn.send_and_read("set mode cooling", settle=0.3)
    conn.send_and_read("set chamber 42.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.state == "COOLING"
    results.append(TestResult(
        test_id=tid, description="Forced COOLING immediate entry",
        passed=passed,
        expected="State=COOLING",
        actual=f"State={s.state}",
    ))
    print("PASS" if passed else "FAIL")

    # Forced COOLING fan speeds (PID active with chamber > setpoint)
    tid = "TC-FORCED.4"
    print(f"  {tid}: Forced COOLING fan speeds...", end=" ", flush=True)
    fans_ok, fans_desc = check_fans(s, 0, 20, 0, e_is_pid=True)
    results.append(TestResult(
        test_id=tid, description="Forced COOLING fan speeds (EHR, PID)",
        passed=fans_ok,
        expected="E=PID(15-100)% R=20% H=0%",
        actual=fans_desc,
    ))
    print("PASS" if fans_ok else "FAIL")

    # Return to AUTO
    tid = "TC-FORCED.5"
    print(f"  {tid}: Return to AUTO (bed cold -> IDLE)...", end=" ", flush=True)
    conn.send_and_read("set mode auto", settle=0.3)
    conn.send_and_read("set bed 20.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.state == "IDLE"
    results.append(TestResult(
        test_id=tid, description="Return to AUTO, bed cold -> IDLE",
        passed=passed,
        expected="State=IDLE",
        actual=f"State={s.state}",
    ))
    print("PASS" if passed else "FAIL")

    return results


def test_edge_cases(conn: SerialConnection, mdt: int) -> list[TestResult]:
    """TC-EDGE: State transition edge cases."""
    results = []
    print("\n--- Edge Cases ---")

    setup_common(conn, mdt)
    set_fan_presence(conn, True, True, True)

    # Edge: In AUTO HEATING, bed drops below RFSBT -> IDLE immediately
    # (AUTO mode checks bedTemp < RFSBT every tick, no MDT gate)
    tid = "TC-EDGE.1"
    print(f"  {tid}: AUTO HEATING -> bed drops below RFSBT -> IDLE...",
          end=" ", flush=True)
    # Get into HEATING via AUTO: bed above threshold, wait MDT
    conn.send_and_read("set mode auto", settle=0.3)
    conn.send_and_read("set bed 65.0", settle=0.3)
    s = wait_for_state(conn, "HEATING", MDT_WAIT_SEC)
    if s.state != "HEATING":
        results.append(TestResult(tid, "Precondition: in HEATING", False,
                                  "State=HEATING", f"State={s.state}"))
        print("FAIL (precondition)")
    else:
        conn.send_and_read("set bed 20.0", settle=0.3)
        time.sleep(SETTLE_SEC)
        s = get_status(conn)
        passed = s.state == "IDLE"
        results.append(TestResult(
            test_id=tid,
            description="AUTO HEATING: bed drops below RFSBT -> IDLE",
            passed=passed,
            expected="State=IDLE",
            actual=f"State={s.state}",
        ))
        print("PASS" if passed else "FAIL")

    # Edge: In AUTO COOLING, bed drops below RFSBT -> IDLE immediately
    tid = "TC-EDGE.2"
    print(f"  {tid}: AUTO COOLING -> bed drops below RFSBT -> IDLE...",
          end=" ", flush=True)
    conn.send_and_read("set mode auto", settle=0.3)
    conn.send_and_read("set bed 50.0", settle=0.3)
    conn.send_and_read("set chamber 42.0", settle=0.3)
    s = wait_for_state(conn, "COOLING", MDT_WAIT_SEC)
    if s.state != "COOLING":
        results.append(TestResult(tid, "Precondition: in COOLING", False,
                                  "State=COOLING", f"State={s.state}"))
        print("FAIL (precondition)")
    else:
        conn.send_and_read("set bed 20.0", settle=0.3)
        time.sleep(SETTLE_SEC)
        s = get_status(conn)
        passed = s.state == "IDLE"
        results.append(TestResult(
            test_id=tid,
            description="AUTO COOLING: bed drops below RFSBT -> IDLE",
            passed=passed,
            expected="State=IDLE",
            actual=f"State={s.state}",
        ))
        print("PASS" if passed else "FAIL")

    # Edge: IDLE -> bed rises above RFSBT -> RECIRC
    tid = "TC-EDGE.3"
    print(f"  {tid}: IDLE -> bed above RFSBT -> RECIRC...", end=" ", flush=True)
    conn.send_and_read("set mode auto", settle=0.3)
    conn.send_and_read("set bed 20.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    conn.send_and_read("set bed 50.0", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.state == "RECIRCULATING"
    results.append(TestResult(
        test_id=tid, description="IDLE: bed above RFSBT -> RECIRCULATING",
        passed=passed,
        expected="State=RECIRCULATING",
        actual=f"State={s.state}",
    ))
    print("PASS" if passed else "FAIL")

    return results


def test_settings_roundtrip(conn: SerialConnection) -> list[TestResult]:
    """TC-SETTINGS: Verify set/get roundtrip for key parameters."""
    results = []
    print("\n--- Settings Round-trip ---")

    # Each test: (id, set_command, search_pattern_regex, expected_value_pattern)
    # Using regex patterns to match specific lines unambiguously
    tests = [
        ("TC-SET.1", "set debug on",     r"^\s*debug\s*:\s*(\S+)",           "on"),
        ("TC-SET.2", "set debug off",    r"^\s*debug\s*:\s*(\S+)",           "off"),
        ("TC-SET.3", "set mdt 5",        r"^\s*mdt\s*:\s*(\d+)",            "5"),
        ("TC-SET.4", "set rfsbt 50",     r"^\s*rfsbt\s*:\s*(\d+)",          "50"),
        ("TC-SET.5", "set threshold 70", r"^\s*threshold\s*:\s*(\d+)",      "70"),
        ("TC-SET.6", "set hfp off",      r"^\s*heating fan\s*:\s*(yes|no)", "no"),
        ("TC-SET.7", "set hfp on",       r"^\s*heating fan\s*:\s*(yes|no)", "yes"),
        ("TC-SET.8", "set efp off",      r"^\s*exhaust fan\s*:\s*(yes|no)", "no"),
        ("TC-SET.9", "set efp on",       r"^\s*exhaust fan\s*:\s*(yes|no)", "yes"),
        ("TC-SET.10", "set rfp off",     r"^\s*recirc fan\s*:\s*(yes|no)",  "no"),
        ("TC-SET.11", "set rfp on",      r"^\s*recirc fan\s*:\s*(yes|no)",  "yes"),
        ("TC-SET.12", "set light on",    r"^\s*light\s*:\s*(\S+)",          "on"),
        ("TC-SET.13", "set light off",   r"^\s*light\s*:\s*(\S+)",          "off"),
    ]

    for tid, cmd, pattern, expected_val in tests:
        print(f"  {tid}: {cmd}...", end=" ", flush=True)
        conn.send_and_read(cmd, settle=0.5)
        # Read settings dump
        get_lines = conn.send_and_read("get", settle=0.5, timeout=3.0)

        # Search for matching line using regex
        found = False
        actual_line = ""
        actual_val = ""
        for line in get_lines:
            m = re.match(pattern, line)
            if m:
                actual_line = line.strip()
                actual_val = m.group(1)
                if actual_val == expected_val:
                    found = True
                break

        results.append(TestResult(
            test_id=tid, description=f"Settings: {cmd}",
            passed=found,
            expected=f"value='{expected_val}'",
            actual=actual_line if actual_line else f"pattern '{pattern}' not found in get output",
        ))
        print("PASS" if found else "FAIL")

    # Restore defaults
    conn.send_and_read("set mdt 1", settle=0.3)
    conn.send_and_read("set rfsbt 45", settle=0.3)
    conn.send_and_read("set threshold 60", settle=0.3)
    conn.send_and_read("set hfp on", settle=0.3)
    conn.send_and_read("set efp on", settle=0.3)
    conn.send_and_read("set rfp on", settle=0.3)

    return results


def test_chamber_light(conn: SerialConnection) -> list[TestResult]:
    """TC-LIGHT: Chamber light toggle."""
    results = []
    print("\n--- Chamber Light ---")

    tid = "TC-LIGHT.1"
    print(f"  {tid}: Light ON...", end=" ", flush=True)
    conn.send_and_read("set light on", settle=0.5)
    s = get_status(conn)
    passed = s.chamber_light == "ON"
    results.append(TestResult(tid, "Light ON", passed, "Light=ON", f"Light={s.chamber_light}"))
    print("PASS" if passed else "FAIL")

    tid = "TC-LIGHT.2"
    print(f"  {tid}: Light OFF...", end=" ", flush=True)
    conn.send_and_read("set light off", settle=0.5)
    s = get_status(conn)
    passed = s.chamber_light == "OFF"
    results.append(TestResult(tid, "Light OFF", passed, "Light=OFF", f"Light={s.chamber_light}"))
    print("PASS" if passed else "FAIL")

    return results


def test_fan_presence_effect_on_absent_fans(conn: SerialConnection, mdt: int) -> list[TestResult]:
    """TC-ABSENT: Verify absent fans always report 0% in all states."""
    results = []
    print("\n--- Absent Fan Speed Verification ---")

    # Use forced modes to quickly check absent fans stay at 0
    setup_common(conn, mdt)

    # Only exhaust present — heating and recirc must always be 0
    set_fan_presence(conn, True, False, False)

    tid = "TC-ABSENT.1"
    print(f"  {tid}: E-only, forced HEATING, H+R must be 0...", end=" ", flush=True)
    conn.send_and_read("set mode heating", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.recirc_pct == 0 and s.heating_pct == 0
    results.append(TestResult(tid, "E-only: absent fans at 0 in HEATING", passed,
                              "R=0% H=0%", f"R={s.recirc_pct}% H={s.heating_pct}%"))
    print("PASS" if passed else "FAIL")

    tid = "TC-ABSENT.2"
    print(f"  {tid}: E-only, forced COOLING, H+R must be 0...", end=" ", flush=True)
    conn.send_and_read("set mode cooling", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.recirc_pct == 0 and s.heating_pct == 0
    results.append(TestResult(tid, "E-only: absent fans at 0 in COOLING", passed,
                              "R=0% H=0%", f"R={s.recirc_pct}% H={s.heating_pct}%"))
    print("PASS" if passed else "FAIL")

    # Only heating present — exhaust and recirc must always be 0
    set_fan_presence(conn, False, True, False)

    tid = "TC-ABSENT.3"
    print(f"  {tid}: H-only, forced HEATING, E+R must be 0...", end=" ", flush=True)
    conn.send_and_read("set mode heating", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.exhaust_pct == 0 and s.recirc_pct == 0
    results.append(TestResult(tid, "H-only: absent fans at 0 in HEATING", passed,
                              "E=0% R=0%", f"E={s.exhaust_pct}% R={s.recirc_pct}%"))
    print("PASS" if passed else "FAIL")

    tid = "TC-ABSENT.4"
    print(f"  {tid}: H-only, forced COOLING, all must be 0...", end=" ", flush=True)
    conn.send_and_read("set mode cooling", settle=0.3)
    time.sleep(SETTLE_SEC)
    s = get_status(conn)
    passed = s.exhaust_pct == 0 and s.recirc_pct == 0 and s.heating_pct == 0
    results.append(TestResult(tid, "H-only: all fans at 0 in COOLING", passed,
                              "E=0% R=0% H=0%",
                              f"E={s.exhaust_pct}% R={s.recirc_pct}% H={s.heating_pct}%"))
    print("PASS" if passed else "FAIL")

    # Restore
    conn.send_and_read("set mode auto", settle=0.3)
    set_fan_presence(conn, True, True, True)

    return results


# ---------------------------------------------------------------------------
# Report generation
# ---------------------------------------------------------------------------
def generate_report(all_results: list[TestResult], output_path: str):
    """Write a markdown test report."""
    total = len(all_results)
    passed = sum(1 for r in all_results if r.passed)
    failed = total - passed

    with open(output_path, "w") as f:
        f.write(f"# Fan Controller Test Report\n\n")
        f.write(f"**Date:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write(f"**Total:** {total} | **Passed:** {passed} | **Failed:** {failed}\n\n")

        if failed > 0:
            f.write("## Failed Tests\n\n")
            f.write("| # | Test ID | Description | Expected | Actual | Notes |\n")
            f.write("|---|---------|-------------|----------|--------|-------|\n")
            for i, r in enumerate(all_results):
                if not r.passed:
                    f.write(f"| {i+1} | {r.test_id} | {r.description} | {r.expected} | {r.actual} | {r.notes} |\n")
            f.write("\n")

        f.write("## All Results\n\n")
        f.write("| # | Test ID | Description | Result | Expected | Actual |\n")
        f.write("|---|---------|-------------|--------|----------|--------|\n")
        for i, r in enumerate(all_results):
            status = "PASS" if r.passed else "**FAIL**"
            f.write(f"| {i+1} | {r.test_id} | {r.description} | {status} | {r.expected} | {r.actual} |\n")

    print(f"\nReport saved to: {output_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="FanController2 automated test runner")
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("--mdt", type=int, default=MDT_MINUTES, help=f"MDT in minutes (default: {MDT_MINUTES})")
    parser.add_argument("--skip-combos", action="store_true", help="Skip the 7-combo matrix (slow)")
    args = parser.parse_args()

    print(f"FanController2 Test Runner")
    print(f"Port: {args.port}  MDT: {args.mdt} min")
    print(f"{'='*60}")

    conn = SerialConnection(args.port)
    all_results: list[TestResult] = []

    try:
        # Quick tests first
        all_results.extend(test_settings_roundtrip(conn))
        all_results.extend(test_chamber_light(conn))
        all_results.extend(test_forced_modes(conn, args.mdt))
        all_results.extend(test_edge_cases(conn, args.mdt))
        all_results.extend(test_manual_override(conn, args.mdt))
        all_results.extend(test_fan_presence_effect_on_absent_fans(conn, args.mdt))

        # Full combo matrix (slow — 7 combos x MDT waits)
        if not args.skip_combos:
            for combo_name, exhaust, heating, recirc in FAN_COMBOS:
                combo_results = run_combo_tests(conn, combo_name, exhaust, heating, recirc, args.mdt)
                all_results.extend(combo_results)
        else:
            print("\n--- Skipping fan combo matrix (--skip-combos) ---")

    finally:
        # Restore safe state
        try:
            setup_common(conn, args.mdt)
            set_fan_presence(conn, True, True, True)
        except Exception:
            pass
        conn.close()

    # Summary
    total = len(all_results)
    passed = sum(1 for r in all_results if r.passed)
    failed = total - passed

    print(f"\n{'='*60}")
    print(f"TOTAL: {total}  PASSED: {passed}  FAILED: {failed}")
    print(f"{'='*60}")

    # Generate report
    timestamp = datetime.now().strftime("%Y-%m-%d-%H%M%S")
    report_path = f"docs/testing/test-results-{timestamp}.md"
    generate_report(all_results, report_path)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
