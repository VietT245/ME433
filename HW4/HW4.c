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
#define MCP23008_ADDR 0x20

// MCP23008 registers
#define IODIR 0x00
#define GPIO 0x09
#define OLAT 0x0A

// Pico heartbeat LED
#define HEARTBEAT_LED 7
#define BUTTON_PIN 0

// Initializing funcs
void drawMessage(int x, int y, char * m);
void drawLetter(int x, int y, char c);

int main()
{
    stdio_init_all();

    // Initialize Heartbeat & Button
    // gpio_init(HEARTBEAT_LED);
    // gpio_set_dir(HEARTBEAT_LED, GPIO_OUT);
    // gpio_init(BUTTON_PIN);
    // gpio_set_dir(BUTTON_PIN, GPIO_IN);
    // gpio_pull_up(BUTTON_PIN); // button reads LOW when pressed

    // // Initialize I2C
    i2c_init(i2c_default, 400000); // 400kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Initialize the display
    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update(); // Clear Screen

    // Initialize ADC0
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    while (true) {
        int i = 1;
        while (1){
            ssd1306_clear();
            float time = to_us_since_boot(get_absolute_time()) / 1000000.0;
            float fps = i / time;
            float voltage = (adc_read() / 4096.0) * 3.3;
            char message[30];

            sprintf(message, "Voltage = %f", voltage);
            drawMessage(5,5,message);

            sprintf(message, "fps = %f", fps);
            drawMessage(5,20,message);
            ssd1306_update();

            i++;
        // Test Heartbeat LED
        // if (gpio_get(BUTTON_PIN) == 0) { // button pressed (active low)
        //     gpio_put(HEARTBEAT_LED, 1);  // hold LED high
        // } else {
        //     // blink 3 times per second: on for ~167ms, off for ~167ms
        //     gpio_put(HEARTBEAT_LED, 1);
        //     sleep_ms(100);
        //     gpio_put(HEARTBEAT_LED, 0);
        //     sleep_ms(100);
        // }

        // Test ssd1306
        // ssd1306_drawPixel(1,1,1);
        // ssd1306_update();
        // sleep_ms(20);
        // ssd1306_drawPixel(1,1,0);
        // ssd1306_update();
        // sleep_ms(20);
        }
        char message[50];
        sprintf(message, "Hello there!");
        drawMessage(10,20,message);
        ssd1306_update();
        sleep_ms(1000);
    }
}

void drawMessage(int x, int y, char * m){ // Where m is an array of characters in message
    int i = 0;
    while (m[i] != 0){ // Detects a "null character = 0" at the end of sprintf
        drawLetter(x + (i * 5), y, m[i]); // Draw letter with separation between characters
        i++;
    }
}

void drawLetter(int x, int y, char c){ 
    for (int j=0; j<5; j++){ // Iterates through columns left to right
        char col = ASCII[c-0x20] [j]; // Starting at 0x20 for ASCII table
        for (int i=0; i<8; i++){ // Iterates through rows top to bottom
            char bit = (col >> i) & 0b1; // Right shift and & each bit to check if it's a 1
            ssd1306_drawPixel(x+j, y+i, bit); // Add the bit to draw pixel
        }
    }
}