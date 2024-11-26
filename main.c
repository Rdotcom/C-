#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#define LORA_UART uart0
#define LORA_BAUD 9600 // LoRa module baud rate (9600)
#define USB_BAUD 115200 // USB Serial baud rate (115200)

#define LORA_TX_PIN 0 // RP2040 TX (to LoRa RX)
#define LORA_RX_PIN 1 // RP2040 RX (to LoRa TX)

#define STRLEN 128  // Define the maximum string length for response

void init_lora_uart() {
    // Initialize UART for LoRa communication at 9600 baud
    uart_init(LORA_UART, LORA_BAUD);

    // Set the UART pins (TX, RX)
    gpio_set_function(LORA_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(LORA_RX_PIN, GPIO_FUNC_UART);

    // Optional: Set the UART to 8N1 (8 data bits, no parity, 1 stop bit)
    uart_set_format(LORA_UART, 8, 1, UART_PARITY_NONE);
}

void send_lora_command(const char *cmd) {
    uart_puts(LORA_UART, cmd);
    uart_puts(LORA_UART, "\r\n");  // Add newline to AT commands
}

void receive_lora_response() {
    char str[STRLEN];
    int pos = 0;

    // Loop to read and print each byte as it's received
    while (true) {
        if (uart_is_readable(LORA_UART)) {
            char c = uart_getc(LORA_UART);  // Read a byte from LoRa UART

            // Store the byte in the string if space allows
            if (pos < STRLEN - 1) {
                str[pos++] = c;
            }

            // Print the received byte to the USB serial for debugging
            printf("Received byte: 0x%02X\n", c);

            // Check for end of response (newline character or buffer full)
            if (c == '\r' || c == '\n') {
                str[pos] = '\0';  // Null-terminate the string
                printf("Complete response: %s\n", str);  // Print the complete response
                pos = 0;  // Reset position for the next line
            }
        }
    }
}

int main() {
    stdio_init_all();  // Initializes USB serial communication at 115200 baud

    init_lora_uart();  // Initialize UART for LoRa communication

    // Send command to get the firmware version of the LoRa module
    send_lora_command("AT+VER");  // Use correct command for your module

    printf("Waiting for LoRa response...\n");

    // Start receiving and printing the LoRa response
    receive_lora_response();

    while (true) {
        // Main loop - can add more logic here as needed
    }
}
