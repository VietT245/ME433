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
#define Heartbeat_LED 25

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

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
