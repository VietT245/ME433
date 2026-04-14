#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C settings
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5
#define MCP23008_ADDR 0x20 // A0=A1=A2=GND

// MCP23008 registers
#define IODIR 0x00
#define GPIO 0x09
#define OLAT 0x0A

// Pico heartbeat LED
#define HEARTBEAT_LED 25

// I2C helper functions

// Writing 1 byte to a register
void write_reg(uint8_t addr, uint8_t reg, uint8_t value){
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
}

// Read 1 byte from a register
uint8_t read_reg(uint8_t addr, uint8_t reg){
    uint8_t val;
    i2c_write_blocking(I2C_PORT, addr, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, addr, &val, 1, false);
    return val;
}

int main()
{
    stdio_init_all();

    // Initializing heartbeat LED
    gpio_init(HEARTBEAT_LED);
    gpio_set_dir(HEARTBEAT_LED, GPIO_OUT);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    sleep_ms(100);

    // Initializing MCP23008

    // GP0 = input, GP7 = output, others input
    // 0b01111111 = 0x7F
    write_reg(MCP23008_ADDR, IODIR, 0x7F);

    // Clear outputs (LED OFF)
    write_reg(MCP23008_ADDR, OLAT, 0x00);

    // Testing LED
    for (int i = 0; i < 5; i++){
        write_reg(MCP23008_ADDR, OLAT, 0x80); // GP7 ON
        sleep_ms(300);
        write_reg(MCP23008_ADDR, OLAT, 0x00); // GP7 OFF
        sleep_ms(300);
    }


    // Button to LED Control
    while (true) {
        // Heartbeat LED button toggle
        gpio_put(HEARTBEAT_LED, 1);
        sleep_ms(100);
        gpio_put(HEARTBEAT_LED, 0);
        sleep_ms(100);

        // Reading GPIO register
        uint8_t gpio_val = read_reg(MCP23008_ADDR, GPIO);

        // Extract GP0 (bit 0)
        uint8_t button = gpio_val & 0x01;

        // Active low button (pressed = 0)
        if (button == 0){
            write_reg(MCP23008_ADDR, OLAT, 0x80); // GP7 ON
        } else {
            write_reg(MCP23008_ADDR, OLAT, 0x00); // GP7 OFF
        }
    }
}