/*
*
* This program is licensed under the Polyform Noncommercial License, version 1.0.0.
* You should have received a copy of the Polyform License. If not, refer to 
* https://polyformproject.org/licenses/noncommercial/1.0.0/
*
* Required Notice: 
* LokLight - Copyright (C) 2026 ADT Embedded (http://www.adte.nl)
*
*/

// This header contains all types, definitions and functions to wrap the C++ loklight functionality 
// for C usage on a specific hardware platform (here: STM32L0xx)

#ifndef LOKLIGHT_WRAPPER_H
#define LOKLIGHT_WRAPPER_H

#include <sys/cdefs.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> // std types
#include <stdbool.h>

/*

    Forward declarations

*/
typedef struct LedControlInitCfg_s LedControlInitCfg_t;

/* 

    Loklight class

*/
// Opaque handle type for C
// typedef void* LoklightHandle;

// Create and destroy
// Not used as class is a singleton
// LoklightHandle loklight_create(void);
// void loklight_destroy(LoklightHandle handle);

// Get the handle of the singleton instance
// LoklightHandle loklight_get_instance(void);

// Initialization, call after creation and HW init
// bool loklight_init(LoklightHandle handle, LedControlInitCfg_t* ledInitCfg);
bool loklight_init(LedControlInitCfg_t* ledInitCfg);

// Check if init is complete
bool loklight_init_status();

// Step function, call periodically
// bool loklight_step(LoklightHandle handle);
bool loklight_step();

/* 

    Led control class

*/
// Set the number of LEDs supported by the hardware
// Make sure to start from 0 and count up 1 for every LED
#define LED_COUNT (2)

typedef enum {
    LED1 = 0,
    LED2 = 1
} LedNumber_t;

// This struct must contain all platform-specific configuration data needed for LED control PWM timer initialization
typedef struct LedControlInitCfg_s {
    uint8_t dummy; // Placeholder member variable
} LedControlInitCfg_t;

// Overwrite when using a specific hardware implementation
inline bool led_control_init(LedControlInitCfg_t* led_cfg)
{
    // Placeholder implementation
    // On STM32, this is done already in main while initializing
    // Therefore, assume the timer has been initialized sucessfully
    return true;
}

// Links a brightness change to a PWM update on the specific hardware
void led_control_set_pwm(LedNumber_t led_number, uint8_t brightness);

/*

    DCC Interface class

*/
// Function to add a DCC bit time to the queue 
// Call this function from an ISR that fires when the dcc track polarity is inverted, for ex. a GPIO IRQ that detects both up- and down-going flanks
// If the queue is full, this method will clear the queue completely to avoid processing inconsistent data
// Returns true when bit-time was succesfully added
// Returns false when the queue was full and has been cleared
bool dcc_bit_queue_add(uint32_t bit_time);

#ifdef __cplusplus
}
#endif

#endif // LOKLIGHT_WRAPPER_H