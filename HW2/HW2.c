#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware_pwm.h"
#include <unistd.h>

// Converting angle to pulse width (in µs)
static inline int angle_to_us(int angle){
    return 1000 + (angle * 1000) / 180; // 1000 - 2000 µs
}

int main(void)
{
    pwm_t servo = pwm_open(0); // PWM channel 0
    pwm_set_freq(servo, 50); //50 Hz for RC servo

    while (1) {
        // Sweep 0 -> 180
        for (int a = 0; a <= 180; a++){
            pwm_set_duty_us(servo, angle_to_us(a));
            usleep(10000); //10 µs step
        }
    }
}
