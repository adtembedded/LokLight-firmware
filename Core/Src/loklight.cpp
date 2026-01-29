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
    : ledControl_(LedControl::getInstance()), // Initialize reference to singleton instance
      loklightConfig_(LoklightConfig::getInstance())
{
}

Loklight::~Loklight()
{
}

LoklightInitResult_t Loklight::init(LedControlInitCfg_t* ledInitCfg /*= nullptr*/)
{
    // Validate input
    if(ledInitCfg == nullptr)
    {
        return LOKLIGHT_INIT_ERROR;
    }

    // Initialize configuration
    // Load configuration from Flash or set defaults
    //TODO
    
    // Initialize DCC decoder
    //TODO

    // Initialize LED control
    // This automatically sets brightness to 0 for all LEDs
    bool ledInitResult = ledControl_.init(ledInitCfg);
    if(!ledInitResult)
    {
        return LOKLIGHT_INIT_ERROR;
    }

    return LOKLIGHT_INIT_OK;
}

bool Loklight::step()
{
    // Perform periodic tasks
    
    // First check if we are initialized
    if(!this->ledControl_.isInitialized())
    {
        return false; // Not initialized
    }
    
    // Check for new DCC commands, update LED states, etc.
    
    //TODO
    
    //Dummy code to control LEDs
    static uint8_t brightness = 0;
    this->ledControl_.setBrightness(LED1, brightness);
    this->ledControl_.setBrightness(LED2, 255 - brightness++);

    return true; // Return true if step was successful
}