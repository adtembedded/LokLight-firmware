# Loklight-firmware
The firmware for Loklight, a light for model railways (H0) that can be DCC controlled as digital drop-in replacement for MS4 &amp; E5.5 lightbulbs

When upgrading older locomotives from analog to digital DCC operation through a decoder, the onboard glowbulbs must be rewired for proper DCC operation. 
Particularly in the case of steam engine models, this requires adding wires from the tender to the front of the locomotives. 
This prevents the model from being used in the normal way where tender and loc can be seperated by the coupling mechanism. Also, the wires could be visible from outside.

The Loklight project undertakes to provide the hardware and software for a DCC controlled LED PCB that can be used as a drop-in replacement for glowbulbs in the commonly used sizes.

This repository contains the firmware for Loklight. It is composed of a C++ application with C-bindings to the STM32 HAL. 
It contains functionality for DCC decoding, CV register management and control of the LEDs. The firmware is developed for an STM32C011 MCU.


Please note that the firmware is under construction and this project is not yet ready for implementation!
