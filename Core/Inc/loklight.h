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

#ifndef LOKLIGHT_H
#define LOKLIGHT_H

#include <cstdint>  // For standard integer types
#include "loklight_wrapper.h"   // For types
#include "config.h"
#include "dcc.h"
#include "led_control.h"       

// return type of init
typedef enum {
    LOKLIGHT_INIT_OK = 0,
    LOKLIGHT_INIT_ERROR = -1
} LoklightInitResult_t;

// return type of led config lookup
typedef struct ledCfgLookupResult_s {
    bool valid;
    ledControlCfg_t ledCfg;
} ledCfgLookupResult_t;

// LED directional sensitivity
// Note this definition corresponds to the bit encoding for loklight in CV116, it must not be changed
typedef enum LedDirectionSensitivity_e { 
    LED_DIR_NONE = 0, 
    LED_DIR_FWD = 1,
    LED_DIR_REV = 2, 
    LED_DIR_BOTH = 3
} LedDirectionSensitivity_t;

// The main Loklight class. This class is the managing parent of LED control through DCC operation.
//
// The init step reads the configuration from flash and initializes the LED controller (brightness, ramp) 
// and DCC reader (addr, control mode, function masking, etc) accordingly.
//
// The step function steps all the subcomponents (LED control, DCC reader) and applies any changes in DCC state to the LED control state.
// It also handles service requests (factory reset, CV writes) and applies them to the configuration and saves the settings to flash.

class Loklight
{
public:
    // This is a singleton class
    static Loklight& getInstance(){ 
        static Loklight instance; // created once
        return instance;
    }

    // For C-wrapper
    static Loklight* getInstancePtr(){
        return &getInstance();  // this works because a reference Loklight& is a name which points to the object itself
    }

    // Prevent copying and moving. This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    Loklight(const Loklight&) = delete;
    Loklight(Loklight&&) = delete;
    Loklight& operator=(const Loklight&) = delete;
    Loklight& operator=(Loklight&&) = delete;
    
    // Public interface
    bool isInitialized(){return isInitialized_;}
    LoklightInitResult_t init(LedHwInitCfg_t* ledHwInitCfg = nullptr, DccHwInitCfg_t* dccHwInitCfg = nullptr);
    bool step();

private:
    // This is a singleton class, make sure this object cannot be created except for getInstance
    Loklight();
    ~Loklight();

    LedControl& ledControl_;            // LED control instance
    LoklightConfig& loklightConfig_;    // Configuration instance
    DccInterface& dccInterface_;        // DCC interface instance

    bool isInitialized_ = false;

    // Led function mapping
    uint8_t led1FunctionMap_ = 0; // This variable is used to store the function mapping for LED1. It is function output 1 on the platform and can be mapped to F0F, F0R, F1..F3 through CVs 33-37
    uint8_t led2FunctionMap_ = 0; // This variable is used to store the function mapping for LED2. It is function output 2 on the platform and can be mapped to F0F, F0R, F1..F3 through CVs 33-37
    LedDirectionSensitivity_t led1DirMap_ = LED_DIR_NONE; // This variable is used to store the direction sensitivity for the outputs. It is determined by CV116 (F..R) and determines if the function output can be enabled or not for a certain direction. By default, off.
    LedDirectionSensitivity_t led2DirMap_ = LED_DIR_NONE; 
    void updateLedFunctionMapping(); // This function updates the led function mapping variables based on the configuration.
    void updateLedDirectionMapping();// This function updates the led direction mapping variables based on the configuration.
    ledCfgLookupResult_t getLedConfig(uint8_t ledNumber); // This function returns the LED control configuration for a given LED number based on the CV values in the config.
};

#endif // LOKLIGHT_H