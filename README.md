# Loklight-firmware
The firmware for Loklight, a light for model railways (H0) that can be DCC controlled as digital drop-in replacement for MS4 &amp; E5.5 lightbulbs

When upgrading older locomotives from analog to digital DCC operation through a decoder, the onboard glowbulbs must be rewired for proper DCC operation. 
Particularly in the case of steam engine models, this requires adding wires from the tender to the front of the locomotives. 
This prevents the model from being used in the normal way where tender and loc can be seperated by the coupling mechanism. Also, the wires could be visible from outside.

The Loklight project undertakes to provide the hardware and software for a DCC controlled LED PCB that can be used as a drop-in replacement for glowbulbs in the commonly used sizes.

This repository contains the firmware for Loklight. It is composed of a C++ application with C-bindings to the STM32 HAL. 
It contains functionality for DCC decoding, CV register management and control of the LEDs. The firmware is developed for an STM32C011 MCU.


# Supported DCC controllers
The firmware has been developed according to the NMRA DCC standards and tested with a Roco Lokmaus controller, with support for all speed modes (14/28/128SS). To configure the settings through CV writes, this controller uses service mode to access CVs through the Direct Mode method.

While this should be compatible with many "normal" DCC controllers, these are untested as of yet.

# Steps to use firmware
- Use an IDE and GCC toolchain to build the firmware. VSCode and the STM32CubeMX toolchains have been used to develop the firmware (automatically installs GCC ARM toolchain)
- Flash the firmware with a suitable debugger (ST-Link, Segger J-Link, ..) and tag connect tc2030 debug cable, using the programming breakout section of the PCB.
- If you want to apply this firmware to another hardware platform, modify the platform-specific functionality in loklight_wrapper.c and .h accordingly. 
- The loklight PCB can always be reset to default CV values by writing CV8=8 or CV777=119.

# Debugging
- Refer to the schematics and the loklight-hardware repo for pcb documentation
- If debugging is required, note that Segger RTT is used for printing. Configure symbols PLATFORM_DEBUGGING, BUFFER_SIZE_UP and in main.c make sure SEGGER_RTT_ConfigUpBuffer(.., SEGGER_RTT_MODE_xxx) is set to a suitable config (block or no block)
- If you want to use another method of communication, USART2 can be used. The Rx/Tx and GND pads are located on the programming breakout PCB.
- A stable power supply can be provided using VCC and GND pads. The VCC pad is located at the power supply on the loklight part of the PCB. Use a diode between your power supply and the VCC connection if you are going to simultaneously use a DCC controller in conjunction with a fixed power supply, so no short-circuits between them exist.

# Known issues and improvements
- Not all required DCC functionality has been implemented. For example, CVs 3 and 4 that control acceleration and deceleration rates are meaningless for this project and have therefore been omitted.
- Programming on the main track is not supported. While this should be relatively straightforward to implement, the DCC controller used for development of the firmware did not have this feature.
- Any other CV access method than Direct Mode in Service Mode is not supported.
- Use in consist configuration is not supported.
- Only CV write operations are supported, reading, verification and bit manipulations are not supported.
- There is no possibility for bidirectional communication with the DCC controller because of space constrains on the PCB
