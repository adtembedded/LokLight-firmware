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

uint16_t LoklightConfig::getDecoderAddress() const
{
    uint16_t decoderAddress = 0;
    
    // The decoder ID is either stored in CV1 (short) or CV17,C18, according to CV29 bit 5.
    cvLookUpResult_t cv1Result = lookUpCV(1);
    cvLookUpResult_t cv17Result = lookUpCV(17);
    cvLookUpResult_t cv18Result = lookUpCV(18);
    cvLookUpResult_t cv29Result = lookUpCV(29);

    if(cv1Result.cvFound && cv17Result.cvFound && cv18Result.cvFound && cv29Result.cvFound)
    {
        if(cv29Result.cvValue & 0x20) // Check bit 5 of CV29
        {
            // DCC Address Extended DCC AddressAddr = (addrExt1-192) * 256 + addrExt2)
            // Check if the value in CV17 is valid (should be between 192 and 231 according to the standard)
            if(cv17Result.cvValue >= 192 && cv17Result.cvValue <= 231)
            {
                decoderAddress = (static_cast<uint16_t>(cv17Result.cvValue) - 192) * 256 + static_cast<uint16_t>(cv18Result.cvValue);
            } // No else, if CV17 is not valid, we just return 0 as the address, which indicates an error in the lookup
            
            //Now check if the address at most 9.999, which is normally the maximum allowed DCC address.
            if(decoderAddress < 128 || decoderAddress > 9999)
            {
                // If the address is invalid, reset it
                decoderAddress = 0;
            }
        }
        else
        {   // DCC Address Short (CV1 value between 1 and 127 according to the standard)
            decoderAddress = static_cast<uint16_t>(cv1Result.cvValue);
            if(decoderAddress < 1 || decoderAddress > 127)
            {
                // If the address is invalid, reset it
                decoderAddress = 0;
            }
        }
    }
    
    return decoderAddress; 
}

DccControlMode_t LoklightConfig::getDccControlMode() const
{
    DccControlMode_t controlMode = DCC_CONTROL_MODE_DCC_128SS; // Default to 128SS if lookup fails
    // The DCC control mode is set in CV29 bit 1 (0 for 14 speed steps DCC and 1 for everything else)
    cvLookUpResult_t cv29Result = lookUpCV(29);
    if(cv29Result.cvFound)
    {
        if(cv29Result.cvValue & 0x02) // Check bit 1 of CV29
        {
            return DCC_CONTROL_MODE_DCC_128SS; // If bit 1 is set, we are in 28 or 128 speed step mode. We treat both the same and use the same control mode
        }
        else
        {
            return DCC_CONTROL_MODE_DCC_14SS; // If bit 1 is not set, we are in 14 speed step mode
        }
    }   

    return controlMode;
}