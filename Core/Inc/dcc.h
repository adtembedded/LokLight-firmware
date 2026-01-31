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

// Size of the DCC bit time queue. Assume processing happens at least once per this number of polarity transitions
// There can be up to 20 transitions per ms, so make this buffer large enough
constexpr uint32_t DCC_BITTIME_QUEUE_SIZE = 128; 

// Expected ticks for DCC bit transitions
// Refer to https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.1_electrical_standards_for_digital_command_control.pdf
// We use timer frequency and include the tolerance to calculate the min and max ticks for each bit type
constexpr uint32_t DCC_BITTIME_T1_MIN = (uint32_t)((DCC_TIMER_FREQ_MIN*52ull)/1000000ull);            // 52us for halfbit
constexpr uint32_t DCC_BITTIME_T1_MAX = (uint32_t)((DCC_TIMER_FREQ_MAX*64ull)/1000000ull);            // 64us for halfbit
constexpr uint32_t DCC_BITTIME_T1_MAX_DELTA = (uint32_t)((DCC_TIMER_FREQ_MAX*6ull)/1000000ull);       // 6us for max time difference between two "1"-half-bits
constexpr uint32_t DCC_BITTIME_T0_MIN = (uint32_t)((DCC_TIMER_FREQ_MIN*90ull)/1000000ull);            // 90us for halfbit
constexpr uint32_t DCC_BITTIME_T0_MAX = (uint32_t)((DCC_TIMER_FREQ_MAX*10000ull)/1000000ull);         // 10.000us for halfbit
constexpr uint32_t DCC_BITTIME_T0_MAX_TOTAL = (uint32_t)((DCC_TIMER_FREQ_MAX*12000ull)/1000000ull);   // 12.000us For total bit (two "0"-half bits with 0 stretching)

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

    bool init(DccHwInitCfg_t* initHwCfg = nullptr);

    //Adds a bit-time to the queue. 
    //If the queue was full, an internal error flag is set and the dcc reader will re-initialize to avoid processing inconsistent data
    bool addBitTime(uint32_t t);

    //Check if there is something to read
    uint32_t elementsInQueue();

    //TODO move to private
    uint32_t readBitTime(); //returns the next bit for processing if one is available. Returns 0 when the queue was empty

private:
    // This is a singleton class, make sure this object cannot be created except for getInstance
    DccInterface();
    ~DccInterface();

    bool isInitialized_ = false;

    //DCC helper funcs
    bool is1HalfBit(uint32_t t);
    bool is0HalfBit(uint32_t t);
    bool valid1BitDelta(uint32_t t11, uint32_t t12);
    bool valid0BitTotal(uint32_t t01, uint32_t t02);

    //Bit-time Queue
    std::array<uint16_t, DCC_BITTIME_QUEUE_SIZE> dccBitTimeQueue_; // Queue to store incoming DCC bits from ISR for processing in main loop
    uint16_t qReadIdx_ = 0;     //read location to extract bit-times
    volatile uint16_t qWriteIdx_ = 0;    //write location to push bit-time. Volatile tells the compiler the value can be changed at any moment, and some code optimization should be skipped
    bool qIsFull_ = false;       //Indicates there are no more spaces in the array to write bit-times to
    bool qErrorFlag_ = false;    //Indicates a write has been attempted while there was no more space in the bit-time array
    bool qIsEmpty_ = true;       //Indicates whether or not new bit-time data is available
};

#endif // DCC_H