#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define HX711_SCK_PIN   2
#define HX711_DT_PIN    3

// IIR filter state
// Single-pole low-pass:  y[n] = alpha*x[n] + (1-alpha)*y[n-1]
// alpha ≈ 0.15 gives a cut-off well below 25 Hz at 80 Hz sample rate
#define IIR_ALPHA_NUM   15      // numerator   (alpha = 15/100 = 0.15)
#define IIR_ALPHA_DEN   100     // denominator

#define MAX_SAMPLES     4000

static int32_t  buf_raw[MAX_SAMPLES];
static int32_t  buf_filt[MAX_SAMPLES];
static uint32_t buf_time_ms[MAX_SAMPLES];

//  HX711 initialization
void hx711_init(void) {
    gpio_init(HX711_SCK_PIN);
    gpio_set_dir(HX711_SCK_PIN, GPIO_OUT);
    gpio_put(HX711_SCK_PIN, 0);          // SCK idle low

    gpio_init(HX711_DT_PIN);
    gpio_set_dir(HX711_DT_PIN, GPIO_IN);
    gpio_pull_up(HX711_DT_PIN);          // weak pull-up so idle high is stable
}

//  Read one 24-bit sample from HX711
//  Returns a sign-extended 32-bit signed integer.
int32_t hx711_read(void) {
    // Wait until DT goes LOW (conversion complete)
    while (gpio_get(HX711_DT_PIN) != 0) {
        tight_loop_contents();   // yield to allow other things if RTOS is used
    }

    // Clock out 24 data bits
    uint32_t raw = 0;
    for (int i = 0; i < 24; i++) {
        gpio_put(HX711_SCK_PIN, 1);
        sleep_us(1);                     // t_high >= 0.2 µs per datasheet
        raw = (raw << 1) | gpio_get(HX711_DT_PIN);
        gpio_put(HX711_SCK_PIN, 0);
        sleep_us(1);                     // t_low  >= 0.2 µs per datasheet
    }

    // 25th pulse: select Channel A, Gain 128 for next conversion
    gpio_put(HX711_SCK_PIN, 1);
    sleep_us(1);
    gpio_put(HX711_SCK_PIN, 0);
    sleep_us(1);

    // Sign-extend 24-bit two's complement → 32-bit signed int
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }

    return (int32_t)raw;
}

int main(void) {
    stdio_init_all();
    hx711_init();

    // Give the host a moment to open its serial terminal before we start
    sleep_ms(2000);

    while (true) {
        // Wait for the host to send the desired sample count
        // Protocol: host sends an ASCII decimal number followed by '\n'.
        uint32_t n_samples = 0;
        int c;
        // Drain any leftover bytes first
        while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) { /* discard */ }

        // Block until we receive a complete number
        while (true) {
            c = getchar();                // blocks until a byte arrives
            if (c == '\n' || c == '\r') {
                if (n_samples > 0) break; // got a valid number
            } else if (c >= '0' && c <= '9') {
                n_samples = n_samples * 10 + (uint32_t)(c - '0');
            }
        }

        // Clamp to our buffer size
        if (n_samples > MAX_SAMPLES) {
            n_samples = MAX_SAMPLES;
        }
        if (n_samples == 0) {
            continue;
        }

        // Collect samples
        // Warm-up: discard the very first reading (device may have been idle)
        hx711_read();

        // Seed the IIR filter with the first real sample to avoid step transient
        int32_t first = hx711_read();
        // Use Q15 fixed-point to avoid floating-point dependency:
        //   filt_fp = filtered value * IIR_ALPHA_DEN
        int64_t filt_fp = (int64_t)first * IIR_ALPHA_DEN;

        buf_raw[0]     = first;
        buf_filt[0]    = first;
        buf_time_ms[0] = to_ms_since_boot(get_absolute_time());

        for (uint32_t i = 1; i < n_samples; i++) {
            int32_t  raw_val = hx711_read();
            uint32_t ts      = to_ms_since_boot(get_absolute_time());

            // IIR:  filt_fp = alpha*raw*DEN + (1-alpha)*filt_fp
            //              = alpha_num*raw + (DEN-alpha_num)/DEN * filt_fp
            filt_fp = (int64_t)IIR_ALPHA_NUM * raw_val
                    + (int64_t)(IIR_ALPHA_DEN - IIR_ALPHA_NUM) * (filt_fp / IIR_ALPHA_DEN);

            buf_raw[i]     = raw_val;
            buf_filt[i]    = (int32_t)(filt_fp / IIR_ALPHA_DEN);
            buf_time_ms[i] = ts;
        }

        // Transmit all data back to host
        // Format per line:  <raw>,<filtered>,<time_ms>\n
        printf("BEGIN %lu\n", (unsigned long)n_samples);
        for (uint32_t i = 0; i < n_samples; i++) {
            printf("%ld,%ld,%lu\n",
                   (long)buf_raw[i],
                   (long)buf_filt[i],
                   (unsigned long)buf_time_ms[i]);
        }
        printf("END\n");
    }

    return 0;
}