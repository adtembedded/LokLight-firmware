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

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint> // For standard integer types

/*

    Constants and Variables for LokLight configuration and state management

*/
// Configuration structure for LokLight. It is stored in Flash memory and loaded into RAM at startup.
// When CVs are written, the structure in RAM is updated and then saved back to Flash.
typedef struct llConfig_s {
    uint8_t  addrBasic;         // CV1     DCC Address (1-127 for short)
    uint8_t  addrExt1;          // CV17    DCC Address (128-10239 for long, this is the 256 multiplier. Addr = (addrExt1-192) * 256 + addrExt2)
    uint8_t  addrExt2;          // CV18    DCC Address (128-10239 for long, this is the added offset)
    uint8_t  addrMode;          // CV29    Addressing mode
    uint8_t  revDir;            // CV29    Reverse direction flag
    uint8_t  analogMode1;       // CV13    Analog mode configuration for F1 .. F8
    uint8_t  analogMode2;       // CV14    Analog mode configuration for F0F, F0R, F9 .. F12
    uint8_t  led1FuncMap;       // CV33..37 Function mapping for LED1 bit0=F0F, bit1=F0R, bit2=F1, bit3=F2, bit4=F3
    uint8_t  led2FuncMap;       // CV33..37 Function mapping for LED2
    uint8_t  led1Max;           // CV112   Maximum brightness level (0-255)
    uint8_t  led2Max;           // CV113   Maximum brightness level (0-255)
    uint8_t  led1Min;           // CV122   Minimum brightness level (0-255)
    uint8_t  led2Min;           // CV123   Minimum brightness level (0-255)
    uint8_t  led1Fade;          // CV114    Fade-in/out time, 255=1 second
    uint8_t  led2Fade;          // CV115    Fade-in/out time, 255=1 second
    uint8_t  led1Dir;           // CV116    LED1 direction sensitivity bit 0..1
    uint8_t  led2Dir;           // CV116    LED2 direction sensitivity bit 2..3
} llConfig_t;

// Default configuration values
const llConfig_t llConfigDefaults = {
    .addrBasic     = 3,        // Default DCC address 3
    .addrExt1      = 192,      // Default extended address high byte (192*256=49152)
    .addrExt2      = 0,        // Default extended address low byte
    .addrMode      = 0,        // Default to short address mode
    .revDir        = 0,        // Default no reverse direction
    .analogMode1   = 0,        // Default analog mode, F1 .. F8 are off
    .analogMode2   = 0b00000001,// Default analog mode for F0F, F0R, F9..F12. F0F is on as this is a front light by default, rest is off.
    .led1FuncMap   = 0b00000001,// Default LED1 function mapping to F0F(bit0), F0R, F1, F2, F3(bit4). F0F is on as this is a front light by default, rest is off.
    .led2FuncMap   = 0b00000001,// Default LED2 function mapping
    .led1Max       = 128,      // Default LED1 max brightness (half power)
    .led2Max       = 128,      // Default LED2 max brightness
    .led1Min       = 0,        // Default LED1 min brightness (fully off)
    .led2Min       = 0,        // Default LED2 min brightness
    .led1Fade      = 25,      // Default LED1 fade time (~0.1 seconds)
    .led2Fade      = 25,      // Default LED2 fade time (~0.1 seconds)
    .led1Dir       = 0b00,     // Default LED1 direction sensitivity: off. By default, the front light sensitivity is implemented by mapping to F0F.
    .led2Dir       = 0b00      // Default LED2 direction sensitivity: off
}; 

// Runtime state variables, stored in RAM only
typedef struct llStateVars_s {
    uint8_t  controlMode;       // Current control mode (DCC/Analog)
    uint16_t addr;              // Current DCC address
    int8_t   speed;             // Current speed step (-127 .. +127)
    uint8_t  led1Brightness;    // Current brightness level for LED1 (0-255)
    uint8_t  led2Brightness;    // Current brightness level for LED2 (0-255)
    uint8_t  led1Enabled;       // LED1 is commanded to be on or off
    uint8_t  led2Enabled;       // LED2 is commanded to be on or off
} llStateVars_t;


/*

    Enums

*/


#endif // CONFIG_H