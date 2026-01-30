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

#include "led_control.h"
#include "loklight_wrapper.h" // For init and hardware PWM control
#include <cstring>

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

bool LedControl::init(ledHwInitCfg_t* initHwCfg /*= nullptr*/)
{
    if(initHwCfg)
    {
        // Initialize PWM timer for LED control
        isInitialized_ = false;

        // Call the hardware-specific initialization function
        if(led_hw_init(initHwCfg))
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

bool LedControl::step()
{
    return false;
}

bool LedControl::setConfig(ledControlCfg_t* cfg, uint8_t ledNumber)
{
    if(cfg != nullptr && isValidLed(ledNumber))
    {

        std::memcpy(&ledControlCfg_[ledNumber], cfg, sizeof(ledControlCfg_t));
        return true;
    }

    return false;
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

void LedControl::enableLight(LedNumber_t ledNumber, bool enable)
{
    ;
}