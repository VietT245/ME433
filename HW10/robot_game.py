"""
robot_game.py  –  Pygame Zero robot obstacle course
Controlled by a Raspberry Pi Pico + MPU-6050 over USB serial.

Run with:
    pgzrun robot_game.py

Dependencies:
    pip install pgzero pyserial

Controls (tilt breadboard):
    Tilt forward  (−Y accel)  → robot moves forward
    Tilt back     (+Y accel)  → robot moves backward
    Tilt left     (−X accel)  → robot steers left
    Tilt right    (+X accel)  → robot steers right
    Button (GP0)              → restart after game over

Serial protocol expected from Pico:
    A,<ax>,<ay>,<az>,<btn>\\n   (50 Hz)
"""

import math
import random
import threading
import serial
import serial.tools.list_ports

# ── Window ────────────────────────────────────────────────────────────────────
WIDTH  = 800
HEIGHT = 600
TITLE  = "Robot Navigator"

# ── Serial config ─────────────────────────────────────────────────────────────
BAUD        = 115200
SERIAL_PORT = None   # auto-detected below

# ── Game tuning ───────────────────────────────────────────────────────────────
TILT_DEAD   = 1500    # raw ADC units — ignore small tilts
TILT_MAX    = 14000   # raw units at full tilt (±2 g range → ±16384 at full)
ROBOT_SPEED = 180     # pixels per second at full forward tilt
STEER_SPEED = 140     # degrees per second at full side tilt
OBSTACLE_W  = 48
OBSTACLE_H  = 48
GOAL_R      = 28
NUM_OBS     = 12

# ── Colours ───────────────────────────────────────────────────────────────────
C_BG        = (15,  20,  35)
C_GRID      = (25,  35,  55)
C_ROBOT     = (80, 200, 120)
C_ROBOT_DIR = (200, 240, 100)
C_OBSTACLE  = (200,  60,  50)
C_GOAL      = (255, 210,   0)
C_HUD       = (200, 220, 255)
C_DEAD      = (220,  60,  60)
C_WIN       = (100, 230, 100)
C_SHADOW    = (0,    0,   0, 100)

# ─────────────────────────────────────────────────────────────────────────────
#  Serial reader  (background thread → shared state)
# ─────────────────────────────────────────────────────────────────────────────
class PicoReader:
    def __init__(self, port, baud):
        self.ax = 0
        self.ay = 0
        self.az = 0
        self.btn = 0
        self._lock = threading.Lock()
        self._running = True
        self._ser = None
        try:
            self._ser = serial.Serial(port, baud, timeout=1)
            print(f"[serial] Opened {port}")
        except Exception as e:
            print(f"[serial] Could not open {port}: {e}")
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while self._running:
            if self._ser is None:
                break
            try:
                line = self._ser.readline().decode(errors="replace").strip()
                if line.startswith("A,"):
                    parts = line.split(",")
                    if len(parts) == 5:
                        with self._lock:
                            self.ax  = int(parts[1])
                            self.ay  = int(parts[2])
                            self.az  = int(parts[3])
                            self.btn = int(parts[4])
            except Exception:
                pass

    def read(self):
        with self._lock:
            return self.ax, self.ay, self.az, self.btn

    def close(self):
        self._running = False
        if self._ser:
            self._ser.close()


def find_pico_port():
    """Try to auto-detect the Pico's USB-CDC port."""
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        mfr  = (p.manufacturer or "").lower()
        if "pico" in desc or "2040" in desc or "raspberry" in mfr or "usbmodem" in p.device:
            return p.device
    # fallback: first tty.usbmodem on macOS / ttyACM on Linux
    for p in serial.tools.list_ports.comports():
        if "usbmodem" in p.device or "ttyACM" in p.device:
            return p.device
    return None


# ── Initialise serial ─────────────────────────────────────────────────────────
_port = find_pico_port()
if _port:
    print(f"[serial] Auto-detected Pico on {_port}")
else:
    # last-resort guess
    import sys
    _port = "/dev/tty.usbmodem2101" if sys.platform == "darwin" else "/dev/ttyACM0"
    print(f"[serial] Could not auto-detect — trying {_port}")

pico = PicoReader(_port, BAUD)


# ─────────────────────────────────────────────────────────────────────────────
#  Utility
# ─────────────────────────────────────────────────────────────────────────────
def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def norm_tilt(raw):
    """Map raw accel to [-1, 1] with dead zone."""
    if abs(raw) < TILT_DEAD:
        return 0.0
    sign = 1 if raw > 0 else -1
    return sign * clamp((abs(raw) - TILT_DEAD) / (TILT_MAX - TILT_DEAD), 0.0, 1.0)

def circles_overlap(x1, y1, r1, x2, y2, r2):
    dx, dy = x1 - x2, y1 - y2
    return math.hypot(dx, dy) < r1 + r2

def rect_circle_overlap(rx, ry, rw, rh, cx, cy, cr):
    """AABB vs circle."""
    nearest_x = clamp(cx, rx - rw/2, rx + rw/2)
    nearest_y = clamp(cy, ry - rh/2, ry + rh/2)
    return math.hypot(cx - nearest_x, cy - nearest_y) < cr


# ─────────────────────────────────────────────────────────────────────────────
#  Game state
# ─────────────────────────────────────────────────────────────────────────────
ROBOT_R = 18   # collision radius

class GameState:
    def __init__(self):
        self.reset()

    def reset(self):
        self.rx   = 80.0          # robot x
        self.ry   = HEIGHT / 2.0  # robot y
        self.angle = 0.0          # degrees, 0 = right
        self.alive = True
        self.won   = False
        self.score = 0
        self.time  = 0.0
        # Goal in opposite corner area
        self.gx = random.randint(WIDTH - 120, WIDTH - 60)
        self.gy = random.randint(60, HEIGHT - 60)
        # Obstacles — keep clear of start and goal
        self.obstacles = []
        attempts = 0
        while len(self.obstacles) < NUM_OBS and attempts < 500:
            attempts += 1
            ox = random.randint(OBSTACLE_W, WIDTH  - OBSTACLE_W)
            oy = random.randint(OBSTACLE_H, HEIGHT - OBSTACLE_H)
            # Don't overlap start zone
            if math.hypot(ox - self.rx, oy - self.ry) < 80:
                continue
            # Don't overlap goal
            if math.hypot(ox - self.gx, oy - self.gy) < 70:
                continue
            self.obstacles.append((ox, oy))

state = GameState()


# ─────────────────────────────────────────────────────────────────────────────
#  Draw helpers (all use pgzero's `screen`)
# ─────────────────────────────────────────────────────────────────────────────
def draw_grid():
    for x in range(0, WIDTH, 40):
        screen.draw.line((x, 0), (x, HEIGHT), C_GRID)
    for y in range(0, HEIGHT, 40):
        screen.draw.line((0, y), (WIDTH, y), C_GRID)

def draw_goal():
    # Pulsing ring
    pulse = 0.7 + 0.3 * math.sin(state.time * 4)
    r = int(GOAL_R * pulse)
    screen.draw.filled_circle((int(state.gx), int(state.gy)), r, C_GOAL)
    screen.draw.circle((int(state.gx), int(state.gy)), GOAL_R + 6, C_GOAL)

def draw_obstacles():
    for ox, oy in state.obstacles:
        hw, hh = OBSTACLE_W // 2, OBSTACLE_H // 2
        screen.draw.filled_rect(
            Rect(ox - hw, oy - hh, OBSTACLE_W, OBSTACLE_H), C_OBSTACLE)
        screen.draw.rect(
            Rect(ox - hw, oy - hh, OBSTACLE_W, OBSTACLE_H), (240, 100, 80))

def draw_robot():
    rx, ry = int(state.rx), int(state.ry)
    # Body
    screen.draw.filled_circle((rx, ry), ROBOT_R, C_ROBOT)
    screen.draw.circle((rx, ry), ROBOT_R, (150, 255, 180))
    # Direction indicator
    rad = math.radians(state.angle)
    tip_x = rx + int((ROBOT_R + 8) * math.cos(rad))
    tip_y = ry + int((ROBOT_R + 8) * math.sin(rad))
    screen.draw.line((rx, ry), (tip_x, tip_y), C_ROBOT_DIR)
    screen.draw.filled_circle((tip_x, tip_y), 5, C_ROBOT_DIR)

def draw_hud():
    ax, ay, az, btn = pico.read()
    tilt_x = norm_tilt(ax)
    tilt_y = norm_tilt(ay)
    screen.draw.text(
        f"Time: {state.time:.1f}s   Score: {state.score}",
        (10, 10), color=C_HUD, fontsize=22)
    screen.draw.text(
        f"Tilt X: {tilt_x:+.2f}   Tilt Y: {tilt_y:+.2f}   Btn: {btn}",
        (10, 36), color=C_HUD, fontsize=18)
    screen.draw.text(
        f"Raw ax={ax:+6d}  ay={ay:+6d}",
        (10, 58), color=(130, 150, 180), fontsize=16)

def draw_overlay(msg, color):
    # Semi-transparent panel
    screen.draw.filled_rect(Rect(WIDTH//2 - 200, HEIGHT//2 - 60, 400, 120),
                             (10, 10, 30))
    screen.draw.rect(Rect(WIDTH//2 - 200, HEIGHT//2 - 60, 400, 120), color)
    screen.draw.text(msg, center=(WIDTH//2, HEIGHT//2 - 20),
                     color=color, fontsize=36)
    screen.draw.text("Press button to restart",
                     center=(WIDTH//2, HEIGHT//2 + 20),
                     color=C_HUD, fontsize=20)


# ─────────────────────────────────────────────────────────────────────────────
#  pgzero hooks
# ─────────────────────────────────────────────────────────────────────────────
def draw():
    screen.fill(C_BG)
    draw_grid()
    draw_goal()
    draw_obstacles()
    draw_robot()
    draw_hud()

    if not state.alive:
        draw_overlay("CRASHED!", C_DEAD)
    elif state.won:
        draw_overlay(f"YOU WIN!  {state.time:.1f}s", C_WIN)


def update(dt):
    state.time += dt

    ax, ay, az, btn = pico.read()

    # Restart on button press when game is over
    if (not state.alive or state.won) and btn:
        state.reset()
        return

    if not state.alive or state.won:
        return

    tilt_fwd   = -norm_tilt(ay)   # forward = tilt board away from you (−Y)
    tilt_steer =  -norm_tilt(ax)   # right   = tilt board right (+X)

    # Steering changes heading
    state.angle += tilt_steer * STEER_SPEED * dt

    # Movement along current heading
    speed = tilt_fwd * ROBOT_SPEED
    rad   = math.radians(state.angle)
    new_x = state.rx + speed * math.cos(rad) * dt
    new_y = state.ry + speed * math.sin(rad) * dt

    # Wall clamping
    new_x = clamp(new_x, ROBOT_R, WIDTH  - ROBOT_R)
    new_y = clamp(new_y, ROBOT_R, HEIGHT - ROBOT_R)

    # Obstacle collision
    for ox, oy in state.obstacles:
        if rect_circle_overlap(ox, oy, OBSTACLE_W, OBSTACLE_H, new_x, new_y, ROBOT_R):
            state.alive = False
            return

    state.rx = new_x
    state.ry = new_y

    # Goal check
    if circles_overlap(state.rx, state.ry, ROBOT_R, state.gx, state.gy, GOAL_R):
        state.won   = True
        state.score += max(1, int(300 - state.time * 10))
