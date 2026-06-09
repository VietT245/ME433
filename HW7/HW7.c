#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "math.h"

#define SPI_PORT spi0
#define PIN_MISO 16 // RX Pin
#define PIN_CS   17
#define PIN_SCK  18 // SCK Pin
#define PIN_MOSI 19 // TX Pin

static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop"); // FIXME
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop"); // FIXME
}

// channel: 0 = channel A, 1 = channel B
// voltage: 0.0 to 3.3
void writeDAC(int channel, float voltage) {
    // Clamp voltage to valid range
    if (voltage < 0.0f)  voltage = 0.0f;
    if (voltage > 3.3f)  voltage = 3.3f;

    uint16_t voltage_bits = (uint16_t) ((voltage/3.3) * 1023.0); // Voltage between 0 and 1023
    uint8_t data[2] ;
    int len = 2;

    data[0] = ((channel & 0x01) << 7) | (0b111 << 4) | ((voltage_bits >> 6) & 0x0F);
    data[1] = ((voltage_bits & 0x3F) << 2);
    
    cs_select(PIN_CS);
    spi_write_blocking(SPI_PORT, data, len);
    cs_deselect(PIN_CS);
}

void makeTriangle(){
    int i = 1;
    triangle_wave[0] = 1.65;
    for (int i = 1; i < 100; i++)
    {
        if (i < 25) {
            triangle_wave[i] = triangle_wave[i-1] + (1.65/25);
        }
        else if (i > 24 && i < 75) {
            triangle_wave[i] = triangle_wave[i-1] - (3.3/50);
        }
        else if (i > 74 && i < 100) {
            triangle_wave[i] = triangle_wave[i-1] + (1.65/25);
        }
    }
}

void makeSine() {
    float radian = 0;
    for (int i=0;i<100;i++){
        sine_wave[i] = 1.65 * sin(radian) + 1.65; // Centers the sine wave on 1.65 with amplitude 1.65, making it always positive
        radian += (2*3.14159265359) / 100; // Makes two periods in one cycle
    }
}

int main()
{
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 1000*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    cs_select(PIN_CS);
    spi_write_blocking(SPI_PORT, data, len); // where data is a uint8_t array with length len
    cs_deselect(PIN_CS);

    while (true) {
        // Call function writeDac
        float t = 0;
        t = t + 0.1;
        float voltage = (sine(2*pi*f*t)+1) / 2 * 3.3;
        wrideDac(channel, voltage); // Update voltage 100 times per second
        sleep_ms(10);
    }
}