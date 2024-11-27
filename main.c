#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define BAUD_RATE 9600
#define UART_TX_PIN 0  // UART TX pin
#define UART_RX_PIN 1  // UART RX pin
#define STRLEN 80
#define RESPONSE_TIMEOUT_MS 2000  // Increased timeout for receiving response in milliseconds
#define MAX_ATTEMPTS 5           // Maximum retry attempts for "AT" command
#define COMMAND_DELAY_MS 1000     // Delay between commands

// Function to send commands to LoRa module
void send_lora_command(const char *cmd) {
    uart_puts(uart0, cmd);
    uart_puts(uart0, "\r\n");  // Append carriage return and newline
    printf("Sent: %s\n", cmd); // Log sent command
}

// Function to clear UART input buffer
void clear_uart_buffer() {
    while (uart_is_readable(uart0)) {
        uart_getc(uart0);  // Read and discard any old data in the UART buffer
    }
}

// Function to receive a response from the LoRa module
bool receive_lora_response(char *buffer, int max_len, uint32_t timeout_ms) {
    int pos = 0;
    absolute_time_t start_time = get_absolute_time();  // Start timeout timer

    while (true) {
        if (uart_is_readable(uart0)) {
            char c = uart_getc(uart0);
            if (c == '\r' || c == '\n') {
                if (pos > 0) {
                    buffer[pos] = '\0';  // Null-terminate string
                    printf("Received: %s\n", buffer);
                    return true;  // Return true if a complete response is received
                }
            } else {
                if (pos < max_len - 1) {
                    buffer[pos++] = c;  // Store character
                }
            }
        }

        // Check if response timeout has occurred
        if (absolute_time_diff_us(start_time, get_absolute_time()) / 1000 > timeout_ms) {
            if (pos > 0) {
                buffer[pos] = '\0';
                printf("Received (partial): %s\n", buffer);
            } else {
                printf("No response received within timeout\n");
            }
            return false;  // Timeout occurred, return false
        }
    }
}
// Function to get the DevEui (EUID) and process it
bool get_deveui(char *deveui) {
    send_lora_command("AT+ID=DevEui");
    char buffer[STRLEN];
    if (receive_lora_response(buffer, STRLEN, RESPONSE_TIMEOUT_MS)) {
        // The response might look like: "+ID: DevEui, 2C:F7:F1:20:42:00:34:09"
        // We need to skip "+ID: DevEui," and remove the colons
        if (strncmp(buffer, "+ID: DevEui,", 12) == 0) {
            // Start parsing from after the comma (skip 12 characters)
            int j = 0;
            for (int i = 12; buffer[i] != '\0'; i++) {
                if (buffer[i] == ':') {
                    continue;  // Skip colons
                }
                // Convert to lowercase and store it in the deveui array
                deveui[j++] = tolower(buffer[i]);
            }
            deveui[j] = '\0';  // Null-terminate DevEui
            printf("DevEui: %s\n", deveui);
            return true;
        } else {
            printf("Error: Unexpected response for DevEui: %s\n", buffer);
        }
    }
    return false;
}


// Function to get the firmware version
bool get_firmware_version() {
    clear_uart_buffer();
    send_lora_command("AT+VER");  // Correct command to get the firmware version
    char buffer[STRLEN];
    if (receive_lora_response(buffer, STRLEN, RESPONSE_TIMEOUT_MS)) {
        // The response should look like: "+VER: 4.0.11"
        // We need to check for the "+VER:" prefix
        if (strncmp(buffer, "+VER:", 5) == 0) {
            printf("Firmware Version: %s\n", buffer + 5);  // Skip "+VER:" prefix
            return true;
        } else {
            printf("Error: Unexpected response for version: %s\n", buffer);
        }
    } else {
        printf("No response for firmware version\n");
    }
    return false;
}


// Main program loop
int main() {
    const uint led_pin = 22;  // LED Pin
    const uint button_pin = 9;    // Button Pin (SW_0)

    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    // Initialize Button pin
    gpio_init(button_pin);
    gpio_set_dir(button_pin, GPIO_IN);
    gpio_pull_up(button_pin);  // Pull-up to ensure it reads correctly when pressed

    // Initialize UART0
    stdio_init_all();
    uart_init(uart0, BAUD_RATE);

    // Configure TX and RX pins
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Main loop
    while (true) {
        // Wait for the button press (SW_0)
        if (!gpio_get(button_pin)) {
            while (!gpio_get(button_pin)) { sleep_ms(500); }  // Wait for button release

            bool connected = false;
            int attempts = 0;
            char buffer[STRLEN];

            // Step 1: Send "AT" and verify response
            while (attempts < MAX_ATTEMPTS && !connected) {
                clear_uart_buffer();  // Clear any previous responses in the UART buffer
                send_lora_command("AT");
                if (receive_lora_response(buffer, STRLEN, RESPONSE_TIMEOUT_MS) && strstr(buffer, "+AT: OK") != NULL) {
                    connected = true;
                    printf("Connected to LoRa module\n");
                } else {
                    attempts++;
                    printf("Attempt %d failed. Retrying...\n", attempts);
                }
            }

            if (!connected) {
                printf("Module not responding\n");
                continue;  // Go back to waiting for button press
            }

            // Add a larger delay between commands to allow the module to reset/settle
            sleep_ms(COMMAND_DELAY_MS);

            // Step 2: Get firmware version after AT command
            if (!get_firmware_version()) {
                printf("Module stopped responding\n");
                continue;  // Go back to waiting for button press
            }

            // Add a larger delay between commands to ensure the module can process next command
            sleep_ms(COMMAND_DELAY_MS);

            // Step 3: Get DevEui (after AT+VER)
            char deveui[17];  // DevEui is 16 characters in hexadecimal
            if (!get_deveui(deveui)) {
                printf("Module stopped responding\n");
                continue;  // Go back to waiting for button press
            }
        }
    }
    return 0;
}
