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

// // Runtime state variables, stored in RAM only
    // typedef struct llStateVars_s {
    //     uint8_t  controlMode;       // Current control mode (DCC/Analog)
    //     uint16_t addr;              // Current DCC address
    //     int8_t   speed;             // Current speed step (-127 .. +127)
    //     uint8_t  led1Brightness;    // Current brightness level for LED1 (0-255)
    //     uint8_t  led2Brightness;    // Current brightness level for LED2 (0-255)
    //     uint8_t  led1Enabled;       // LED1 is commanded to be on or off
    //     uint8_t  led2Enabled;       // LED2 is commanded to be on or off
    // } llStateVars_t;