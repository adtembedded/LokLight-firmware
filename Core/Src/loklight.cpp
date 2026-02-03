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
    
    //TODO

    //Dummy code for dcc read, discard result
    // static uint32_t bitsRead = 0;
    // while(dccInterface_.elementsInQueue() > 0)
    // {
    //     dccInterface_.readBitTime();
    //     bitsRead++;
    //     if(bitsRead > 25000)
    //     {
    //         ledControl_.enableLight(LED1, !ledControl_.isLightEnabled(LED1));
    //         ledControl_.enableLight(LED2, !ledControl_.isLightEnabled(LED2));
    //         bitsRead = 0;
    //     }
    // }

    // Update LED states
    if(!ledControl_.step())
    {
        return false; // Return false if step failed, we should reset the device if this happens
    }

    return true; // Return true if step was successful
}