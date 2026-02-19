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

bool LedControl::init(LedHwInitCfg_t* initHwCfg /*= nullptr*/)
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

    return false;
}

bool LedControl::step()
{
    if(!isInitialized_)
    {
        return false;
    }

    // Get current system time
    uint32_t currentTime = platform_get_tick_ms();
    
    // Calculate elapsed time since execution of this function
    // At start, this might be a large difference but that doesn't matter
    uint32_t elapsedTime = currentTime - lastStepTime_;
    if(elapsedTime == 0)
    {
        //Not a millisecond has passed, skip updating
        return true;
    }
    lastStepTime_ = currentTime;

    // Update brightness for each LED
    for(auto i = 0; i < LED_COUNT; ++i)
    {
        LedNumber_t ledNumber = static_cast<LedNumber_t>(i);
        // To check for ramps if we actually want to update this LED
        uint32_t elapsedTimeSinceLastLedUpdate = currentTime - ledUpdated_[i];

        // Use one larger size than brightness type so we can do calculations without overflow
        uint16_t currentBrightness = static_cast<uint16_t>(ledBrightness_[i]);
        uint16_t targetBrightness = ledEnabled_[i] ? ledControlCfg_[i].maxBrightness : ledControlCfg_[i].minBrightness;
        uint16_t brightnessRamp = ledControlCfg_[i].brightnessRamp;
        uint16_t max = ledControlCfg_[i].maxBrightness;
        uint16_t min = ledControlCfg_[i].minBrightness;

        // Only update if there's a difference and we have a ramp rate
        if(currentBrightness != targetBrightness && brightnessRamp > 0)
        {
            // Calculate brightness change based on elapsed time and brightnessStep
            // brightnessStep is the amount to change per millisecond
            // The factor 65 scales the brightnessStep to 0-255 for 0s to ~1s transitions
            // The factor (max-min)/255 scales the step relative to the max and min brightness
            uint32_t totalChange = (((max-min) * 65 * elapsedTimeSinceLastLedUpdate) / (brightnessRamp * 255));
            // If the change would be 0, skip it. If we do not do this and the code is fast enough, we would
            // only get 0-updates and the LEDs don't actually ramp
            // Rather, we wait until the elapsed time is large enough for an update
            if(totalChange == 0)
            {
                continue;
            }

            uint16_t newBrightness = targetBrightness;

            if(currentBrightness < targetBrightness)
            {
                // Ramp up
                newBrightness = currentBrightness + static_cast<uint16_t>(totalChange);
                if(newBrightness >= targetBrightness)
                {
                    newBrightness = targetBrightness;
                }
                ledBrightness_[i] = static_cast<uint8_t>(newBrightness);
            }
            else
            {
                // Ramp down
                if(currentBrightness > static_cast<uint16_t>(totalChange))
                {
                    newBrightness = currentBrightness - static_cast<uint16_t>(totalChange);
                }
                else
                {
                    newBrightness = 0;
                }
                ledBrightness_[i] = static_cast<uint8_t>(newBrightness);
            }
            // Keep track of time
            ledUpdated_[i] = currentTime;
        } // currentBrightness != targetBrightness && brightnessRamp > 0
        else
        {   // Either currentBrightness == targetBrightness or brightnessRamp == 0
            // Set target directly in either case
            ledBrightness_[i] = static_cast<uint8_t>(targetBrightness);
            ledUpdated_[i] = currentTime;
        }
        led_control_set_pwm(ledNumber, ledBrightness_[i]);
    }

    return true;
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
    if(isValidLed(ledNumber))
    {
        ledEnabled_[ledNumber] = enable;
    }
}

void LedControl::shortFlash(uint8_t times)
{
    for(uint8_t i = 0; i < times; ++i)
    {
        // Turn on all LEDs
        for(auto j = 0; j < LED_COUNT; ++j)
        {
            led_control_set_pwm(static_cast<LedNumber_t>(j), ledControlCfg_[j].maxBrightness);
        }
        platform_delay_ms(50); // Short duration, adjust as needed

        // Turn off all LEDs
        for(auto j = 0; j < LED_COUNT; ++j)
        {
            led_control_set_pwm(static_cast<LedNumber_t>(j), ledControlCfg_[j].minBrightness);
        }
        platform_delay_ms(50); // Short duration, adjust as needed
     }
}
void LedControl::longFlash(uint8_t times)
{
    for(uint8_t i = 0; i < times; ++i)
    {
        // Turn on all LEDs
        for(auto j = 0; j < LED_COUNT; ++j)
        {
            led_control_set_pwm(static_cast<LedNumber_t>(j), ledControlCfg_[j].maxBrightness);
        }
        platform_delay_ms(250); // Long duration, adjust as needed

        // Turn off all LEDs
        for(auto j = 0; j < LED_COUNT; ++j)
        {
            led_control_set_pwm(static_cast<LedNumber_t>(j), ledControlCfg_[j].minBrightness);
        }
        platform_delay_ms(250); // Long duration, adjust as needed
     }
}