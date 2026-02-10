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
#include <map>

/*

    Constants and Variables for LokLight configuration and state management

*/

/*

    Enums

*/
typedef enum {
    LL_CFG_INIT_ERROR = -1,
    LL_CFG_INIT_STORED_CFG_LOADED = 0,
    LL_CFG_INIT_NO_STORED_CFG_DEFAULTS_LOADED = 1
} LoklightConfigInitResult_t;

/* 

    Classes 
    
*/

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

private:
    // This is a singleton class, make sure this object cannot be created except for getInstance
    LoklightConfig(){;}
    ~LoklightConfig(){;}

    bool isInitialized_ = false;
    static std::map<uint16_t, uint8_t> cvMap_;
};

/* 

    CV/Config Types

*/
// This CV Map is part of the config class. This initialization is used to set defaults.
// Change values / insert / delete as needed
inline std::map<uint16_t, uint8_t> LoklightConfig::cvMap_ { 
    {1, 3},     // CV1: DCC Address Basic (1-127 for short)
    {17, 192},  // CV17: DCC Address Extended 1 DCC Address (128-10239 for long, this is the 256 multiplier. Addr = (addrExt1-192) * 256 + addrExt2)
    {18, 0},    // CV18: DCC Address Extended 2 DCC Address (128-10239 for long, this is the added offset)
    {29, 4},    // CV29: Configuration register
                        // Bit0 Travel dir: 0 Normal direction of travel, 1 Reversed direction of travel
                        // Bit1 Speed config: 0 for 14 speed steps DCC and FL = bit4 of dcc speed data, 2 for 28 or 128 speed steps DCC and FL is bit 4 in function group 1 instruction message.
                        // Bit2 Analog operation: 0 Disable analog operation, 4 Enable analog operation
                        // Bit3 UNUSED RailCom: 0 to Disable RailCom®, 8 to Enable RailCom®
                        // Bit4 UNUSED Speed curve: 0 for curve through CV 2, 5, 6; 16 for curve through CV 67-94
                        // Bit5 Addressing mode: 0 Short addresses (CV 1) in DCC mode, 32 Long addresses (CV 17 + 18) in DCC mode
                        // Bit6 UNUSED Reserved
                        // Bit7 UNUSED Accessory Decoder: 0 for multipurpose decoder, 128 for accessory decoder
    {13, 1},    // CV13: Analog mode F1..F8. 1 is on, 0 is off during analog mode. (bit0 = F1, bit1 = F2, ..., bit7 = F8)
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
    {114, 25},  // CV114: LED1 Fade time. Fade-in/out time. 0=instant, 255=1 second, scaling is linear
    {115, 25},  // CV115: LED2 Fade time
    {116, 15}   // CV116: LED Direction sensitivity. 
                // LED1 direction sensitivity bit 0 F..1 R; By default set to 3 (sensitive to both directions)
                // LED2 direction sensitivity bit 2 F..3 R; By default set to 3<<2 (sensitive to both directions)
};


#endif // CONFIG_H