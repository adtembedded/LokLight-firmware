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
#include "dcc_defs.h"           // Common DCC definitions
#include <array>                // As memory-efficient queue between ISR and dcc processing w.r.t. bit-timing on tracks

// Size of the DCC bit time queue. Assume processing happens at least once per this number of polarity transitions
// There can be up to 20 transitions per ms, so make this buffer large enough
constexpr uint32_t DCC_BITTIME_QUEUE_SIZE = 128; 

// Run-time config
typedef enum DccControlMode_e : uint8_t{
    DCC_CONTROL_MODE_ANALOG = 0,
    DCC_CONTROL_MODE_DCC_14SS = 1,  //In this mode, speed steps are 1-14, and bit4 of speed data is used for F0 (light) function
    DCC_CONTROL_MODE_DCC_128SS = 2  //Also for 28 speed steps. F0 is bit4 in function group 1 instruction message
} DccControlMode_t;

constexpr uint16_t DCC_DEFAULT_ADDR = 3; //Default DCC address if none is set on init

typedef enum DccDirection_e : bool {
    DCC_DIRECTION_FORWARD = 0,
    DCC_DIRECTION_REVERSE = 1
} DccDirection_t;

typedef struct DccConfig_s {
    uint8_t  controlMode;       // Current control mode (DCC/Analog)
    bool     direction;         // Current direction (false is normal, true is reverse)
    uint16_t addr;              // Current DCC address
} DccConfig_t;

// Run-time variables
typedef enum DccFuncMask_e : uint16_t {
    DCC_FUNC_F0F = (1ul << 0),
    DCC_FUNC_F0R = (1ul << 1),
    DCC_FUNC_F1  = (1ul << 2),
    DCC_FUNC_F2  = (1ul << 3),
    DCC_FUNC_F3  = (1ul << 4),
    DCC_FUNC_F4  = (1ul << 5),
    DCC_FUNC_F5  = (1ul << 6),
    DCC_FUNC_F6  = (1ul << 7),
    DCC_FUNC_F7  = (1ul << 8),
    DCC_FUNC_F8  = (1ul << 9),
    DCC_FUNC_F9  = (1ul << 10),
    DCC_FUNC_F10 = (1ul << 11),
    DCC_FUNC_F11 = (1ul << 12),
    DCC_FUNC_F12 = (1ul << 13)
} DccFuncMask_t;

typedef struct DccState_s {
    int8_t   speed;             // Current speed step (-127 .. +127, -28 .. +28 or -14 .. +14 depending on speed step mode)
    uint16_t funcEnanbled;      // Current function bits (F0F, F0R, F1 ..F12). These are masked bits, so bit 0 is F0F, bit 1 is F0R, bit 2 is F1, etc.
} DccVarState_t;

//Message processing
// Expected ticks for DCC bit transitions
// Refer to https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.1_electrical_standards_for_digital_command_control.pdf
// We use timer frequency and include the tolerance to calculate the min and max ticks for each bit type
// Note that we assume the timer type can be uint16_t, even if we use a uint32_t here
constexpr uint32_t DCC_BITTIME_T1_MIN = (uint32_t)((DCC_TIMER_FREQ_MIN*52ull)/1000000ull);            // 52us for halfbit
constexpr uint32_t DCC_BITTIME_T1_MAX = (uint32_t)((DCC_TIMER_FREQ_MAX*64ull)/1000000ull);            // 64us for halfbit
constexpr uint32_t DCC_BITTIME_T1_MAX_DELTA = (uint32_t)((DCC_TIMER_FREQ_MAX*6ull)/1000000ull);       // 6us for max time difference between two "1"-half-bits
constexpr uint32_t DCC_BITTIME_T0_MIN = (uint32_t)((DCC_TIMER_FREQ_MIN*90ull)/1000000ull);            // 90us for halfbit
constexpr uint32_t DCC_BITTIME_T0_MAX = (uint32_t)((DCC_TIMER_FREQ_MAX*10000ull)/1000000ull);         // 10.000us for halfbit
constexpr uint32_t DCC_BITTIME_T0_MAX_TOTAL = (uint32_t)((DCC_TIMER_FREQ_MAX*12000ull)/1000000ull);   // 12.000us For total bit (two "0"-half bits with 0 stretching)

// state machine enumeration for DCC definitions
constexpr uint8_t NUM_ONES_VALID_PREAMBLE = 10; // At receiver side. There should be 12 or more preamble bits by the controller
constexpr uint8_t MAX_BYTESIZE_ADDR = 2;
constexpr uint8_t MAX_BYTESIZE_DATA = 7;
constexpr uint8_t MAX_BYTESIZE_CMD_ARG = 4;


typedef enum DccReaderState_e : uint8_t {
    reader_reset = 0,
    read_preamble = 1,
    read_start = 2,
    read_byte = 3,
    read_sync = 4,
    check_crc = 5
} DccReaderState_t;

typedef enum DccHalfbitState_e : uint8_t {
    halfbit_uninitialized = 0,
    half_bit = 1,
    valid_1 = 2,
    valid_0 = 3,
    invalid_bit = 4
} DccHalfbit_t;

typedef struct DccMsg_s {
    uint16_t addr;
    uint8_t cmd;
    DccMsgType_t msg_type;
    uint8_t cmd_arg[MAX_BYTESIZE_CMD_ARG];
    int8_t speed;
    uint8_t af_group1;
    uint8_t af_group2;
    uint8_t validMsg;
} DccMsg_t;


class DccInterface
{
public:
    // This is a singleton class
    static DccInterface& getInstance(){ 
        static DccInterface instance; // created once
        return instance;
    }
    
    bool init(DccHwInitCfg_t* initHwCfg = nullptr, DccConfig_t* initCfg = nullptr);
    bool step();
    const DccMsg_t& getLastMsg() const {return lastDccMsg_;}
    const DccReaderState_t& getReaderState() const {return dccReaderState_;}
    
    //Adds a bit-time to the queue. This func must be exposed to the hardware wrapper
    //If the queue was full when calling this function, an internal error flag is set and the dcc reader 
    //will re-initialize to avoid processing inconsistent data
    bool addBitTime(uint32_t t);

    
    //TODO move to private
    //Check if there is something to read
    uint32_t elementsInQueue();
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

    //DCC processing funcs
    DccHalfbit_t feedHalfbit(uint32_t t);
    DccMsg_t feedBit(DccHalfbit_t bit);
    void resetQueue();

    //Config and run-time state
    DccConfig_t dccConfig_ = {DCC_CONTROL_MODE_DCC_128SS, DCC_DIRECTION_FORWARD, DCC_DEFAULT_ADDR};
    DccVarState_t dccVarState_ = {0, 0};  //Set speed to 0, all functions off
    DccHalfbit_t lastHalfbitState_ = halfbit_uninitialized;
    DccReaderState_t dccReaderState_ = reader_reset;
    DccMsg_t dccMsgBuf_ = {0, 0, no_new_dcc_msg, {0}, 0, 0, 0, 0};  //Buffer for processing incoming messages
    DccMsg_t lastDccMsg_ = {0, 0, no_new_dcc_msg, {0}, 0, 0, 0, 0}; //Last valid message received
    bool cvWriteInProgress_ = false; //Indicates a CV write operation is ongoing. Flag is set after reception of the first messsage, and cleared after the second required cmd message was received OR when the write is invalidated.

    //Bit-time Queue
    std::array<uint16_t, DCC_BITTIME_QUEUE_SIZE> dccBitTimeQueue_; // Queue to store incoming DCC bits from ISR for processing in main loop
    uint16_t qReadIdx_ = 0;     //read location to extract bit-times
    volatile uint16_t qWriteIdx_ = 0;    //write location to push bit-time. Volatile tells the compiler the value can be changed at any moment, and some code optimization should be skipped
    bool qIsFull_ = false;       //Indicates there are no more spaces in the array to write bit-times to
    bool qErrorFlag_ = false;    //Indicates a write has been attempted while there was no more space in the bit-time array
    bool qIsEmpty_ = true;       //Indicates whether or not new bit-time data is available
};

#endif // DCC_H