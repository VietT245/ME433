import argparse
import sys
import time

import numpy as np
import serial
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec


# ── Command-line arguments ────────────────────────────────────────────────────
def parse_args():
    p = argparse.ArgumentParser(description="HX711 force sensor host script")
    p.add_argument("--port",    default="/dev/tty.usbmodem101",
                   help="Serial port (default: /dev/tty.usbmodem101, Windows: COMx)")
    p.add_argument("--baud",    type=int, default=115200,
                   help="Baud rate (default: 115200)")
    p.add_argument("--samples", type=int, default=400,
                   help="Number of samples to collect (default: 400, ~5 s at 80 Hz)")
    return p.parse_args()


# ── Serial helpers ────────────────────────────────────────────────────────────
def open_port(port: str, baud: int) -> serial.Serial:
    try:
        ser = serial.Serial(port, baud, timeout=30)
        print(f"Opened {port} at {baud} baud.")
        time.sleep(0.5)          # let CDC enumerate
        ser.reset_input_buffer()
        return ser
    except serial.SerialException as exc:
        sys.exit(f"Could not open {port}: {exc}")


def collect_data(ser: serial.Serial, n_samples: int):
    """Send sample count, wait for BEGIN, read data, wait for END."""

    # Flush any stale bytes that arrived before we connected
    # (leftover data lines from a previous run, reset noise, etc.)
    ser.reset_input_buffer()
    time.sleep(0.1)
    ser.reset_input_buffer()

    # Send the sample count
    ser.write(f"{n_samples}\n".encode())
    ser.flush()
    print(f"Requested {n_samples} samples …")

    # Wait for BEGIN line — ignore anything that isn't it.
    # If the Pico was mid-transmission we'll see data lines first; skip them.
    begin_deadline = time.time() + 10   # give up after 10 s
    while True:
        if time.time() > begin_deadline:
            # Pico may not have seen our request (e.g. it was busy sending).
            # Flush and resend once.
            print("  [retry] No BEGIN yet — flushing and resending request …")
            ser.reset_input_buffer()
            time.sleep(0.2)
            ser.write(f"{n_samples}\n".encode())
            ser.flush()
            begin_deadline = time.time() + 15   # longer grace period on retry

        line = ser.readline().decode(errors="replace").strip()
        if not line:
            continue
        if line.startswith("BEGIN"):
            parts = line.split()
            reported = int(parts[1]) if len(parts) > 1 else n_samples
            print(f"Pico acknowledged {reported} samples. Receiving …")
            break
        # anything else (stale data lines, blank lines) is silently skipped

    raw_vals  = []
    filt_vals = []
    time_ms   = []

    while True:
        line = ser.readline().decode(errors="replace").strip()
        if line == "END":
            break
        if not line:
            continue
        parts = line.split(",")
        if len(parts) != 3:
            print(f"  [warn] unexpected line: {line!r}")
            continue
        raw_vals.append(int(parts[0]))
        filt_vals.append(int(parts[1]))
        time_ms.append(int(parts[2]))

    print(f"Received {len(raw_vals)} samples.")
    return (np.array(raw_vals,  dtype=np.float64),
            np.array(filt_vals, dtype=np.float64),
            np.array(time_ms,   dtype=np.float64))


# ── FFT helper ────────────────────────────────────────────────────────────────
def compute_fft(signal: np.ndarray, fs: float):
    """Return (frequencies, single-sided magnitude spectrum)."""
    n      = len(signal)
    window = np.hanning(n)
    # Remove DC so the 0 Hz spike doesn't dominate the plot
    sig_ac = (signal - signal.mean()) * window
    mag    = np.abs(np.fft.rfft(sig_ac)) * 2.0 / n
    freqs  = np.fft.rfftfreq(n, d=1.0 / fs)
    return freqs, mag


# ── Plotting ──────────────────────────────────────────────────────────────────
def plot_results(raw: np.ndarray,
                 filt: np.ndarray,
                 time_ms: np.ndarray):

    # Derive actual sample rate from timestamps
    dt_s = np.diff(time_ms) / 1000.0          # seconds between samples
    fs   = 1.0 / np.median(dt_s)
    print(f"Estimated sample rate: {fs:.1f} Hz  (Nyquist: {fs/2:.1f} Hz)")

    t_s = (time_ms - time_ms[0]) / 1000.0     # time axis in seconds

    freqs_raw,  mag_raw  = compute_fft(raw,  fs)
    freqs_filt, mag_filt = compute_fft(filt, fs)

    fig = plt.figure(figsize=(13, 8))
    fig.suptitle("HX711 Force Sensor  –  Raw vs IIR-Filtered", fontsize=14, fontweight="bold")
    gs  = gridspec.GridSpec(2, 2, figure=fig, hspace=0.45, wspace=0.35)

    # ── Time-domain: raw ──────────────────────────────────────────────────
    ax_raw = fig.add_subplot(gs[0, 0])
    ax_raw.plot(t_s, raw,  color="#1f77b4", linewidth=0.8, label="raw")
    ax_raw.set_title("Raw signal")
    ax_raw.set_xlabel("Time (s)")
    ax_raw.set_ylabel("ADC counts")
    ax_raw.grid(True, alpha=0.3)

    # ── Time-domain: filtered ─────────────────────────────────────────────
    ax_filt = fig.add_subplot(gs[0, 1])
    ax_filt.plot(t_s, filt, color="#ff7f0e", linewidth=0.8, label="filtered")
    ax_filt.set_title("IIR-filtered signal")
    ax_filt.set_xlabel("Time (s)")
    ax_filt.set_ylabel("ADC counts")
    ax_filt.grid(True, alpha=0.3)

    # ── Overlay ───────────────────────────────────────────────────────────
    ax_both = fig.add_subplot(gs[1, 0])
    ax_both.plot(t_s, raw,  color="#1f77b4", linewidth=0.6,
                 alpha=0.6, label="raw")
    ax_both.plot(t_s, filt, color="#ff7f0e", linewidth=1.2,
                 alpha=0.9, label="filtered")
    ax_both.set_title("Overlay")
    ax_both.set_xlabel("Time (s)")
    ax_both.set_ylabel("ADC counts")
    ax_both.legend(fontsize=8)
    ax_both.grid(True, alpha=0.3)

    # ── FFT comparison ────────────────────────────────────────────────────
    ax_fft = fig.add_subplot(gs[1, 1])
    ax_fft.semilogy(freqs_raw,  mag_raw,  color="#1f77b4", linewidth=0.8,
                    alpha=0.8, label="raw FFT")
    ax_fft.semilogy(freqs_filt, mag_filt, color="#ff7f0e", linewidth=1.1,
                    alpha=0.9, label="filtered FFT")
    ax_fft.axvline(x=fs / 2, color="gray", linestyle="--",
                   linewidth=0.8, label=f"Nyquist ({fs/2:.0f} Hz)")
    # Annotate the 25-30 Hz noise band
    ax_fft.axvspan(25, 30, alpha=0.12, color="red", label="noise band (25–30 Hz)")
    ax_fft.set_title("FFT (single-sided, Hanning window)")
    ax_fft.set_xlabel("Frequency (Hz)")
    ax_fft.set_ylabel("Magnitude (counts)")
    ax_fft.set_xlim(0, fs / 2 + 2)
    ax_fft.legend(fontsize=7)
    ax_fft.grid(True, which="both", alpha=0.3)

    plt.tight_layout()
    plt.savefig("force_sensor_data.png", dpi=150)
    print("Plot saved to force_sensor_data.png")
    plt.show()


# ── Entry point ───────────────────────────────────────────────────────────────
def main():
    args = parse_args()

    ser  = open_port(args.port, args.baud)
    raw, filt, time_ms = collect_data(ser, args.samples)
    ser.close()

    if len(raw) == 0:
        sys.exit("No data received. Check wiring and firmware.")

    plot_results(raw, filt, time_ms)


if __name__ == "__main__":
    main()
