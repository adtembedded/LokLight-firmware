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
#include "config.h"

LoklightConfigInitResult_t LoklightConfig::init(void)
{
    isInitialized_ = false;

    //TODO
    
    isInitialized_ = true;
    return LL_CFG_INIT_NO_STORED_CFG_DEFAULTS_LOADED;
}

cvLookUpResult_t LoklightConfig::lookUpCV(uint16_t cvNumber) const
{
    for (const auto& entry : cvMap_) {
        if (entry.cvNumber == cvNumber) {
            return {true, entry.cvValue};
        }
    }
    return {false, 0};
}

uint16_t LoklightConfig::getFunctionOutputMask(DccFunctionOutputMap_t function) const
{
    //F0F, F0R, F1, F2, F3 can be mapped to platform specific outputs 1..8 through CV33-CV37
    //F4..F8  can be mapped to platform specific outputs 4..11 through CV38-CV42
    //F9..F12 can be mapped to platform specific outputs 7..14 through CV43-CV46
    
    uint16_t outputMask = 0;
    outputMask = lookUpCV(static_cast<uint16_t>(function)).cvValue; // CV number is the same as the function map enum value

    switch(function)
    {
        case DCC_FOMAP_F0F:
        case DCC_FOMAP_F0R:
        case DCC_FOMAP_F1:
        case DCC_FOMAP_F2:
        case DCC_FOMAP_F3:
            outputMask = outputMask & 0x00ff; // F0F, F0R, F1, F2, F3 to outputs 1..8
            break;
        case DCC_FOMAP_F4:
        case DCC_FOMAP_F5:
        case DCC_FOMAP_F6:
        case DCC_FOMAP_F7:
        case DCC_FOMAP_F8:
            outputMask = (outputMask << 3) & 0x07f8; // F4..F8 to outputs 4..11
            break;
        case DCC_FOMAP_F9:
        case DCC_FOMAP_F10:
        case DCC_FOMAP_F11:
        case DCC_FOMAP_F12:
            outputMask = (outputMask << 6) & 0x3fc0; // F9..F12 to outputs 7..14
            break;
    }
    
    return outputMask;
}