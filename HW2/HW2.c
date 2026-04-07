#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware_pwm.h"
#include <unistd.h>

// Converting angle to pulse width (in µs)
static inline int angle_to_us(int angle){
    return 1000 + (angle * 1000) / 180; // 1000 - 2000 µs
}

int main()
{
    stdio_init_all();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
