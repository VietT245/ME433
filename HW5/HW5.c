#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "hardware/adc.h"
#include "font.h"

// I2C settings
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5

// Pico heartbeat LED
#define HEARTBEAT_LED 7
#define BUTTON_PIN 0

// MPU6050 7-bit address
#define MPU6050_ADDR 0x68

// Given config registers
// config registers
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C
// sensor data registers:
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75

// Function prototypes
void drawMessage(int x, int y, char * m); // Character array m for message
void drawLetter(int x, int y, char c);
void mpu6050_init();
void mpu6050_read(float *ax, float *ay, float *az, float *gx, float *gy, float *gz, float *temp);

// Writing 1 byte to a register on the MPU6050
void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t data){
    uint8_t buf[2] = {reg, data};
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
}

// Read n bytes starting from a register on the MPU6050
void i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len){
    i2c_write_blocking(I2C_PORT, addr, &reg, 1, true); // true means keep bus active
    i2c_read_blocking(I2C_PORT, addr, buf, len, false);
}

void mpu6050_init(){
    // Check WHO_AM_I
    uint8_t who;
    i2c_read_reg(MPU6050_ADDR, WHO_AM_I, &who, 1);
    if (who != 0x68 && who != 0x98){
        // Wrong value -> turn on LED and hang
        gpio_put(HEARTBEAT_LED, 1);
        while (1) {tight_loop_contents(); }
    }

    // Wake up chip by writing 0x00 to PWER_MGMT 1
    i2c_write_reg(MPU6050_ADDR, PWR_MGMT_1, 0x00);

    // Set accelerometer sensitivty to +- 2g (bits [4:3] = 00)
    i2c_write_reg(MPU6050_ADDR, ACCEL_CONFIG, 0x00);

    // Set gyroscope sensitivity to +- 2000 dps (bits [4:3] = 11)
    i2c_write_reg(MPU6050_ADDR, GYRO_CONFIG, 0x18);
}

void mpu6050_read(float *ax, float *ay, float *az, float *gx, float *gy, float *gz, float *temp){
    uint8_t buf[14];

    // Burst read 14 bytes starting from ACCEL_XOUT_H
    i2c_read_reg(MPU6050_ADDR, ACCEL_XOUT_H, buf, 14);

    // Combine high and low bytes into signed 16-bit integers
    int16_t raw_ax = (int16_t)(buf[0] << 8 | buf[1]);
    int16_t raw_ay   = (int16_t)(buf[2]  << 8 | buf[3]);
    int16_t raw_az   = (int16_t)(buf[4]  << 8 | buf[5]);
    int16_t raw_temp = (int16_t)(buf[6]  << 8 | buf[7]);
    int16_t raw_gx   = (int16_t)(buf[8]  << 8 | buf[9]);
    int16_t raw_gy   = (int16_t)(buf[10] << 8 | buf[11]);
    int16_t raw_gz   = (int16_t)(buf[12] << 8 | buf[13]);

    // Convert to physical units
    *ax   = raw_ax   * 0.000061;   // g
    *ay   = raw_ay   * 0.000061;   // g
    *az   = raw_az   * 0.000061;   // g
    *gx   = raw_gx   * 0.007630;   // degrees per second
    *gy   = raw_gy   * 0.007630;   // degrees per second
    *gz   = raw_gz   * 0.007630;   // degrees per second
    *temp = raw_temp / 340.00 + 36.53; // degrees C
}

void drawLine(int x0, int y0, int x1, int y1, unsigned char color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        ssd1306_drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int main()
{
    stdio_init_all();

    // Initialize heartbeat LED
    gpio_init(HEARTBEAT_LED);
    gpio_set_dir(HEARTBEAT_LED, GPIO_OUT);
    
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Initialize display
    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();

    // Initialize MPU6050
    mpu6050_init();

    // Display center
    const int cx = 64; // 128/2
    const int cy = 16; // 32/2
    const float scale = 15.0; // max line length in pixels (at 1g)

    float ax, ay, az, gx, gy, gz, temp;

    while (true) {
        mpu6050_read(&ax, &ay, &az, &gx, &gy, &gz, &temp);

        // Scale acceleration to pixel length (clamped to screen bounds)
        int ex = cx - (int)(ax * scale);
        int ey = cy + (int)(ay * scale);

        // Clamp to display bounds
        if (ex < 0)   ex = 0;
        if (ex > 127) ex = 127;
        if (ey < 0)   ey = 0;
        if (ey > 31)  ey = 31;

        ssd1306_clear();

        // Draw crosshair at center
        ssd1306_drawPixel(cx,     cy,     1);
        ssd1306_drawPixel(cx + 1, cy,     1);
        ssd1306_drawPixel(cx - 1, cy,     1);
        ssd1306_drawPixel(cx,     cy + 1, 1);
        ssd1306_drawPixel(cx,     cy - 1, 1);

        // Draw line from center to scaled acceleration endpoint
        drawLine(cx, cy, ex, ey, 1);

        ssd1306_update();
    }
}

void drawMessage(int x, int y, char * m) {
    int i = 0;
    while (m[i] != 0) {
        drawLetter(x + (i * 5), y, m[i]);
        i++;
    }
}

void drawLetter(int x, int y, char c) {
    for (int j = 0; j < 5; j++) {
        char col = ASCII[c - 0x20][j];
        for (int i = 0; i < 8; i++) {
            char bit = (col >> i) & 0b1;
            ssd1306_drawPixel(x + j, y + i, bit);
        }
    }
}