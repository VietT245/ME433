#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// ── Pin / address definitions ────────────────────────────────────────────────
#define I2C_SDA         4
#define I2C_SCL         5
#define HEARTBEAT_LED   7
#define BUTTON_PIN      0

#define MPU6050_ADDR    0x68
#define CONFIG          0x1A
#define GYRO_CONFIG     0x1B
#define ACCEL_CONFIG    0x1C
#define PWR_MGMT_1      0x6B
#define WHO_AM_I        0x75
#define ACCEL_XOUT_H    0x3B

// ── I2C helpers ──────────────────────────────────────────────────────────────
static void mpu_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c0, MPU6050_ADDR, buf, 2, false);
}

static void mpu_read(uint8_t reg, uint8_t *dst, size_t len) {
    i2c_write_blocking(i2c0, MPU6050_ADDR, &reg, 1, true);   // keep bus
    i2c_read_blocking (i2c0, MPU6050_ADDR, dst, len, false);
}

// ── MPU-6050 initialisation ──────────────────────────────────────────────────
static bool mpu6050_init(void) {
    // Wake the device (clear sleep bit)
    mpu_write(PWR_MGMT_1, 0x00);
    sleep_ms(100);

    // Verify WHO_AM_I
    uint8_t who = 0;
    mpu_read(WHO_AM_I, &who, 1);
    if (who != 0x68) return false;   // not found

    // Low-pass filter: ~44 Hz BW (DLPF_CFG = 3)
    mpu_write(CONFIG, 0x03);

    // Accel full-scale: ±2 g  (AFS_SEL = 0)
    mpu_write(ACCEL_CONFIG, 0x00);

    // Gyro full-scale: ±250 °/s (FS_SEL = 0) – not used for steering but init anyway
    mpu_write(GYRO_CONFIG, 0x00);

    return true;
}

// ── Read all 3 accel axes ────────────────────────────────────────────────────
static void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t raw[6];
    mpu_read(ACCEL_XOUT_H, raw, 6);
    *ax = (int16_t)((raw[0] << 8) | raw[1]);
    *ay = (int16_t)((raw[2] << 8) | raw[3]);
    *az = (int16_t)((raw[4] << 8) | raw[5]);
}

// ── Main ─────────────────────────────────────────────────────────────────────
int main(void) {
    stdio_init_all();

    // I2C at 400 kHz
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Heartbeat LED
    gpio_init(HEARTBEAT_LED);
    gpio_set_dir(HEARTBEAT_LED, GPIO_OUT);

    // Button (active-low)
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    sleep_ms(2000);   // wait for USB CDC to enumerate

    // Init MPU — blink fast if not found
    while (!mpu6050_init()) {
        gpio_put(HEARTBEAT_LED, 1); sleep_ms(100);
        gpio_put(HEARTBEAT_LED, 0); sleep_ms(100);
    }

    uint32_t last_tx  = 0;
    uint32_t last_hb  = 0;
    bool     led_state = false;
    const uint32_t TX_INTERVAL_MS = 20;   // 50 Hz

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Heartbeat: toggle LED every 500 ms
        if (now - last_hb >= 500) {
            led_state = !led_state;
            gpio_put(HEARTBEAT_LED, led_state);
            last_hb = now;
        }

        // Stream at 50 Hz
        if (now - last_tx >= TX_INTERVAL_MS) {
            int16_t ax, ay, az;
            mpu6050_read_accel(&ax, &ay, &az);
            uint8_t btn = gpio_get(BUTTON_PIN) ? 0 : 1;   // active-low → invert

            printf("A,%d,%d,%d,%u\n", ax, ay, az, btn);
            last_tx = now;
        }
    }

    return 0;
}