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
// Testing showed that the number of elements in the queue is 0..1 during normal operation.
// If the main loop is executed with with a 1ms delay, the number of elements is up to 28
constexpr uint32_t DCC_BITTIME_QUEUE_SIZE = 48; 

constexpr bool DCC_PRINT_DEBUG_INFO = true; // Set to true to enable periodic printing of DCC reader debug info through platform debug channel
constexpr uint32_t DCC_DEBUG_PERIOD_MS = 500; //Period for printing debug info about DCC reader state
constexpr bool DCC_DEBUG_HALFBITS = false;  // Set to true to enable printing of debug info about halfbit processing
constexpr bool DCC_DEBUG_FRAMES = false;    // Set to true to enable printing of debug info about byte processing of DCC frames
constexpr bool DCC_DEBUG_MESSAGES = false;  // Set to true to enable printing of debug info about message processing
constexpr bool DCC_DEBUG_STATE = true;      // Set to true to enable printing of debug info about the current DCC state (speed, functions, etc.) after processing each message  

// Run-time config
typedef enum DccControlMode_e : uint8_t{
    DCC_CONTROL_MODE_ANALOG = 0,
    DCC_CONTROL_MODE_DCC_14SS = 1,  //In this mode, speed steps are 1-14, and bit4 of speed data is used for F0 (light) function
    DCC_CONTROL_MODE_DCC_128SS = 2  //Also for 28 speed steps. F0 is bit4 in function group 1 instruction message
} DccControlMode_t;

constexpr uint16_t DCC_DEFAULT_ADDR = 3; //Default DCC address if none is set on init

typedef enum DccDirection_e : bool {
    DCC_DIRECTION_REVERSE = 0,
    DCC_DIRECTION_FORWARD = 1
} DccDirection_t;

typedef struct DccConfig_s {
    DccControlMode_t  controlMode;  // Default control mode (DCC/Analog)
    DccDirection_t direction;       // Direction reversal (true is normal, false is reverse)
    uint16_t addr;                  // Configured DCC address
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
    uint8_t  speed;             // Current speed step (-127 .. +127, -28 .. +28 or -14 .. +14 depending on speed step mode)
    DccDirection_t direction;   // Current direction (true is forward, false is reverse)
    uint16_t funcEnanbled;      // Current function bits (F0F, F0R, F1 ..F12). These are masked bits, so bit 0 is F0F, bit 1 is F0R, bit 2 is F1, etc.
} DccVarState_t;

//Message processing
// Expected ticks for DCC bit transitions
// Refer to https://www.nmra.org/sites/default/files/standards/sandrp/DCC/S/s-9.1_electrical_standards_for_digital_command_control.pdf
// We use timer frequency and include the tolerance to calculate the min and max ticks for each bit type
// Note that we assume the timer type can be uint16_t, even if we use a uint32_t here
constexpr uint32_t DCC_BITTIME_T1_MIN = (uint32_t)((DCC_TIMER_FREQ_MIN*52ull)/1000000ull);            // 52us for halfbit
constexpr uint32_t DCC_BITTIME_T1_MAX = (uint32_t)((DCC_TIMER_FREQ_MAX*64ull)/1000000ull);            // 64us for halfbit
constexpr uint32_t DCC_BITTIME_T1_MAX_DELTA = (uint32_t)((DCC_TIMER_FREQ_MAX*6ull)/1000000ull);       // 6us in dcc spec for max time difference between two "1"-half-bits
constexpr uint32_t DCC_BITTIME_T0_MIN = (uint32_t)((DCC_TIMER_FREQ_MIN*90ull)/1000000ull);            // 90us for halfbit
constexpr uint32_t DCC_BITTIME_T0_MAX = (uint32_t)((DCC_TIMER_FREQ_MAX*10000ull)/1000000ull);         // 10.000us for halfbit
constexpr uint32_t DCC_BITTIME_T0_MAX_TOTAL = (uint32_t)((DCC_TIMER_FREQ_MAX*12000ull)/1000000ull);   // 12.000us For total bit (two "0"-half bits with 0 stretching)

// state machine enumeration for DCC definitions
constexpr uint8_t NUM_ONES_VALID_PREAMBLE = 10; // At receiver side. There should be 14 or more preamble bits by the controller
constexpr uint8_t MAX_BYTESIZE_ADDR = 2;
constexpr uint8_t MAX_BYTESIZE_CMD_ARG = 3;
constexpr uint8_t MAX_BYTESIZE_DATA = MAX_BYTESIZE_ADDR + MAX_BYTESIZE_CMD_ARG + 1; //2 address bytes, 3 data bytes
constexpr uint8_t MIN_BYTESIZE_DATA = 3; //At least the address, one byte of data, and one byte of CRC


typedef enum DccReaderState_e : uint8_t {
    dcc_reader_reset = 0,
    dcc_read_preamble = 1,
    dcc_read_start = 2,
    dcc_read_byte = 3,
    dcc_read_sync = 4,
    dcc_check_crc = 5,
    dcc_reader_new_msg = 6
} DccReaderState_t;

typedef enum DccHalfbitState_e : uint8_t {
    dcc_halfbit_uninitialized = 0,
    dcc_half1_bit = 1,
    dcc_half0_bit = 2,
    dcc_valid_1 = 3,
    dcc_valid_0 = 4,
    dcc_invalid_bit = 5
} DccHalfbit_t;

typedef struct DccMsg_s {
    uint16_t addr;
    bool longAddr = false;
    DccMsgType_t msg_type;
    uint8_t cmd_arg[MAX_BYTESIZE_CMD_ARG];
    uint8_t speed;
    DccDirection_t direction;
    uint8_t af_group1;
    uint8_t af_group2;
    uint8_t validMsg;
} DccMsg_t;

typedef enum DccReinterpretBaseline_e : uint8_t {
    dcc_reinterpret_baseline_none = 0,
    dcc_reinterpret_baseline_14ss = 1, // Reinterpret speed in baseline messages as 14 speed steps, where bit4 is F0
    dcc_reinterpret_baseline_28ss = 2  // Reinterpret speed in baseline messages as 28 speed steps, where bit4 is an additional LSB for speed
} DccReinterpretBaseline_t;

// Debugging info
typedef struct DccDebugInfo_s {
    uint32_t RxHbTotHalfBits;   // Total number of half-bits received, including invalid ones
    uint32_t RxHbValid1Bits;    // Total number of valid full "1"-bits received
    uint32_t RxHbValid0Bits;    // Total number of valid full "0" bits received
    uint32_t EHbBitTimeViolations;      // Total number of half-bits received with an invalid time (not within the defined intervals for "0" or "1" half-bits)
    uint32_t EHbDeltaViolations;        // Total number of times a "1" bit was received where the time difference between the two half-bits was larger than the allowed delta
    uint32_t EHbTotTimeViolations;      // Total number of times a "0" bit was received where the total time of the two half-bits was larger than the allowed total
    uint32_t EHbBadSyncHalfBits;        // Total number of times a "0" or "1" bit was received where the second half-bit was not valid for that bit type (e.g. receiving a "1" bit where the second half-bit was not a valid "1" half-bit)
    uint32_t RxFrTotBytes;             // Total number of full bytes received (for valid and invalid messages)
    uint32_t RxFrValidFrames;          // Total number of valid frames received (frame is valid when it has a valid preamble, valid bytes and a correct CRC)
    uint32_t RxMsgValidMsgs;            // Total number of valid messages received (valid msg type, amount of bytes, etc.)
    uint32_t RxMsgIdle;                 // Total number of idle messages received (address byte is 255)
    uint32_t RxMsgBroadcast;            // Total number of broadcast messages received 
    uint32_t RxMsgForMpDecoder;         // Total number of valid messages for a multipurpose decoder
    uint32_t RxMsgTotMsgsForThisUnit;   // Total number of valid messages received that are relevant for this unit (e.g. correct address, or broadcast message)
    uint32_t EFrReaderResets;          // Total number of times the DCC reader was reset for whatever reason
    uint32_t EFrInvalidPreambles;      // Amount of times an invalid preamble was received (e.g. not enough "1" bits). We can expect this to happen for any of the DCC reader resets
    uint32_t EFrInvalidFrames;         // Amount of times a faulty frame was received (for ex too long)
    uint32_t EFrInvalidCRC;            // Amount of times a faulty byte was received (e.g. more than 8 bits, invalid start bit, etc.)
    uint32_t EMsgInvalidMsgType;        // Amount of times a faulty message was received
    uint32_t EMsUnsupportedMsgType;     // Amount of times an unsupported message type was received
} DccDebugInfo_t;

class DccBitTimeQueue
{
public:
    DccBitTimeQueue(){;}
    ~DccBitTimeQueue(){;}

    // Bit-time queue funcs
    bool queueIsFull(){ return qIsFull_; }
    bool queueIsEmpty(){ return qIsEmpty_; }
    bool queueHasError(){ return qErrorFlag_; }
    void resetQueue();

    //If the queue was full when calling this function, an internal error flag is set
    bool addBitTime(uint32_t bit_time); //returns true when bit-time was succesfully added, false when full or error was set
    uint32_t elementsInQueue();     // Returns the number of elements currently in the queue
    uint32_t readBitTime();         //returns the next bit for processing if one is available. Returns 0 when the queue was empty
private:
    //Bit-time Queue
    std::array<uint16_t, DCC_BITTIME_QUEUE_SIZE> dccBitTimeQueue_{}; // Queue to store incoming DCC bits from ISR for processing in main loop
    uint16_t qReadIdx_ = 0;     //read location to extract bit-times
    volatile uint16_t qWriteIdx_ = 0;    //write location to push bit-time. Volatile tells the compiler the value can be changed at any moment, and some code optimization should be skipped
    bool qIsFull_ = false;       //Indicates there are no more spaces in the array to write bit-times to
    bool qErrorFlag_ = false;    //Indicates a write has been attempted while there was no more space in the bit-time array
    bool qIsEmpty_ = true;       //Indicates whether or not new bit-time data is available
};


class DccInterface
{
public:
    // This is a singleton class
    static DccInterface& getInstance(){ 
        static DccInterface instance; // created once
        return instance;
    }

    // Prevent copying and moving. 
    // This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    DccInterface(const DccInterface&) = delete;
    DccInterface(DccInterface&&) = delete;
    DccInterface& operator=(const DccInterface&) = delete;
    DccInterface& operator=(DccInterface&&) = delete;
    
    bool init(DccHwInitCfg_t* initHwCfg = nullptr, DccConfig_t* initCfg = nullptr);
    bool step();
    const DccMsg_t& getLastMsg() const {return lastDccMsg_;}
    const DccReaderState_t& getReaderState() const {return dccReaderState_;}
    const DccControlMode_t& getControlMode() const {return dccConfig_.controlMode;}
    const uint16_t getActiveFuncs() const {return dccVarState_.funcEnanbled;}

    // This function must be exposed to the wrapper, as it is called from the DCC bit-time ISR to add new bit-times to the queue
    bool addBitTime(uint32_t t);
    
private:
    // This is a singleton class, make sure this object cannot be created except for getInstance
    DccInterface();
    ~DccInterface();

    bool isInitialized_ = false;
    //DCC processing funcs
    void resetDccReader(bool resetLastMsg = false);
    DccHalfbit_t feedHalfbit(uint32_t t);
    DccReaderState_t feedBit(DccHalfbit_t bit);

    //DCC helper funcs
    bool is1HalfBit(uint32_t t);
    bool is0HalfBit(uint32_t t);
    bool valid1BitDelta(uint32_t t11, uint32_t t12);
    bool valid0BitTotal(uint32_t t01, uint32_t t02);

    //Message processing funcs
    bool isMsgForThisUnit();
    bool processDccMsg();
    bool processAddress();
    bool processCmdType();
    bool processBaselineMsg(DccReinterpretBaseline_t speedSetting = dcc_reinterpret_baseline_none);
    bool processAdvancedMsg();
    bool processFuncGroupMsg();
    bool processCvWriteMsg();
    bool applyMsgToState();
    bool applyBaselineMsgToState();
    bool applyAdvancedMsgToState();
    bool applyFuncGroupMsgToState();
    void updateF0();

    // Debug functions
    void printDccDebugInfo();
    const uint32_t dccDebugPrintPeriod_ = DCC_DEBUG_PERIOD_MS;   // period in ms
    DccDebugInfo_t dccDebugInfo_ = {};

    //Config and run-time state
    DccConfig_t dccConfig_ = {DCC_CONTROL_MODE_DCC_128SS, DCC_DIRECTION_FORWARD, DCC_DEFAULT_ADDR};
    DccVarState_t dccVarState_ = {0, DCC_DIRECTION_FORWARD, 0};  //Set speed to 0, all functions off
    DccHalfbit_t halfbitState_ = dcc_halfbit_uninitialized;
    DccReaderState_t dccReaderState_ = dcc_reader_reset;
    uint8_t dccMsgBuf_[MAX_BYTESIZE_DATA] = {0}; //Buffer to store incoming bytes while processing a message. Size is max addr bytes + max data bytes
    DccMsg_t lastDccMsg_ = {0, false, no_new_dcc_msg, {0}, 0, DCC_DIRECTION_FORWARD, 0, 0, 0}; //Last valid message received
    DccBitTimeQueue bitTimeQueue_;
    bool cvWriteInProgress_ = false; //Indicates a CV write operation is ongoing. Flag is set after reception of the first messsage, and cleared after the second required cmd message was received OR when the write is invalidated.

};

#endif // DCC_H