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
    bool configInitResult = loklightConfig_.init() != LL_CFG_INIT_ERROR;
    if(!configInitResult)    {
        return LOKLIGHT_INIT_ERROR;
    }

    // Set the function output mapping & direction mapping based on the config.
    updateLedFunctionMapping();
    updateLedDirectionMapping();

    // Set the LED config
    for(uint8_t i = LED1; i <= LED2; ++i)
    {
        ledCfgLookupResult_t ledCfgResult = getLedConfig(i);
        if(!ledCfgResult.valid)
        {
            // If config lookup failed, disable LED
            ledCfgResult.ledCfg = {0, 0, 0}; 
        }
        ledControl_.setConfig(&ledCfgResult.ledCfg, static_cast<LedNumber_t>(i));
    }
    
    // Collect the DCC config
    DccConfig_t dccConfig = {};
    dccConfig.addr = loklightConfig_.getDecoderAddress(); //Note that is this function fails, the 0 or broadcast address is set, which is safe.
    dccConfig.controlMode = loklightConfig_.getDccControlMode(); // Defaults to 128SS if lookup fails
    dccConfig.direction = loklightConfig_.getDirection(); // Defaults to normal direction if lookup fails
    dccConfig.analogFuncMask = loklightConfig_.getAnalogFuncMask(); // Defaults to 0 if lookup fails

    // Initialize DCC decoder
    bool dccInitResult = dccInterface_.init(dccHwInitCfg, &dccConfig);
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
    
    // Update LED enabled according to its function map and the active functions
    uint16_t activeFuncs = dccInterface_.getActiveFuncs();
    enableLed1 = (activeFuncs & led1FunctionMap_) != 0;
    enableLed2 = (activeFuncs & led2FunctionMap_) != 0;

    // Update LED enabled according to direction
    DccDirection_t direction = dccInterface_.getDirection();
    if(direction == DCC_DIRECTION_FORWARD)
    {
        // Note: the LED_DIR_XX enum values follow a bitmap [bit1: rev, bit0: fwd]
        // That is to say, if LED_DIR_ALL is set, the bit for LED_DIR_FWD is enabled
        // and the expression below will evaluate to true.
        enableLed1 = enableLed1 && (led1DirMap_ & LED_DIR_FWD);
        enableLed2 = enableLed2 && (led2DirMap_ & LED_DIR_FWD);
    }
    else if(direction == DCC_DIRECTION_REVERSE)
    {
        enableLed1 = enableLed1 && (led1DirMap_ & LED_DIR_REV);
        enableLed2 = enableLed2 && (led2DirMap_ & LED_DIR_REV);
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

void Loklight::updateLedFunctionMapping()
{
    // This function iterates the rows of the function map (refer to NMRA S9.2.2 Table 2)
    // Every CV contains a row, which maps one function to 8 function outputs.
    // For example row CV33 maps F0F to outputs 1..8.
    // This function translates that to a row where one function outputs is linked to a set of functions.
    // I.e. one bitmaks maps function output 1 or 2 to functions F0F, F0R, F1..F3. This is used in the step function to determine which LEDs to turn on based on which functions are active in the DCC messages.
    
    // First set LED1, then LED2. They are function output 1 and 2 on the platform and can be mapped to F0F, F0R, F1..F3 through CVs 33-37
    led1FunctionMap_ = 0;
    led2FunctionMap_ = 0;
    for(uint8_t function = DCC_FOMAP_F0F; function <= DCC_FOMAP_F3; ++function)
    {
        uint16_t outputMask = loklightConfig_.getFunctionOutputMask(static_cast<DccFunctionOutputMap_t>(function));
        if(outputMask & 0x01) // Check if function maps to output 1
        {
            // Set corresponding bit in led1FunctionMap_
            // Bit0 maps to F0F, bit1 maps to F0R, bit2 maps to F1, bit3 maps to F2, bit4 maps to F3
            led1FunctionMap_ |= (1 << (function - DCC_FOMAP_F0F));
        }
        if(outputMask & 0x02) // Check if function maps to output 2
        {
            // Set corresponding bit in led2FunctionMap_
            // Bit0 maps to F0F, bit1 maps to F0R, bit2 maps to F1, bit3 maps to F2, bit4 maps to F3
            led2FunctionMap_ |= (1 << (function - DCC_FOMAP_F0F));
        }
    }
}

void Loklight::updateLedDirectionMapping()
{
    //This function collects the content of CV116 and maps it to the LEDs
    cvLookUpResult_t cv116Result = loklightConfig_.lookUpCV(116); 
    if(cv116Result.cvFound)
    {
        //bits 0 and 1 are for LED1
        led1DirMap_ = static_cast<LedDirectionSensitivity_t>(cv116Result.cvValue & 0x03); // Mask bits 0 and 1 
        //bits 2 and 3 are for LED2 
        led2DirMap_ = static_cast<LedDirectionSensitivity_t>((cv116Result.cvValue >> 2) & 0x03); // Shift right by 2 and mask bits 0 and 1 } else { // If CV116 is not found, default to off for both LEDs, which is already set in the member variable initialization. }
    }
    else
    {
        // If not found, default to off for both LEDs
        led1DirMap_ = LED_DIR_NONE; 
        led2DirMap_ = LED_DIR_NONE;
    }
}

ledCfgLookupResult_t Loklight::getLedConfig(uint8_t ledNumber)
{
    // This function returns the LED control configuration for a given LED number based on the CV values in the config.
    // For example, CV112 contains the max brightness for LED1, CV122 contains the min brightness for LED1, etc.
    // If no CVs are found for the LED, default values are returned (max brightness 128, min brightness 0, ramp 25)
    ledControlCfg_t cfg = {0, 0, 0}; // Default values in case of error
    bool errorInLookup = false;

    uint16_t maxBrightnessCV = (ledNumber == LED1) ? 112 : 113;
    uint16_t minBrightnessCV = (ledNumber == LED1) ? 122 : 123;
    uint16_t rampCV = (ledNumber == LED1) ? 114 : 115;

    cvLookUpResult_t maxBrightnessResult = loklightConfig_.lookUpCV(maxBrightnessCV);
    if(maxBrightnessResult.cvFound)
    {
        cfg.maxBrightness = maxBrightnessResult.cvValue;
    }
    else
    {
        errorInLookup = true;
    }

    cvLookUpResult_t minBrightnessResult = loklightConfig_.lookUpCV(minBrightnessCV);
    if(minBrightnessResult.cvFound)
    {
        cfg.minBrightness = minBrightnessResult.cvValue;
    }
    else
    {
        errorInLookup = true;
    }

    cvLookUpResult_t rampResult = loklightConfig_.lookUpCV(rampCV);
    if(rampResult.cvFound)
    {
        cfg.brightnessRamp = rampResult.cvValue;
    }
    else    {
        errorInLookup = true;
    }

    if(errorInLookup)
    {
        // If any of the CV lookups failed, return default values with valid=false
        return {false, {0, 0, 0}};
    }
    
    return {true, cfg};
}