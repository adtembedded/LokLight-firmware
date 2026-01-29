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

#ifndef DCC_H
#define DCC_H

#include "loklight_wrapper.h"   // This is where generic loklight functions are bound to platform-dependent methods
#include <array>               // As memory-efficient queue between ISR and dcc processing w.r.t. bit-timing on tracks

constexpr uint32_t DCC_BITTIME_QUEUE_SIZE = 32; // Size of the DCC bit time queue. Assume processing happens at least once per this number of polarity transitions

// // Runtime state variables, stored in RAM only
    // typedef struct llStateVars_s {
    //     uint8_t  controlMode;       // Current control mode (DCC/Analog)
    //     direction 
    //     uint16_t addr;              // Current DCC address
    //     int8_t   speed;             // Current speed step (-127 .. +127)
    // } llStateVars_t;

class DccInterface
{
public:
    // This is a singleton class
    static DccInterface& getInstance(){ 
        static DccInterface instance; // created once
        return instance;
    }

    bool init();

    //Adds a bit-time to the queue. 
    //If the queue was full, an internal error flag is set and the dcc reader will re-initialize to avoid processing inconsistent data
    bool addBitTime(uint32_t t);
    //TODO move to private
    uint32_t readBitTime(); //returns the next bit for processing if one is available. Returns 0 when the queue was empty

private:
    // This is a singleton class, make sure this object cannot be created except for getInstance
    DccInterface();
    ~DccInterface();


    bool isInitialized_ = false;

    //Bit-time Queue
    std::array<uint32_t, DCC_BITTIME_QUEUE_SIZE> dccBitTimeQueue_; // Queue to store incoming DCC bits from ISR for processing in main loop
    uint16_t qReadIdx_ = 0;     //read location to extract bit-times
    volatile uint16_t qWriteIdx_ = 0;    //write location to push bit-time. Volatile tells the compiler the value can be changed at any moment, and some code optimization should be skipped
    bool qIsFull_ = false;       //Indicates there are no more spaces in the array to write bit-times to
    bool qErrorFlag_ = false;    //Indicates a write has been attempted while there was no more space in the bit-time array
    bool qIsEmpty_ = true;       //Indicates whether or not new bit-time data is available
};

#endif // DCC_H