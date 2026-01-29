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

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "loklight_wrapper.h"   // This is where generic loklight functions are bound to platform-dependent methods

// class led
class LedControl
{
public:
    // Singleton access
    static LedControl& getInstance()
    {
        static LedControl instance;
        return instance;
    }

    // Prevent copying and moving. This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    LedControl(const LedControl&) = delete;
    LedControl(LedControl&&) = delete;
    LedControl& operator=(const LedControl&) = delete;
    LedControl& operator=(LedControl&&) = delete;

    bool init(LedControlInitCfg_t* initCfg = nullptr);
    bool isInitialized() const { return isInitialized_; }
    void setBrightness(LedNumber_t ledNumber, uint8_t brightness);
    uint8_t getBrightness(LedNumber_t ledNumber) const;

private:
    uint8_t ledBrightness_[LED_COUNT] = {};
    bool isInitialized_ = false;

    //This class is a singleton
    LedControl(){;}
    ~LedControl(){;}
};

#endif