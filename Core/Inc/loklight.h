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
    void updateLedFunctionMapping(); // This function updates the led function mapping variables based on the configuration.
};

#endif // LOKLIGHT_H