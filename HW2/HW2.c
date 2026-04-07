#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    const uint pin = 0;
    const float clkdiv = 125.0f;
    const uint16_t wrap = 20000;
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    pwm_set_clkdiv(slice, clkdiv);
    pwm_set_wrap(slice, wrap);

    pwm_set_enabled(slice, true);

    while (1) {
        //Sweep 1000–2000 µs
        for (int us = 1000; us <= 2000; us+= 10){
            pwm_set_chan_level(slice, channel, us);

            // Convert pulse width to angle (approx)
            float angle = (us - 1000) * (180.0f / 1000.0f);

            printf("Pulse: %d us   Angle: %.1f degrees\n", us, angle);

            sleep_ms(10);
        }
        // Sweep backward
        for (int us = 2000; us >= 1000; us-= 10){
            pwm_set_chan_level(slice, channel, us);

            float angle = (us - 1000) * (180.0f / 1000.0f);

            printf("Pulse: %d us   Angle: %.1f degrees\n", us, angle);

            sleep_ms(10);
        }
    }
}
