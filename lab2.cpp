#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"

// Define GPIO pins for LEDs and rotary encoder
const uint LED_PINS[] = {20, 21, 22}; // for LEDs
const uint ROT_B = 11;                // Rotary encoder B
const uint ROT_A = 10;                // Rotary encoder A
const uint ROT_SW = 12;               // Rotary encoder switch

#define PWM_WRAP 999                    // Wrap value for PWM
#define DEFAULT_BRIGHTNESS 500          // Default brightness level
#define MIN_BRIGHTNESS 0                // Minimum brightness level
#define MAX_BRIGHTNESS 1000             // Maximum brightness level
#define DIM_STEP 20                     // Steps for brightness
#define HALF_BRIGHTNESS (PWM_WRAP / 2)  // 50% brightness
#define SYSTEM_CLOCK_FREQ 125000000     // 125 MHz system clock
#define TARGET_PWM_FREQ 1000            // PWM frequency 1 kHz

// State variables
bool led_on = false;
int brightness = DEFAULT_BRIGHTNESS; // Store current brightness
bool last_on_off_state = true;       // Last state for the rotary switch button

void update_brightness_from_encoder(uint gpio, uint32_t events);

void configure_pwm() {
    float pwm_divider = (float)SYSTEM_CLOCK_FREQ / 1000000; // 1 MHz input to PWM

    // Initialize PWM for each LED pin
    for (int i = 0; i < 3; i++) {
        gpio_set_function(LED_PINS[i], GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(LED_PINS[i]);

        // Set the PWM clock divider
        pwm_set_clkdiv(slice_num, pwm_divider);
        pwm_set_wrap(slice_num, PWM_WRAP); // Set the wrap value for 1 kHz freq

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

void setup_gpio() {
    // Initialize LEDs
    configure_pwm();

    // Initialize rotary encoder pins with interrupts on Rot_A
    gpio_init(ROT_B);
    gpio_set_dir(ROT_B, GPIO_IN);
    gpio_disable_pulls(ROT_B); // No pull-up/pull-down
    gpio_set_irq_enabled_with_callback(ROT_B, GPIO_IRQ_EDGE_RISE, true, &update_brightness_from_encoder);

    gpio_init(ROT_A);
    gpio_set_dir(ROT_A, GPIO_IN);
    gpio_disable_pulls(ROT_A); // No pull-up/pull-down

    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW); // Use pull-up resistor
}

// Interrupt handler for rotary encoder
void update_brightness_from_encoder(uint gpio, uint32_t events) {
    if (gpio == ROT_B && led_on) { // Only adjust brightness if LED is on
        bool rot_b_state = gpio_get(ROT_A); // Check state Rot_B to determine direction

        // Adjust brightness based on Rot_B state
        if (rot_b_state) {
            brightness += DIM_STEP; // Clockwise step
        } else {
            brightness -= DIM_STEP; // Counterclockwise step
        }

        // Clamp brightness and set it
        brightness = (brightness < MIN_BRIGHTNESS) ? MIN_BRIGHTNESS : (brightness > MAX_BRIGHTNESS ? MAX_BRIGHTNESS : brightness);
        set_led_brightness(brightness);
    }
}

int main() {
    stdio_init_all();
    setup_gpio();

    // Main loop
    while (true) {
        // Handle ON/OFF button (Rotary encoder)
        bool current_on_off_state = read_button(ROT_SW);

        // Detect button press (debounced)
        if (current_on_off_state && !last_on_off_state) {
            if (led_on) {
                // If LEDs are on and brightness is at min, set to 50% brightness
                if (brightness == MIN_BRIGHTNESS) {
                    brightness = HALF_BRIGHTNESS; // Set to 50% if coming from 0%
                    set_led_brightness(brightness);
                } else {
                    // If LEDs are on and brightness is not 0%, toggle the LEDs OFF
                    led_on = false;
                    set_led_brightness(MIN_BRIGHTNESS); // Turn off LEDs
                }
            } else {
                // If LEDs are off, turn them ON and set to current brightness
                led_on = true;
                set_led_brightness(brightness);
            }

            // Wait for button release (debounce)
            while (read_button(ROT_SW)) {
                sleep_ms(10);
            }
        }

        // Update last known button state
        last_on_off_state = current_on_off_state;

        sleep_ms(10); // Small delay
    }
}
