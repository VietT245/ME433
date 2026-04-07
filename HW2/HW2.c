#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

int main()
{
    const uint pin = 16;
    while (1) {
        //Sweep 1000–2000 µs
        for (int us = 1000; us <= 2000; us++){
            pwm_set_chan_level(slice, channel, us);
            sleep_ms(10);
        }
        for (int us = 2000; us >= 1000; us--){
            pwm_set_chan_level(slice, channel, us);
            sleep_ms(10);
        }
    }
}
