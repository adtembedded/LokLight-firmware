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

#include "loklight.h"

Loklight::Loklight()
    :   ledControl_(LedControl::getInstance()), // Initialize reference to singleton instance
        loklightConfig_(LoklightConfig::getInstance()),
        dccInterface_(DccInterface::getInstance())
{
}

Loklight::~Loklight()
{
}

LoklightInitResult_t Loklight::init(LedHwInitCfg_t* ledHwInitCfg, DccHwInitCfg_t* dccHwInitCfg)
{
    // Validate input
    if(ledHwInitCfg == nullptr || dccHwInitCfg == nullptr)
    {
        return LOKLIGHT_INIT_ERROR;
    }

    // Initialize configuration
    // Load configuration from Flash or set defaults
    //TODO
    //For now, load led defaults
    for(auto i = 0; i < LED_COUNT; ++i)
    {
        ledControlCfg_t ledCfg = {128, 0, 25};
        ledControl_.setConfig(&ledCfg, static_cast<LedNumber_t>(i));
    }
    
    // Initialize DCC decoder
    bool dccInitResult = dccInterface_.init(dccHwInitCfg);
    if(!dccInitResult)
    {
        return LOKLIGHT_INIT_ERROR;
    }

    // Initialize LED control
    // This automatically sets brightness to 0 for all LEDs
    bool ledInitResult = ledControl_.init(ledHwInitCfg);
    if(!ledInitResult)
    {
        return LOKLIGHT_INIT_ERROR;
    }

    isInitialized_ = true;
    return LOKLIGHT_INIT_OK;
}

bool Loklight::step()
{
    // Perform periodic tasks
    
    // First check if we are initialized
    if(!ledControl_.isInitialized())
    {
        return false; // Not initialized
    }
    
    // Check for new DCC commands, update LED states, etc.
    
    if(!dccInterface_.step())
    {
        return false; // DCC step failed
    }

    // Set LED status according to settings and DCC state
    bool enableLed1 = false, enableLed2 = false;
    DccControlMode_t controlMode = dccInterface_.getControlMode();
    if(controlMode == DCC_CONTROL_MODE_DCC_14SS || controlMode == DCC_CONTROL_MODE_DCC_128SS)
    {
        // TODO check config registers to determine which functions control the LEDs
        // For now, set on for F0F as front light
        uint16_t activeFuncs = dccInterface_.getActiveFuncs();
        if(activeFuncs & DCC_FUNC_F0F)
        {
            enableLed1 = true;
            enableLed2 = true;
        }
    }
    else if(controlMode == DCC_CONTROL_MODE_ANALOG)
    {
        //TODO
    }
    else
    {
        //This should never happen, but if it does, disable all LEDs
        enableLed1 = false;
        enableLed2 = false;
    }

    // Update LED states
    ledControl_.enableLight(LED1, enableLed1);
    ledControl_.enableLight(LED2, enableLed2);

    if(!ledControl_.step())
    {
        return false; // Return false if step failed, we should reset the device if this happens
    }

    return true; // Return true if step was successful
}