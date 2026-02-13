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

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint> // For standard integer types
#include "dcc_defs.h" // For DCC related types

/*

    Constants and Variables for LokLight configuration and state management

*/

/*

    Enums

*/
typedef enum LoklightConfigInitResult_e : int8_t {
    LL_CFG_INIT_ERROR = -1,
    LL_CFG_INIT_STORED_CFG_LOADED = 0,
    LL_CFG_INIT_NO_STORED_CFG_DEFAULTS_LOADED = 1
} LoklightConfigInitResult_t;

typedef enum DccFunctionOutputMap_e: uint8_t {
    DCC_FOMAP_F0F = 33,     //Maps function to platform specific output through its corresponding CV.
    DCC_FOMAP_F0R = 34,
    DCC_FOMAP_F1 = 35,
    DCC_FOMAP_F2 = 36,
    DCC_FOMAP_F3 = 37,
    DCC_FOMAP_F4 = 38,
    DCC_FOMAP_F5 = 39,
    DCC_FOMAP_F6 = 40,
    DCC_FOMAP_F7 = 41,
    DCC_FOMAP_F8 = 42,
    DCC_FOMAP_F9 = 43,
    DCC_FOMAP_F10 = 44,
    DCC_FOMAP_F11 = 45,
    DCC_FOMAP_F12 = 46
} DccFunctionOutputMap_t;

/* 

    Classes 
    
*/
typedef struct cvEntry_s {
    uint16_t cvNumber;
    uint8_t cvValue;
} cvEntry_t;

typedef struct cvLookUpResult_s {
    bool cvFound;
    uint8_t cvValue;
} cvLookUpResult_t;

class LoklightConfig
{
public:
    // This is a singleton class
    static LoklightConfig& getInstance(){ 
        static LoklightConfig instance; // created once
        return instance;
    } 

    // Prevent copying and moving. 
    // This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    LoklightConfig(const LoklightConfig&) = delete;
    LoklightConfig(LoklightConfig&&) = delete;
    LoklightConfig& operator=(const LoklightConfig&) = delete;
    LoklightConfig& operator=(LoklightConfig&&) = delete;

    // Read from flash into RAM
    // If no valid config is found in flash, load defaults
    LoklightConfigInitResult_t init(void);
    // Lookup a CV value in the map
    cvLookUpResult_t lookUpCV(uint16_t cvNumber) const;

    // Specific DCC getters. Translates between DCC entity and CV value according to the standard.
    // Get the function outputs that a dcc function should enable. If no CVs are found within the config, this function will return a 0 mask.
    uint16_t getFunctionOutputMask(DccFunctionOutputMap_t function) const;
    // Get the decoder address. If the lookup fails, returns 0
    uint16_t getDecoderAddress() const;
    // Get the DCC control mode. Defaults to 128SS when lookup fails
    DccControlMode_t getDccControlMode() const;
    // Get the direction reversal setting. Defaults to normal direction when lookup fails
    DccDirection_t getDirection() const;
    // Get the analog function mask. Every bit corresponds to a function, bit0 is F0F, bit1 is F0R, bit2 is F1, etc. If the lookup fails, returns 0 (no functions on in analog mode)
    uint16_t getAnalogFuncMask() const;

private:
    // This is a singleton class, make sure this object cannot be created except for getInstance
    LoklightConfig(){;}
    ~LoklightConfig(){;}

    bool isInitialized_ = false;
    static cvEntry_t cvMap_[];
};

/* 

    CV/Config Types

*/
// This CV Map is part of the config class. This initialization is used to set defaults.
// Change values / insert / delete as needed
inline cvEntry_t LoklightConfig::cvMap_[] = {
    {1, 3},     // CV1: DCC Address Basic (1-127 for short)
    {17, 194},  // CV17: DCC Address Extended 1 DCC Address (128-10239 for long, this is the 256 multiplier. Addr = (addrExt1-192) * 256 + addrExt2)
    {18, 48},    // CV18: DCC Address Extended 2 DCC Address (128-10239 for long, this is the added offset)
    {29, 38},    // CV29: Configuration register
                        // Bit0 Travel dir: 0 Normal direction of travel, 1 Reversed direction of travel
                        // Bit1 Speed config: 0 for 14 speed steps DCC and FL = bit4 of dcc speed data, 2 for 28 or 128 speed steps DCC and FL is bit 4 in function group 1 instruction message.
                        // Bit2 Analog operation: 0 Disable analog operation, 4 Enable analog operation
                        // Bit3 UNUSED RailCom: 0 to Disable RailCom®, 8 to Enable RailCom®
                        // Bit4 UNUSED Speed curve: 0 for curve through CV 2, 5, 6; 16 for curve through CV 67-94
                        // Bit5 Addressing mode: 0 Short addresses (CV 1) in DCC mode, 32 Long addresses (CV 17 + 18) in DCC mode
                        // Bit6 UNUSED Reserved
                        // Bit7 UNUSED Accessory Decoder: 0 for multipurpose decoder, 128 for accessory decoder
    {13, 0},    // CV13: Analog mode F1..F8. 1 is on, 0 is off during analog mode. (bit0 = F1, bit1 = F2, ..., bit7 = F8)
    {14, 3},    // CV14: Analog mode F0F, F0R, F9..F12 (bit0 = F0F, bit1 = F0R, bit2 = F9, bit3 = F10, bit4 = F11, bit5 = F12)
    {33, 3},    // CV33: Function map. Maps F0 forward to outputs. FO1 and FO2 are the LEDs and normally they are linked only to forward motion
    {34, 0},    // CV34: Function map. F0 backward. Refer to https://www.nmra.org/sites/default/files/s-9.2.2_2012_10.pdf
    {35, 0},    // CV35: Function map. F1
    {36, 0},    // CV36: Function map. F2
    {37, 0},    // CV37: Function map. F3
    {112, 128}, // CV112: LED1 Max brightness. Set to half power by default
    {113, 128}, // CV113: LED2 Max brightness
    {122, 0},   // CV122: LED1 Min brightness. Set to fully off by default
    {123, 0},   // CV123: LED2 Min brightness
    {114, 10},  // CV114: LED1 Fade time. Fade-in/out time. 0=instant, 255=1 second, scaling is linear
    {115, 10},  // CV115: LED2 Fade time
    {116, 15}   // CV116: LED Direction sensitivity. 
                // LED1 direction sensitivity bit 0 F..1 R; By default set to 3 (sensitive to both directions)
                // LED2 direction sensitivity bit 2 F..3 R; By default set to 3<<2 (sensitive to both directions)
};


#endif // CONFIG_H