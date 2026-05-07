#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// I2C settings
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5
#define MCP23008_ADDR 0x20

// MCP23008 registers
#define IODIR 0x00
#define GPIO 0x09
#define OLAT 0x0A

// Pico heartbeat LED
#define HEARTBEAT_LED 7
#define BUTTON_PIN 0

int main()
{
    stdio_init_all();
    ssd1306_setup();

    while (true) {
        ssd1306_drawPixel(1,1,1);
        ssd1306_update();
        sleep_ms(20);
        ssd1306_drawPixel(1,1,0);
        ssd1306_update();
        sleep_ms(20);
    }
}
