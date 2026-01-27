/*
* LokLight
* Copyright (C) 2026 ADT Embedded
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This header contains all types, definitions and functions to wrap the C++ loklight functionality 
// for C usage on a specific hardware platform (here: STM32L0xx)

#ifndef LOKLIGHT_WRAPPER_H
#define LOKLIGHT_WRAPPER_H

#include <sys/cdefs.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> // bool

/* 

    Loklight class

*/
// Opaque handle type for C
typedef void* LoklightHandle;

// Create and destroy
LoklightHandle loklight_create(void);

void loklight_destroy(LoklightHandle handle);

// Initialization, call after creation and HW init
bool loklight_init(LoklightHandle handle, uint32_t a);

/* 

    Led control class

*/
typedef enum {
    LED1 = 0,
    LED2 = 1
} LedNumber_t;

// This struct must contain all platform-specific configuration data needed for LED control PWM timer initialization
typedef struct LedControlInitCfg_s {
    uint8_t dummy; // Placeholder member variable
} LedControlInitCfg_t;

// Overwrite when using a specific hardware implementation
inline bool led_control_init(LedControlInitCfg_t led_cfg)
{
    // Placeholder implementation
    // On STM32, this is done already in main while initializing
    // Therefore, assume the timer has been initialized sucessfully
    return true;
}

// Links a brightness change to a PWM update on the specific hardware
void led_control_set_pwm(LedNumber_t ledNumber, uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif // LOKLIGHT_WRAPPER_H