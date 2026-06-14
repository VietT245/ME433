#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

int main() {
    stdio_init_all();
    sleep_ms(2000);
    // Initialize UART1 for STM32 communication
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    while (1) {
        // If data from computer → send to STM32
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            uart_putc(UART_ID, (char)c);
        }

        // If data from STM32 → print to computer (don't send back to STM32)
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            putchar(c);
            stdio_flush();  // Add this to fix missing characters
        }
    }

    return 0;
}