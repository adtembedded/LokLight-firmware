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

#include "loklight.h"

Loklight::Loklight()
{
    // Constructor implementation (if needed)
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