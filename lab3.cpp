#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pico/stdlib.h>

// Define GPIO pins for stepper motor
#define IN1 2
#define IN2 3
#define IN3 6
#define IN4 13
#define OPTO_PIN 28 // Optical sensor pin

// Stepper motor half-step sequence (28BYJ-48)
const bool step_sequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1},
};

uint current_step = 0;
uint steps_per_revolution = 4096;  // Set steps with half stepping
bool is_calibrated = false;

// Initialize GPIO pins
void init_gpio() {
    gpio_init(IN1);
    gpio_init(IN2);
    gpio_init(IN3);
    gpio_init(IN4);
    gpio_set_dir(IN1, GPIO_OUT);
    gpio_set_dir(IN2, GPIO_OUT);
    gpio_set_dir(IN3, GPIO_OUT);
    gpio_set_dir(IN4, GPIO_OUT);

    // Initialize optical sensor pin
    gpio_init(OPTO_PIN);
    gpio_set_dir(OPTO_PIN, GPIO_IN);
    gpio_pull_up(OPTO_PIN);  // Set pull-up to default high
}

// Function to set the motor coils based on the step sequence
void step_motor(uint step) {
    gpio_put(IN1, step_sequence[step][0]);
    gpio_put(IN2, step_sequence[step][1]);
    gpio_put(IN3, step_sequence[step][2]);
    gpio_put(IN4, step_sequence[step][3]);
}

// Move the stepper motor forward
void move_stepper_forward() {
    current_step = (current_step + 1) % 8;
    step_motor(current_step);
    sleep_ms(2);
}
void calibrate() {
    printf("Starting calibration...\n");

    int total_steps = 0;
    const int max_steps = steps_per_revolution * 2;  // Maximum steps to avoid loop
    const int tolerance = steps_per_revolution / 50;  // Error margin 2%

    for (int i = 0; i < 3; i++) {
        int steps = 0;
        bool cycle_complete = false;

        // Wait for the opto sensor to go low (0)
        while (gpio_get(OPTO_PIN) == 1) {
            move_stepper_forward();
            steps++;

            // Check for maximum steps
            if (steps > max_steps) {
                printf("Calibration error: Maximum step limit reached.\n");
                return;
            }
        }

        printf("Opto sensor triggered, counting steps...\n");

        // Count steps until the opto sensor goes high again (1)
        while (gpio_get(OPTO_PIN) == 0) {
            move_stepper_forward();
            steps++;

            if (steps > max_steps) {
                printf("Calibration error: Maximum step limit reached.\n");
                return;
            }
        }

        // Check if the number of steps is within acceptable range of full revolution
        printf("Completed one cycle, steps counted: %d\n", steps);

        if (steps >= steps_per_revolution - tolerance && steps <= steps_per_revolution + tolerance) {
            // If steps are close to the whole cycle consider valid
            total_steps += steps;
            cycle_complete = true;
        } else {
            // If steps didn't do one whole cycle, start over
            printf("Steps counted (%d) isn't full cycle. Starting calibrating.\n", steps);
        }

        if (cycle_complete) {
            sleep_ms(200);  // Stabilize sensor before next run
        } else {
            i--;  // Decrement to retry cycle
        }
    }

    // Calculate the average steps per revolution
    if (total_steps > 0) {
        steps_per_revolution = total_steps / 3;
        is_calibrated = true;
        printf("Calibration complete. Average steps per revolution: %u\n", steps_per_revolution);
    } else {
        printf("Calibration failed. No valid cycles detected.\n");
    }
}


int main() {
    stdio_init_all();
    init_gpio();
    printf("Stepper motor test program started.\n");

    while (true) {
        char command[16];
        printf("Enter command (status, calib, run [N]): ");
        if (scanf("%s", command) == 1) {
            if (strcmp(command, "status") == 0) {
                printf("\nCalibrated: %s\n", is_calibrated ? "Yes" : "No");
                if (is_calibrated) {
                    printf("Steps per revolution: %u\n", steps_per_revolution);
                } else {
                    printf("Steps per revolution: not available\n");
                }
            } else if (strcmp(command, "calib") == 0) {
                calibrate();
            } else if (strcmp(command, "run") == 0) {
                int n = 8;
                int step_count = 0;
                if (scanf("%d", &n) != 1) {
                    n = 8; // Default one full revolution if no value is provided
                }
                for (int i = 0; i < n * (steps_per_revolution / 8); i++) {
                    move_stepper_forward();
                    step_count++;
                }
                printf("Motor run complete with %d steps.\n", step_count);
            } else {
                printf("Unknown command.\n");
            }
        }
    }
}
