#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_CS_RAM 13

static float sine_wave[1000];
static float radian = 0;
static float read_sine_wave[1000];

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

void spi_ram_write(uint16_t address, float v){
    uint8_t write_init[3], write_data[4];
    write_init[0] = 0b00000010;
    write_init[1] = ((address >> 8) & 0b11111111);
    write_init[2] = (address & 0b11111111);

    union FloatInt num;
    num.f = v;

    write_data[0] = ((num.i >> 24) & 0b11111111);
    write_data[1] = ((num.i >> 16) & 0b11111111);
    write_data[2] = ((num.i >> 8) & 0b11111111);
    write_data[3] = (num.i & 0b11111111);

    cs_select(PIN_CS_RAM);
    spi_write_blocking(SPI_PORT, write_init, 3);
    spi_write_blocking(SPI_PORT,write_data,4);
    cs_deselect(PIN_CS_RAM);
}

float spi_ram_read(uint16_t address) {
    uint8_t write[3], read[4];
    write[0] = 0b00000011;
    write[1] = ((address >> 8) & 0b11111111);
    write[2] = (address & 0b11111111);

    
    cs_select(PIN_CS_RAM);
    spi_write_blocking(SPI_PORT, write, 3);
    spi_read_blocking(SPI_PORT, 0, read, 4);
    cs_deselect(PIN_CS_RAM);

    union FloatInt num;
    num.i = 0;
    num.i |= (read[0] << 24);
    num.i |= (read[1] << 16);
    num.i |= (read[2] << 8);
    num.i |= (read[3]);

    return num.f;
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
    for (int i=0;i<100;i++){
        sine_wave[i] = 1.65 * sin(radian) + 1.65; // Centers the sine wave on 1.65 with amplitude 1.65, making it always positive
        radian += (2 * 3.14159265359 * 2) / 100; // Makes two periods in one cycle
    }
}

int main()
{
    stdio_init_all();


    while (!stdio_usb_connected()){
        sleep_ms(100);
    }
    print("Start\n");

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
        uint16_t addr = 4*i;
        spi_ram_write(addr, sine_wave[i]);
        printf("Writing sine wave %.4f\n", sine_wave[i]);
    }

    printf("Sine wave sent to RAM");

    while (true) {
        for (int i = 0; i< 1000; i++) {
            uint16_t address = 4*i;
            printf("Address: %d\r\n",address);
            read_sine_wave[i] = spi_ram_read(address);
            printf("Original sine wave: %.4f\r\n Read sine wave: %.4f\n", sine_wave[i], read_sine_wave[i]);
            writeDAC(0, read_sine_wave[i]);
            writeDAC(1, sine_wave[i]);
            sleep_ms(1);
        }
    }
}