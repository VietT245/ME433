import time
import board
import pwmio

# Set up PWM on GP0 (adjust if needed)
pwm = pwmio.PWMOut(board.GP0, frequency=50)

# Convert microseconds to duty cycle
def us_to_duty_cycle(us):
    period_us = 20000  # 20 ms
    return int((us / period_us) * 65535)

while True:
    # Sweep forward
    for us in range(1000, 2001, 10):
        pwm.duty_cycle = us_to_duty_cycle(us)

        angle = (us - 1000) * (180.0 / 1000.0)
        print(f"Pulse: {us} us   Angle: {angle:.1f} degrees")

        time.sleep(0.02)

    # Sweep backward
    for us in range(2000, 999, -10):
        pwm.duty_cycle = us_to_duty_cycle(us)

        angle = (us - 1000) * (180.0 / 1000.0)
        print(f"Pulse: {us} us   Angle: {angle:.1f} degrees")

        time.sleep(0.02)