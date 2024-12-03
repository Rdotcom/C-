#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Define GPIO pins for buttons and LEDs
const uint LED_PINS[] = {20, 21, 22}; // for LEDs
const uint BUTTON_ON_OFF = 8;         // for the ON/OFF button (SW1)
const uint BUTTON_DIM_UP = 9;         // for dim up button (SW0)
const uint BUTTON_DIM_DOWN = 7;       // for dim down button (SW2)

#define PWM_WRAP 999                    // Wrap value for PWM
#define DEFAULT_BRIGHTNESS 500          // Default brightness level
#define MIN_BRIGHTNESS 0                // Minimum brightness level
#define MAX_BRIGHTNESS 1000             // Maximum brightness level
#define DIM_STEP 20                     // Steps for brightness
#define HALF_BRIGHTNESS (PWM_WRAP / 2)  // 50% brightness
#define SYSTEM_CLOCK_FREQ 125000000     // 125 MHz system clock
#define TARGET_PWM_FREQ 1000            // PWM frequency of 1 kHz

// Variables to keep track of state
bool led_on = false;
int brightness = DEFAULT_BRIGHTNESS; // Store current brightness
bool last_on_off_state = true; // Last state for the ON/OFF button

void configure_pwm() {
    // Calculate the divider to achieve 1 MHz frequency from 125 MHz
    float pwm_divider = (float)SYSTEM_CLOCK_FREQ / 1000000; // 1 MHz input to PWM

    // Initialize PWM for each LED pin
    for (int i = 0; i < 3; i++) {
        gpio_set_function(LED_PINS[i], GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(LED_PINS[i]);

        // Set the PWM clock divider
        pwm_set_clkdiv(slice_num, pwm_divider);
        pwm_set_wrap(slice_num, PWM_WRAP); // Set the wrap value for 1 kHz frequency

        // Start with LEDs off
        pwm_set_gpio_level(LED_PINS[i], 0);
        pwm_set_enabled(slice_num, true); // Enable PWM on this slice
    }
}

void set_led_brightness(int level) {
    // Clamp brightness level to within defined min and max
    level = (level < MIN_BRIGHTNESS) ? MIN_BRIGHTNESS : (level > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : level);
    for (int i = 0; i < 3; i++) {
        pwm_set_gpio_level(LED_PINS[i], level); // Set the brightness for each LED
    }
}

bool read_button(uint gpio) {
    return !gpio_get(gpio); // Returns true if the button is pressed (active low)
}

int main() {
    stdio_init_all(); // Initialize standard I/O

    // Initialize PWM with 1 MHz frequency for 1 kHz PWM frequency
    configure_pwm();

    // Initialize buttons with pull-up resistors
    gpio_init(BUTTON_ON_OFF);
    gpio_set_dir(BUTTON_ON_OFF, GPIO_IN);
    gpio_pull_up(BUTTON_ON_OFF);

    gpio_init(BUTTON_DIM_UP);
    gpio_set_dir(BUTTON_DIM_UP, GPIO_IN);
    gpio_pull_up(BUTTON_DIM_UP);

    gpio_init(BUTTON_DIM_DOWN);
    gpio_set_dir(BUTTON_DIM_DOWN, GPIO_IN);
    gpio_pull_up(BUTTON_DIM_DOWN);

    // Main loop
    while (true) {
        // Handle ON/OFF button (SW1)
        bool current_on_off_state = read_button(BUTTON_ON_OFF);

        // Detect button press
        if (current_on_off_state && !last_on_off_state) {
            // Toggle LED state
            led_on = !led_on;

            if (brightness == MIN_BRIGHTNESS) {
                // Set brightness to 50% if it was previously at min
                brightness = (brightness == MIN_BRIGHTNESS) ? HALF_BRIGHTNESS : brightness;
                set_led_brightness(brightness); // Use updated brightness value
                continue;
            } else {
                // Turn off LED by setting brightness to minimum
                set_led_brightness(MIN_BRIGHTNESS);
            }

            // Wait for button release (debounce)
            while (read_button(BUTTON_ON_OFF)) {
                sleep_ms(10); // Small delay
            }
        }

        // Update last known button state
        last_on_off_state = current_on_off_state;

        // If LEDs are ON, handle dimming buttons (SW0 and SW2)
        if (led_on) {
            // Brightness up button (SW0)
            if (read_button(BUTTON_DIM_UP)) {
                brightness += DIM_STEP; // Increase brightness
            }

            // Brightness down button (SW2)
            if (read_button(BUTTON_DIM_DOWN)) {
                brightness -= DIM_STEP; // Decrease brightness
            }

            // Clamp brightness between min and max
            brightness = (brightness < MIN_BRIGHTNESS) ? MIN_BRIGHTNESS : (brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : brightness);
            set_led_brightness(brightness); // Always use the brightness variable
        }

        sleep_ms(20); // Small delay to debounce button press
    }
}
