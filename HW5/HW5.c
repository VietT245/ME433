#include <stdio.h>
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
    i2c_write_blocking(i2c_defulat, addr, buf, 2, false);
}

// Read n bytes starting from a register on the MPU6050
void i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len){
    i2c_write_blocking(i2c_default, addr, &reg, 1, true); // true means keep bus active
    i2c_read_blocking(i2c_default, addr, buf, len, false);
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
    i2c_write_reg(MPU6050_ADDR, GYRO_CONFIG, 0x18)
}

int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);


    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
