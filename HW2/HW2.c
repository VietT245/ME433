#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

int main()
{
    const uint pin = 16;
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    pwm_set_wrap(slice, 20000); //20 µs period (50hz)
    pwm_set_enabled(slice, true);
    
    while (1) {
        //Sweep 1000–2000 µs
        for (int us = 1000; us <= 2000; us++){
            pwm_set_chan_level(slice, channel, us);
            sleep_ms(10);
        }
        // Sweep backward
        for (int us = 2000; us >= 1000; us--){
            pwm_set_chan_level(slice, channel, us);
            sleep_ms(10);
        }
    }
}
