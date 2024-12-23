#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// TODO+ Device remembers state even if reset EEPROM
// Device connected to Lorawan and sends logs when status is changed

#define OPTO_PIN 28 // Optical sensor pin
#define PIEZO_PIN 27 // Pill drop sensor

#define MOTOR_1 2 // Motor pins
#define MOTOR_2 3
#define MOTOR_3 6
#define MOTOR_4 13

#define LED1_PIN 22 // Led pins
#define LED2_PIN 21
#define LED3_PIN 20

#define Calibration_PIN 9 // SW0
#define Dispense_PIN 8  // SW1
#define SW2_PIN 7

#define SDA_PIN 16
#define SCL_PIN 17

#define PWM_CLOCKDIV 125
#define PWM_WRAP 999

volatile bool LedTimerTriggered = false;
bool timer_callback(repeating_timer_t *rt) {
    LedTimerTriggered = true;
    return true; // keeps repeating
}

const uint MOTOR_PINS[] = {MOTOR_1, MOTOR_2, MOTOR_3, MOTOR_4};

// Sequence for half-step
const int step_sequence[8][4] = {
        {1, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 0, 1},
        {1, 0, 0, 1}
};

void stepMotor(int step) {
    for (int pin = 0; pin < 4; ++pin) {
        gpio_put(MOTOR_PINS[pin], step_sequence[step][pin]);
    }
}

static int GlobalMotorStep_count = 0;
void runMotor(int fraction, int averageSteps) {
    int stepsToRun = (averageSteps / 8)*fraction;
    int target_step_count = GlobalMotorStep_count + stepsToRun;

    for (; GlobalMotorStep_count < target_step_count; GlobalMotorStep_count++) {
        stepMotor(GlobalMotorStep_count % 8);
        sleep_ms(2);
    }
}

// Interrupt handler for optical and piezo sensor
volatile bool optopinTriggered = false;
volatile bool pillDispensed = false;
void generic_irq_callback(uint gpio, uint32_t event_mask){
    if (gpio == OPTO_PIN) {
        optopinTriggered = true;
    }
    if (gpio == PIEZO_PIN) {
        pillDispensed = true;
    }
}

// Main program
int main() {
    gpio_init(OPTO_PIN);
    gpio_set_dir(OPTO_PIN, GPIO_IN);
    gpio_pull_up(OPTO_PIN);

    gpio_init(PIEZO_PIN);
    gpio_set_dir(PIEZO_PIN, GPIO_IN);
    gpio_pull_up(PIEZO_PIN);

    //Initializing stepper motor pins
    gpio_init(MOTOR_1);
    gpio_set_dir(MOTOR_1, GPIO_OUT);
    gpio_init(MOTOR_2);
    gpio_set_dir(MOTOR_2, GPIO_OUT);
    gpio_init(MOTOR_3);
    gpio_set_dir(MOTOR_3, GPIO_OUT);
    gpio_init(MOTOR_4);
    gpio_set_dir(MOTOR_4, GPIO_OUT);

    //Initializing LED1 pin
    gpio_init(LED1_PIN);
    gpio_set_function(LED1_PIN, GPIO_FUNC_PWM);
    gpio_set_dir(LED1_PIN, GPIO_OUT);

    //Initializing LED2 pin
    gpio_init(LED2_PIN);
    gpio_set_function(LED2_PIN, GPIO_FUNC_PWM);
    gpio_set_dir(LED2_PIN, GPIO_OUT);

    //Initializing LED3 pin
    gpio_init(LED3_PIN);
    gpio_set_function(LED3_PIN, GPIO_FUNC_PWM);
    gpio_set_dir(LED3_PIN, GPIO_OUT);

    //Initializing button pins
    gpio_init(Calibration_PIN);
    gpio_set_dir(Calibration_PIN, GPIO_IN);
    gpio_init(Dispense_PIN);
    gpio_set_dir(Dispense_PIN, GPIO_IN);
    gpio_init(SW2_PIN);
    gpio_set_dir(SW2_PIN, GPIO_IN);
    gpio_pull_up(Calibration_PIN);
    gpio_pull_up(Dispense_PIN);
    gpio_pull_up(SW2_PIN);


    // Interrupts
    gpio_set_irq_enabled_with_callback(OPTO_PIN, GPIO_IRQ_EDGE_FALL, true, &generic_irq_callback);
    gpio_set_irq_enabled(OPTO_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIEZO_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_callback(&generic_irq_callback);

    stdio_init_all();

    // Creates timer
    repeating_timer_t timer;

    // Negative timeout means exact delay
    if (!add_repeating_timer_us(-500000, timer_callback, NULL, &timer)) {
        printf("Timer failed.\n");
        return 1;
    }
    printf("Booting...");

    //Config PWM for LED1
    uint led1PwnChannel = pwm_gpio_to_channel(LED1_PIN);
    uint led1PwnSlice = pwm_gpio_to_slice_num(LED1_PIN);
    pwm_set_enabled(led1PwnSlice, false);
    pwm_config config_led1 = pwm_get_default_config();
    pwm_config_set_wrap(&config_led1, PWM_WRAP);
    pwm_config_set_clkdiv(&config_led1, PWM_CLOCKDIV);
    pwm_init(led1PwnSlice, &config_led1, false);
    pwm_set_chan_level(led1PwnSlice, led1PwnChannel, 0);
    gpio_set_function(LED1_PIN, GPIO_FUNC_PWM);
    pwm_set_enabled(led1PwnSlice, true);

    //Config PWM for LED2
    uint led2PwnChannel = pwm_gpio_to_channel(LED2_PIN);
    uint led2PwnSlice = pwm_gpio_to_slice_num(LED2_PIN);
    pwm_set_enabled(led2PwnSlice, false);
    pwm_config config_led2 = pwm_get_default_config();
    pwm_config_set_wrap(&config_led2, PWM_WRAP);
    pwm_config_set_clkdiv(&config_led2, PWM_CLOCKDIV);
    pwm_init(led2PwnSlice, &config_led2, false);
    pwm_set_chan_level(led2PwnSlice, led2PwnChannel, 0);
    gpio_set_function(LED2_PIN, GPIO_FUNC_PWM);
    pwm_set_enabled(led2PwnSlice, true);

    //Config PWM for LED3
    uint led3PwnChannel = pwm_gpio_to_channel(LED3_PIN);
    uint led3PwnSlice = pwm_gpio_to_slice_num(LED3_PIN);
    pwm_set_enabled(led3PwnSlice, false);
    pwm_config config_led3 = pwm_get_default_config();
    pwm_config_set_wrap(&config_led3, PWM_WRAP);
    pwm_config_set_clkdiv(&config_led3, PWM_CLOCKDIV);
    pwm_init(led3PwnSlice, &config_led3, false);
    pwm_set_chan_level(led3PwnSlice, led3PwnChannel, 0);
    gpio_set_function(LED3_PIN, GPIO_FUNC_PWM);
    pwm_set_enabled(led3PwnSlice, true);

    bool isCalibrated = false;
    int pillsDispensed_count = 0;
    int turnsCount = 0;
    int step_index = 0;
    int step_count = 0;
    float averageSteps = 0;
    int CalibMeasurements[3];
    int ledState = 0;


while (1) {
    // Blink LED1 while waiting
    if (LedTimerTriggered) {
        if (!isCalibrated) {
            // If not calibrated, Led blinks slowly
            pwm_set_chan_level(led1PwnSlice, led1PwnChannel, ledState);
            ledState = ledState ? 0 : 100;
        } else {
            // If calibrated, led blinks faster
            pwm_set_chan_level(led1PwnSlice, led1PwnChannel, ledState);
            ledState = ledState ? 0 : 100; // Other possible Led state
        }
        LedTimerTriggered = false;
    }

    // Calibrating
    // SW0 button to start calibrating
    if (!gpio_get(Calibration_PIN))
    {
        while (!gpio_get(Calibration_PIN)) {
            sleep_ms(50);
        }

        // Check if already calibrated and average steps are valid
        if (isCalibrated && averageSteps > 4090) {
            printf("System is already calibrated. Average steps: %g\n", averageSteps);
            continue; // Skip calibration if already calibrated
        }
        printf("Calibration starting...\n");
        pwm_set_chan_level(led1PwnSlice, led1PwnChannel, 0);
        isCalibrated = false; // Reset to false at the start of calibration

        // Initialize CalibMeasurements to a known state
        memset(CalibMeasurements, 0, sizeof(CalibMeasurements));

        while (!isCalibrated) {
            // Find reference point using optical sensor
            while (!optopinTriggered) {
                stepMotor(step_index % 8);
                step_index++;
                sleep_ms(2);
            }
            printf("Optical sensor has been found. Reference position.\n");
            optopinTriggered = false;

            // Turns 3 times to calibrate motor
            int validCount = 0; // Count of valid measurements
            int totalSteps = 0; // Total of valid steps
            for (int i = 0; i < 3; i++) {
                optopinTriggered = false;
                step_count = 0;
                while (!optopinTriggered) {
                    stepMotor(step_index % 8);
                    step_index++;
                    step_count++;
                    sleep_ms(2);
                }
                if (step_count >= 4095) { // Check if step count is valid
                    if (validCount < 3) { // Array bound
                        CalibMeasurements[validCount] = step_count;
                        totalSteps += step_count;
                        validCount++;
                    } else {
                        printf("Max valid measurement reached.\n");
                    }
                } else {
                    printf("Step count %d is below threshold and will not be used.\n", step_count);
                }
            }

            // Calculate avg steps per revolution if valid measurements exist
            if (validCount > 0) {
                averageSteps = totalSteps / validCount; // Calculate the avg of valid steps
                printf("Average steps: %g\n", averageSteps);
            } else {
                averageSteps = 0; // If no valid measurements, set avg default 0
                printf("No valid measurements found for average calculation.\n");
            }

            // Prints valid measurements
            for (int j = 0; j < validCount; j++) {
                printf("%d. Steps: %d\n", j + 1, CalibMeasurements[j]);
            }

            // Align motor with first slot (slot alignment)
            for (int step = 0; step < 166; step++) {
                stepMotor(step % 8);
                sleep_ms(5);
                step_index++;
            }
            printf("Dispenser is aligned...\n");

            isCalibrated = true; // Calibration success
        }
    }

    // Dispensing pill slots
    // SW1 button to start dispensing pills
    if (!gpio_get(Dispense_PIN)) {
        while (!gpio_get(Dispense_PIN)) {
            sleep_ms(50);
        }
        // Turns LED1 off
        if (LedTimerTriggered) {
            pwm_set_chan_level(led1PwnSlice, led1PwnChannel, 0);
        }
        // Checks if device is calibrated
        if (!isCalibrated) {
            printf("Calibration is not ready. Press SW0\n");
            continue; // Skips dispensing if not calibrated
        }
        pillDispensed = false;
        while (turnsCount < 8) {
            runMotor(1, averageSteps);
            sleep_ms(80);
            if (pillDispensed) {
                pillsDispensed_count++;
                pillDispensed = false;
                printf("Pill dispensed\n");
            } else {
                // Blink LED1 5 times if no pills detected
                for (int i = 0; i <= 5; i++) {
                    pwm_set_chan_level(led1PwnSlice, led1PwnChannel, 100);
                    sleep_ms(100);
                    pwm_set_chan_level(led1PwnSlice, led1PwnChannel, 0);
                    sleep_ms(100);
                }
                printf("No dispense detected\n");
            }
            turnsCount++;
        }
        printf("Number of pills dispensed: %d\n", pillsDispensed_count);
        turnsCount = 0;
        pillsDispensed_count = 0;
    }
}
    cancel_repeating_timer(&timer);
}
