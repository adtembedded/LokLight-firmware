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

// types
typedef struct ledControlCfg_s {
    uint8_t maxBrightness;
    uint8_t minBrightness;
    uint8_t brightnessRamp;
} ledControlCfg_t;

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

    bool init(LedHwInitCfg_t* initHwCfg = nullptr);
    bool step();
    bool setConfig(ledControlCfg_t* cfg, uint8_t ledNumber);
    void enableLight(LedNumber_t ledNumber, bool enable);
    bool isLightEnabled(LedNumber_t ledNumber) { return ledEnabled_[ledNumber]; }
    bool isInitialized() const { return isInitialized_; }
    uint8_t getBrightness(LedNumber_t ledNumber) const;
    
    private:
    //This class is a singleton
    LedControl(){;}
    ~LedControl(){;}
    
    ledControlCfg_s ledControlCfg_[LED_COUNT] = {};
    uint8_t ledBrightness_[LED_COUNT] = {};
    uint32_t ledUpdated_[LED_COUNT] = {};
    bool ledEnabled_[LED_COUNT] = {};
    uint32_t lastStepTime_ = 0;
    bool isInitialized_ = false;
    
    void setBrightness(LedNumber_t ledNumber, uint8_t brightness);
};

#endif