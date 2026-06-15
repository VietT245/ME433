"""
collect_and_plot.py

Waits 1 second after launch, then records encoder angle and force data
from the STM32 over USB serial for 7 seconds, then plots the results.

Streams CSV from STM32:
    timestamp_ms,angle_deg,force_raw

Usage:
    pip install pyserial matplotlib
    python collect_and_plot.py
"""

import serial
import matplotlib.pyplot as plt
import time

# ------------------------------------------------------------------ #
#  CONFIG — adjust to match your setup                                #
# ------------------------------------------------------------------ #
SERIAL_PORT     = "/dev/tty.usbmodem2102"
BAUD_RATE       = 115200
WAIT_SEC        = 1.0    # seconds to wait before recording starts
RECORD_SEC      = 7.0    # seconds to record data

# Optional: set to a float to convert raw HX711 counts to Newtons
FORCE_SCALE     = None   # None = plot raw counts

# ------------------------------------------------------------------ #
#  Open serial port                                                   #
# ------------------------------------------------------------------ #
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    print(f"Opened {SERIAL_PORT} at {BAUD_RATE} baud")
except serial.SerialException as e:
    print(f"Could not open serial port: {e}")
    print("Edit SERIAL_PORT at the top of this script.")
    raise SystemExit(1)

# ------------------------------------------------------------------ #
#  Wait before recording                                              #
# ------------------------------------------------------------------ #
print(f"Waiting {WAIT_SEC}s before recording...")
time.sleep(WAIT_SEC)
ser.reset_input_buffer()

# ------------------------------------------------------------------ #
#  Record for RECORD_SEC seconds                                      #
# ------------------------------------------------------------------ #
times   = []
angles  = []
forces  = []

t_start = None
print(f"Recording for {RECORD_SEC}s — move the handle now!")

record_start = time.time()

while (time.time() - record_start) < RECORD_SEC:
    if ser.in_waiting:
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
        except Exception:
            continue

        if not line or line.startswith("timestamp"):
            continue

        parts = line.split(",")
        if len(parts) != 3:
            continue

        try:
            ts_ms     = int(parts[0])
            angle_deg = float(parts[1])
            force_raw = int(parts[2])
        except ValueError:
            continue

        if t_start is None:
            t_start = ts_ms

        t_sec     = (ts_ms - t_start) / 1000.0
        force_val = (force_raw * FORCE_SCALE) if FORCE_SCALE else force_raw

        times.append(t_sec)
        angles.append(angle_deg)
        forces.append(force_val)

ser.close()

print(f"Done! Collected {len(times)} samples over {RECORD_SEC}s")
if len(times) > 1:
    actual_rate = len(times) / (times[-1] - times[0]) if times[-1] > times[0] else 0
    print(f"Effective sample rate: {actual_rate:.1f} Hz")

# ------------------------------------------------------------------ #
#  Plot — handles constant/missing data gracefully                    #
# ------------------------------------------------------------------ #
if not times:
    print("No data received — check your serial port and STM32 connection.")
    raise SystemExit(1)

fig, (ax_angle, ax_force) = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
fig.suptitle(f"Encoder & Force — {RECORD_SEC}s Capture", fontsize=13)

# --- Angle ---
ax_angle.plot(times, angles, color="#2196F3", linewidth=1.5)
ax_angle.set_ylabel("Angle (°)")
ax_angle.grid(True, alpha=0.3)

a_min, a_max = min(angles), max(angles)
a_range = a_max - a_min
# Add padding so a flat line is still visible
ax_angle.set_ylim(a_min - max(a_range * 0.1, 5), a_max + max(a_range * 0.1, 5))
ax_angle.set_title(
    f"Min: {a_min:.1f}°  Max: {a_max:.1f}°  Range: {a_range:.1f}°"
    + ("  (AS5600 not detected — check wiring)" if a_range < 0.1 else ""),
    fontsize=9
)

# --- Force ---
force_label = "Force (N)" if FORCE_SCALE else "Force (raw counts)"
ax_force.plot(times, forces, color="#E91E63", linewidth=1.5)
ax_force.set_ylabel(force_label)
ax_force.set_xlabel("Time (s)")
ax_force.grid(True, alpha=0.3)

f_min, f_max = min(forces), max(forces)
f_range = f_max - f_min
# Add padding so a flat line is still visible
ax_force.set_ylim(f_min - max(f_range * 0.1, 1), f_max + max(f_range * 0.1, 1))
ax_force.set_title(
    f"Min: {f_min:,.0f}  Max: {f_max:,.0f}  Range: {f_range:,.0f}"
    + ("  (HX711 not responding — check wiring)" if f_range == 0 else ""),
    fontsize=9
)

plt.tight_layout()
plt.show()