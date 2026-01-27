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

#include "led_control.h"
#include "loklight_wrapper.h" // For init and hardware PWM control

bool isValidLed(int value)
{ 
    if((value >= 0) && (value < LED_COUNT))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool LedControl::init(LedControlInitCfg_t* initCfg /*= nullptr*/)
{
    if(initCfg)
    {
        // Initialize PWM timer for LED control
        isInitialized_ = false;

        // Call the hardware-specific initialization function
        if(led_control_init(initCfg))
        {
            // Set all LEDs to off initially
            for(auto i = 0; i < LED_COUNT; ++i)
            {
                led_control_set_pwm(static_cast<LedNumber_t>(i), 0);
            }

            isInitialized_ = true;
        }
        return isInitialized_;
    }
    else
    {
        return false;
    }
}

void LedControl::setBrightness(LedNumber_t ledNumber, uint8_t brightness)
{
    if(isValidLed(ledNumber))
    {
        ledBrightness_[ledNumber] = brightness;
    }

    if(isInitialized_)
    {
        led_control_set_pwm(ledNumber, brightness);
    }
}

uint8_t LedControl::getBrightness(LedNumber_t ledNumber) const
{
    if(isValidLed(ledNumber))
    {
        return ledBrightness_[ledNumber];
    }

    return 0;
}