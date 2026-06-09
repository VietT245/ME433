#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "math.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_CS_RAM 13

static uint16_t sine_wave_dac[1000]; // Pre-converted DAC values

static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

// Prototypes
void writeDac(int channel, float voltage);
void spi_ram_init();
void spi_ram_write(uint16_t address, float v);
float spi_ram_read(uint16_t address);
union FloatInt {
    float f;
    uint32_t i;
};

void spi_ram_init(){ // Sets the mode as sequential
    uint8_t buf[2];
    buf[0] = 0b00000001;
    buf[1] = 0b01000000;

    cs_select(PIN_CS_RAM);
    spi_write_blocking(SPI_PORT, buf, 2 );
    cs_deselect(PIN_CS_RAM);
}

void spi_ram_write(uint16_t address, uint16_t val) {
    uint8_t write_init[3], write_data[2];
    write_init[0] = 0b00000010;          // write instruction
    write_init[1] = (address >> 8) & 0xFF;
    write_init[2] =  address       & 0xFF;
    write_data[0] = (val >> 8) & 0xFF;
    write_data[1] =  val       & 0xFF;
    cs_select(PIN_CS_RAM);
    spi_write_blocking(SPI_PORT, write_init, 3);
    spi_write_blocking(SPI_PORT, write_data, 2);
    cs_deselect(PIN_CS_RAM);
}

uint16_t spi_ram_read(uint16_t address) {
    uint8_t write[3], read[2];
    write[0] = 0b00000011;           // read instruction
    write[1] = (address >> 8) & 0xFF;
    write[2] =  address       & 0xFF;
    cs_select(PIN_CS_RAM);
    spi_write_blocking(SPI_PORT, write, 3);
    spi_read_blocking(SPI_PORT, 0, read, 2);
    cs_deselect(PIN_CS_RAM);
    return (read[0] << 8) | read[1];
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

void makeSine() {
    float radian = 0;
    for (int i = 0; i < 1000; i++) {
        // One full cycle of sine wave from 0V to 3.3V
        float voltage = 1.65f * sinf(radian) + 1.65f;

        // Convert voltage to 10-bit DAC command word (channel A)
        uint16_t voltage_bits = (uint16_t)((voltage / 3.3f) * 1023.0f);
        uint8_t high = ((0 & 0x01) << 7) | (0b111 << 4) | ((voltage_bits >> 6) & 0x0F);
        uint8_t low  = (voltage_bits & 0x3F) << 2;
        sine_wave_dac[i] = (high << 8) | low;

        radian += (2.0f * 3.14159265359f) / 1000.0f; // one full period over 1000 steps
    }
}

int main()
{
    stdio_init_all();


    while (!stdio_usb_connected()){
        sleep_ms(100);
    }
    printf("Start\n");

    // Initialize SPI
    spi_init(SPI_PORT, 1000*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    gpio_set_dir(PIN_CS_RAM, GPIO_OUT);
    gpio_put(PIN_CS_RAM, 1);

    // Initialize RAM
    spi_ram_init();
    printf("Initialized RAM\n");

    makeSine();
    printf("Initialized Sine wave");

    // Write sine wave to RAM
    for (int i=0;i<1000;i++){
        uint16_t addr = 2*i;
        spi_ram_write(addr, sine_wave[i]);
        printf("Writing sine wave %.4f\n", sine_wave[i]);
    }

    printf("Sine wave sent to RAM");

    while (true) {
        for (int i = 0; i< 1000; i++) {
            uint16_t address = 2*i;
            // printf("Address: %d\r\n",address);
            read_sine_wave[i] = spi_ram_read(address);
            // printf("Original sine wave: %.4f\r\n Read sine wave: %.4f\n", sine_wave[i], read_sine_wave[i]);
            writeDAC(0, read_sine_wave[i]);
            writeDAC(1, sine_wave[i]);
            sleep_ms(1);
        }
    }
}