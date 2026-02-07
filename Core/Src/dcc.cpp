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

#include "dcc.h"
#include <cstring>  // For memset

DccInterface::DccInterface()
{
}

DccInterface::~DccInterface()
{
}

bool DccInterface::init(DccHwInitCfg_t* initHwCfg /*= nullptr*/, DccConfig_t* initCfg /*= nullptr*/)
{
    isInitialized_ = false; //Setting this flag will also prevent writes to the bit-time queue

    //Set software config. This is optional
    if(initCfg)
    {
        dccConfig_ = *initCfg;  //Copy the data
    }

    //Reset runtime state of reader
    bitTimeQueue_.resetQueue();
    resetDccReader(true);
    dccVarState_ = {0, 0};  //Set speed to 0, all functions off
    //Do not reset the config. It has default values and the caller can overwrite them if needed. The settings are preserved across inits.
    
    // Initialize PWM timer for DCC reading, this is mandatory
    if(initHwCfg)
    {
        // Make sure the timer settings make sense.
        // We want at least 50 ticks per halfbit, which is ~1MHz timer frequency minimum
        // Then we want the max bit-stretched "0" half-bit (10ms) to still fit in a uint16_t, which is ~6.5MHz maximum
        static_assert(DCC_TIMER_FREQ_MIN >= 1000000ul, "DCC timer frequency too low for reliable DCC decoding");
        static_assert(DCC_TIMER_FREQ_MAX <= 6500000ul, "DCC timer frequency too high for reliable DCC decoding");

        // Call the hardware-specific initialization function
        if(dcc_hw_init(initHwCfg))
        {
            // Nothing to do for now
            // We could check if we can receive DCC bits, but on an analog track that would not work so skip that step.
            isInitialized_ = true;
        }
    }

    return isInitialized_;
}

bool DccInterface::step()
{
    if(!isInitialized_)
    {
        return false;
    }

    // Process incoming DCC bits from the queue
    if(bitTimeQueue_.queueHasError())
    {
        // An error occured in the queue, reset the DCC reader to avoid processing inconsistent data
        resetDccReader(false);
        bitTimeQueue_.resetQueue();
        // Stop processing here
        return false;
    }

    //Print debug info here, as we want to see the number of elements in the queue
    printDccDebugInfo();
    // Update halfbit processor
    while(bitTimeQueue_.elementsInQueue() > 0)
    {
        uint32_t bitTime = bitTimeQueue_.readBitTime();
        DccHalfbit_t bitStatus = feedHalfbit(bitTime);
        if(bitStatus == dcc_valid_0 || bitStatus == dcc_valid_1)
        {
            // Process msg if valid
            if(feedBit(bitStatus) == dcc_reader_new_msg)
            {
                processDccMsg();
            }
        }
    }
    
    // Update bit processor

    // Check for activity and revert to analog mode if none is detected.
    // Note, analog polarity could change without the dcc reader loosing power. Therefore we check for 
    // Activity after filtering for valid DCC bits, not on polarity changes directly.

    // Update message processing

    return true;
}

bool DccInterface::addBitTime(uint32_t t)
{
    if(!isInitialized_)
    {
        return false;
    }
    return bitTimeQueue_.addBitTime(t);
}

void DccInterface::resetDccReader(bool resetLastMsg)
{
    halfbitState_ = dcc_halfbit_uninitialized;
    dccReaderState_ = dcc_reader_reset;
    memset(dccMsgBuf_, 0, sizeof(dccMsgBuf_)); // Clear the message buffer
    if(resetLastMsg) {
        lastDccMsg_ = {0, false, no_new_dcc_msg, {0}, 0, 0, 0, 0};
    }
    cvWriteInProgress_ = false;
    dccDebugInfo_.EFrReaderResets++;
}

DccHalfbit_t DccInterface::feedHalfbit(uint32_t t)
{
    static uint32_t lastHalfbitTime = 0;    // 0 will denote invalid/uninitialized
    dccDebugInfo_.RxHbTotHalfBits++;
    
    // First check if we are processing a valid bit-time
    bool validHalfbit = is1HalfBit(t) || is0HalfBit(t);
    if(!validHalfbit)
    {
        // Stop processing here, invalid half-bit time
        halfbitState_ = dcc_invalid_bit;
        dccDebugInfo_.EHbBitTimeViolations++;
        // loklight_debug_print("Invalid half-bit time: %lu\r\n", t);
        // Resetting the state will happen below
    }
    else
    {
        // We are processing a valid bit
        switch(halfbitState_)
        {   
            // Upon init, after processing valid bits and after an invalid bit, we need to start over detection.
            case dcc_halfbit_uninitialized:
            case dcc_valid_0:
            case dcc_valid_1:
            case dcc_invalid_bit:
            {
                // Note the following subtlety:
                // DCC 0s and 1s consist of two half-bits. If we start here, we may be out of sync, starting to read the second half-bit of a DCC bit.
                // If we are to set half0 or half1, we may detect an error upon the next halfbit, if it is a different one. Therefore, in case of an
                // invalid bit we must immediately be ready to accept both half-bits again on the next call to get in sync.
                
                // First half-bit received
                if(is1HalfBit(t))
                {
                    halfbitState_ = dcc_half1_bit;
                }
                else if(is0HalfBit(t))
                {
                    halfbitState_ = dcc_half0_bit;
                }
                else
                {
                    // Happens when we found a time that is neither a 0 nor 1 half-bit
                    // Actually cannot not end up here because of the validHalfbit check above
                    halfbitState_ = dcc_invalid_bit;
                    dccDebugInfo_.EHbBitTimeViolations++;
                    // loklight_debug_print("Invalid half-bit time: %lu\r\n", t);
                }
                break;
            }
            case dcc_half1_bit:
            {
                // Second half-bit received for a "1" bit
                bool valid1Halfbit = is1HalfBit(t);
                bool valid0Halfbit = is0HalfBit(t);
                bool validDelta = valid1BitDelta(t, lastHalfbitTime);
                if(valid1Halfbit && validDelta)
                {
                    halfbitState_ = dcc_valid_1;
                    dccDebugInfo_.RxHbValid1Bits++;
                }
                else if(valid0Halfbit)
                {
                    // This can happen in a special case where we are receiving the pre-amble and the dcc control station is sending
                    // an uneven number of 1 halfbits in the idle time between subsequent frames.
                    if(dccReaderState_ == dcc_read_start)
                    {
                        // Accept the previous half-bit as a valid 1 half-bit, and this one as a valid 0 half-bit, to stay in sync with the controller.
                        halfbitState_ = dcc_half0_bit;
                        // loklight_debug_print("Receiving inverse polarity preamble!\r\n");
                    }
                    else
                    {
                        halfbitState_ = dcc_invalid_bit;
                        dccDebugInfo_.EHbBadSyncHalfBits++;
                        // loklight_debug_print("Received 1-halfbit and 0-halfbit but not while reading preamble: %lu %lu %u\r\n", lastHalfbitTime, t, dccReaderState_);
                    }
                }
                else
                {
                    halfbitState_ = dcc_invalid_bit;
                    if(!valid1Halfbit)
                    {
                        dccDebugInfo_.EHbBadSyncHalfBits++;
                        // loklight_debug_print("Receiving 1bit, second half not 1bit nor 0bit: %lu %lu\r\n", lastHalfbitTime, t);
                    }
                    else
                    {   //This can only happen if the delta was invalid
                        dccDebugInfo_.EHbDeltaViolations++;
                        // uint32_t delta = (lastHalfbitTime > t) ? (lastHalfbitTime - t) : (t - lastHalfbitTime);
                        // loklight_debug_print("Receiving 1bit, delta invalid: %lu %lu %lu\r\n", lastHalfbitTime, t, delta);
                    }
                }
                break;
            }
            case dcc_half0_bit:
            {
                // Second half-bit received for a "0" bit
                bool valid0Halfbit = is0HalfBit(t);
                bool validTotal = valid0BitTotal(t, lastHalfbitTime);
                if(valid0Halfbit && validTotal)
                {
                    halfbitState_ = dcc_valid_0;
                    dccDebugInfo_.RxHbValid0Bits++;
                }
                else
                {
                    halfbitState_ = dcc_invalid_bit;
                    if(!valid0Halfbit)
                    {
                        dccDebugInfo_.EHbBadSyncHalfBits++;
                        // loklight_debug_print("Receiving 0bit, second half not 0bit: %lu %lu\r\n", lastHalfbitTime, t);
                    }
                    else
                    {
                        dccDebugInfo_.EHbTotTimeViolations++;
                        uint32_t total = t + lastHalfbitTime;
                        loklight_debug_print("Receiving 0bit, total invalid: %lu %lu %lu\r\n", lastHalfbitTime, t, total);
                    }
                }
                break;
            }
            default:
            {
                // We cannot end up here, but just in case reset to uninitialized
                halfbitState_ = dcc_halfbit_uninitialized;
                break;
            }
        }        
    }   // If validHalfbit

    // Check if we are in a valid state after processing the half bit. If not, we must also reset the reader
    bool validState =   (halfbitState_ == dcc_valid_0)      || (halfbitState_ == dcc_valid_1) ||
                        (halfbitState_ == dcc_half0_bit)    || (halfbitState_ == dcc_half1_bit);
    if(!validState)
    {
        // Reset the DCC bit reader to avoid processing inconsistent data
        // Note this also resets the halfbit state to uninitialized
        resetDccReader(false);
    }

    // Store the last half-bit time for delta calculations
    lastHalfbitTime = t;

    return halfbitState_;
}

DccReaderState_t DccInterface::feedBit(DccHalfbit_t bit)
{
    // Process full bits only
    bool validBit = (bit == dcc_valid_0) || (bit == dcc_valid_1);
    if(!validBit)
    {
        return dccReaderState_;
    }

    static uint8_t data[MAX_BYTESIZE_DATA];
    static uint8_t rxByteCnt = 0;
    static uint8_t preambleCount = 0;
    static uint8_t bitPos = 0;
    static uint8_t crc;

    // Start with resetting to a valid state, also do this after a message has been received
    if(dccReaderState_ == dcc_reader_reset || dccReaderState_ == dcc_reader_new_msg)
    {
        memset(data, 0, sizeof(data));
        rxByteCnt = 0;
        preambleCount = 0;
        bitPos = 0;
        crc = 0;
        dccReaderState_ = dcc_read_preamble;

        // The bit was not consumed, move on to reading the preamble
    }

    // We must always receive at least a preamble of 10 "1" bits before any valid data
    if(dccReaderState_ == dcc_read_preamble)
    {
        if(bit == dcc_valid_1)
        {
            preambleCount++;
            if(preambleCount >= NUM_ONES_VALID_PREAMBLE)
            {
                dccReaderState_ = dcc_read_start;
                preambleCount = 0; //reset for next time
            }
        }
        else
        {
            // Invalid bit during preamble, reset count
            dccDebugInfo_.EFrInvalidPreambles++;
            preambleCount = 0;
        }

        // The bit is consumed, exit here
        return dccReaderState_;
    }

    // After reading enough preamble bits, we simply wait until we receive a start bit (0)
    if(dccReaderState_ == dcc_read_start)
    {
        if(bit == dcc_valid_0)
        {
            // Start bit detected, move to reading data bytes
            dccReaderState_ = dcc_read_byte;
            bitPos = 0;
        }

        // The bit is consumed, exit here
        return dccReaderState_;
    }

    // After a start bit or a sync bit, we read a full byte
    if(dccReaderState_ == dcc_read_byte)
    {
        // If we received a 1, set the bit. By default bits are 0 so no action needed there.
        if(bit == dcc_valid_1)
        {
            data[rxByteCnt] |= (1 << (7 - bitPos));
        }

        bitPos++;
        if(bitPos >= 8)
        {
            // Byte complete, check if we are within limits
            bitPos = 0;
            rxByteCnt++;
            dccDebugInfo_.RxFrTotBytes++;

            if(rxByteCnt >= MAX_BYTESIZE_DATA)
            {
                // Too many bytes received, reset reader
                dccDebugInfo_.EFrInvalidFrames++;
                resetDccReader(false);
            }
            
            // We now wait for a divider
            dccReaderState_ = dcc_read_sync;
        }

        // The bit is consumed, exit here
        return dccReaderState_;
    }

    // Between bytes, we expect a sync bit (0) or the end of a frame (1)
    if(dccReaderState_ == dcc_read_sync)
    {
        if(bit == dcc_valid_0)
        {
            // Sync bit detected, move to reading next byte
            dccReaderState_ = dcc_read_byte;

            // Bit is consumed, exit here
            return dccReaderState_;
        }
        else if(bit == dcc_valid_1) // End of frame indicated by a "1" bit after reading at least one byte
        {
            // Check if we have received a valid frame, i.e. at least the minimum number of bytes
            if(rxByteCnt >= MIN_BYTESIZE_DATA)
            {
                // We have received a valid frame, move to CRC check
                dccReaderState_ = dcc_check_crc;

                // Bit is NOT consumed, process CRC next
            }
            else
            {
                // Not enough bytes received, reset reader
                dccDebugInfo_.EFrInvalidFrames++;
                resetDccReader(false);

                // Bit is consumed, exit here
                return dccReaderState_;
            }
        }
        else
        {
            // This cannot happen, reset reader
            dccReaderState_ = dcc_reader_reset;
            
            // Bit is consumed, exit here
            return dccReaderState_;
        }
    }
    
    // Finally, check the CRC byte
    if(dccReaderState_ == dcc_check_crc)
    {
        // Calculate CRC over the received bytes, except the last one which is the CRC byte itself
        crc = 0;
        for(uint8_t i = 0; i < rxByteCnt - 1; i++)
        {
            crc ^= data[i];
        }

        // Compare calculated CRC with received CRC (last byte)
        if(crc == data[rxByteCnt - 1])
        {
            // CRC has expecte value. This is a valid DCC frame
            dccReaderState_ = dcc_reader_new_msg;
            dccDebugInfo_.RxFrValidFrames++;
        }
        else
        {
            // CRC does not match, invalid frame
            dccDebugInfo_.EFrInvalidCRC++;
            resetDccReader(false);
        }
    }

    // If there is a new message, copy the data to the internal message buffer
    memset(dccMsgBuf_, 0, sizeof(dccMsgBuf_)); // Clear the message buffer
    for(auto i = 0; i < rxByteCnt; i++)
    {
        dccMsgBuf_[i] = data[i];
    }

    return dccReaderState_;
}

bool DccInterface::is1HalfBit(uint32_t t)
{
    // Check if bit time is within valid interval for a "1" half-bit (52us - 64us)
    return (t >= DCC_BITTIME_T1_MIN) && (t <= DCC_BITTIME_T1_MAX);
}

bool DccInterface::is0HalfBit(uint32_t t)
{
    // Check if bit time is within valid interval for a "0" half-bit (90us - 10.000us)
    return (t >= DCC_BITTIME_T0_MIN) && (t <= DCC_BITTIME_T0_MAX);
}

bool DccInterface::valid1BitDelta(uint32_t t11, uint32_t t12)
{
    // Check if the difference between two "1" half-bits is within the allowed delta (6us)
    uint32_t delta = (t11 > t12) ? (t11 - t12) : (t12 - t11);
    return (delta <= DCC_BITTIME_T1_MAX_DELTA);
}

bool DccInterface::valid0BitTotal(uint32_t t01, uint32_t t02)
{
    // Check if the total time of two "0" half-bits is within the allowed total (12.000us)
    uint32_t total = t01 + t02;
    return (total <= DCC_BITTIME_T0_MAX_TOTAL);
}

bool DccInterface::isMsgForThisUnit()
{
    return false; //TODO
}

bool DccInterface::processDccMsg()
{
    // Step 0: reset the buffer
    lastDccMsg_ = {0, false, no_new_dcc_msg, {0}, 0, 0, 0, 0};
    
    // Step 1: determine the address
    // After this step, the address can be found in lastDccMsg_.addr
    if(!processAddress())
    {
        return false;
    }

    // Step 2: determine the command type
    // After this step, the command type can be found in lastDccMsg_.msg_type
    if(!processCmdType())
    {
        return false;
    }

    lastDccMsg_.validMsg = true;
    dccDebugInfo_.RxMsgValidMsgs++;

    return true;
}

bool DccInterface::processAddress()
{
    // Unpack the address
    uint8_t addrByte1 = dccMsgBuf_[0];

    if(addrByte1 == dcc_short_addr_all)
    {
        // Broadcast message, address is 0
        lastDccMsg_.addr = dcc_short_addr_all;
        lastDccMsg_.longAddr = false;
        dccDebugInfo_.RxMsgBroadcast++;
    }
    else if(addrByte1 >= dcc_short_addr_multipurp_start && addrByte1 <= dcc_short_addr_multipurp_end)
    {
        // In range of multipurpose decoder with short addresses.
        // The address is encoded in the first byte
        lastDccMsg_.addr = addrByte1;
        lastDccMsg_.longAddr = false;
        dccDebugInfo_.RxMsgForMpDecoder++;
    }
    else if(addrByte1 >= dcc_short_addr_accessory_start && addrByte1 <= dcc_short_addr_accessory_end)
    {
        // In range of accessory decoder addresses. The address is encoded in the first two bytes as follows
        // byte1 [1, 0, A7, A6, A5, A4, A3, A2] 
        // byte 2 [1, A10, A9, A8, D A1, A0, R]     We ignore D and R, not relevant for Loklight
        uint8_t addrByte2 = dccMsgBuf_[1];
        uint16_t addr = 0;

        addr = (addrByte1 & 0x3F)<<2; // Take the last 6 bits of byte 1
        addr |= (addrByte2 & 0x06)>>1; // A1 and A0
        addr |= (addrByte2 & 0x70)<<4; // A10, A9, A8
        lastDccMsg_.addr = addr;
        lastDccMsg_.longAddr = true;
    }
    else if(addrByte1 >= dcc_short_addr_14bmulti_start && addrByte1 <= dcc_short_addr_14bmulti_end)
    {
        // In range of 14-bit multi-purpose decoder addresses. The address is encoded in the first two bytes
        // The formula for the address is as follows:
        // [byte1 - 192]*256 + byte2
        uint8_t addrByte2 = dccMsgBuf_[1];
        uint16_t addr = ((addrByte1 - 192) << 8) + addrByte2;
        lastDccMsg_.addr = addr;
        dccDebugInfo_.RxMsgForMpDecoder++;
        lastDccMsg_.longAddr = true;
    }
    else if(addrByte1 == dcc_short_addr_idle)
    {
        // Idle packet, address is 0
        lastDccMsg_.addr = dcc_short_addr_idle;
        dccDebugInfo_.RxMsgIdle++;
        lastDccMsg_.longAddr = false;
    }
    else
    {
        // For reserved space, advanced addressing space. Not supported
        lastDccMsg_.addr = 0;
        lastDccMsg_.longAddr = false;
        lastDccMsg_.validMsg = false;
        dccDebugInfo_.EMsUnsupportedMsgType++;
        return false;
    }

    return true; 
}

bool DccInterface::processCmdType()
{
    // Filter out idle messages
    if(lastDccMsg_.addr == dcc_short_addr_idle)
    {
        // Idle packet, no command type
        lastDccMsg_.msg_type = dcc_msg_idle;
    }
    else if (dccMsgBuf_[0] >= dcc_short_addr_accessory_start && dccMsgBuf_[0] <= dcc_short_addr_accessory_end){
        // Filter out accessory messages. These appear to have a valid MP decoder address and command type, so need to
        // be filtered out here before the command type is determined.
        lastDccMsg_.msg_type = dcc_reader_unsupported;
        dccDebugInfo_.EMsUnsupportedMsgType++;
        return false; // Unsupported message type
    }  
    else
    {
        // Continue for other message types
        uint8_t cmdIdx = lastDccMsg_.longAddr ? 2 : 1; // Command byte is preceeded by 1 address byte for short addresses, and 2 address bytes for long addresses
        uint8_t cmdBits = (dccMsgBuf_[cmdIdx]>>5) & 0x07; // Command bits are bit 7..5 of the command byte
        switch(cmdBits)
        {
            case dcc_msg_aoi:
            case dcc_msg_sdir:
            case dcc_msg_sdif:
            case dcc_msg_fgi1:
            case dcc_msg_fgi2:
            case dcc_msg_cvai:
                lastDccMsg_.msg_type = static_cast<DccMsgType_t>(cmdBits);
                break;
            case dcc_msg_fexp:
            case dcc_msg_dcci:
                lastDccMsg_.msg_type = dcc_reader_unsupported;
                dccDebugInfo_.EMsUnsupportedMsgType++;
                return false; // Unsupported message type
            default:
                lastDccMsg_.msg_type = dcc_reader_error;
                dccDebugInfo_.EMsgInvalidMsgType++;
                return false; // Unsupported message type
        }
    }

    return true;
}

bool DccInterface::processBaselineMsg()
{
    return false; //TODO
}

bool DccInterface::processAdvancedMsg()
{
    return false; //TODO
}

bool DccInterface::processFuncGroupMsg()
{
    return false; //TODO
}
bool DccInterface::processCvWriteMsg()
{
    return false; //TODO
}

void DccInterface::printDccDebugInfo()
{
    if(!DCC_PRINT_DEBUG_INFO)
    {
        return;
    }
    
    static uint32_t lastPrintTime = 0;
    uint32_t currentTime = platform_get_tick_ms();
    if(currentTime - lastPrintTime >= dccDebugPrintPeriod_)
    {
        lastPrintTime = currentTime;
        if(DCC_DEBUG_HALFBITS)
        {
            // Print debug information about the DCC reader state, queue status, etc.
            // loklight_debug_print("QElem: %u, BitSM: %u, ReadSM: %u\n", elementsInQueue(), halfbitState_, dccReaderState_);
            loklight_debug_print("TOTBRX:%u, TOTB1:%u, TOTB0:%u, EBT:%u, EB1DT:%u, EB0TOT:%u, EBSYNC:%u, ", 
                dccDebugInfo_.RxHbTotHalfBits, 
                dccDebugInfo_.RxHbValid1Bits, 
                dccDebugInfo_.RxHbValid0Bits, 
                dccDebugInfo_.EHbBitTimeViolations, 
                dccDebugInfo_.EHbDeltaViolations, 
                dccDebugInfo_.EHbTotTimeViolations, 
                dccDebugInfo_.EHbBadSyncHalfBits);
        }
        if(DCC_DEBUG_FRAMES)
        {
            loklight_debug_print("TOTBYTE: %u, TOTFR:%u, ERESET:%u, EPRE:%u, EFRAME:%u, ECRC:%u, ", 
                dccDebugInfo_.RxFrTotBytes,
                dccDebugInfo_.RxFrValidFrames, 
                dccDebugInfo_.EFrReaderResets, 
                dccDebugInfo_.EFrInvalidPreambles,
                dccDebugInfo_.EFrInvalidFrames, 
                dccDebugInfo_.EFrInvalidCRC);
        }
        if(DCC_DEBUG_MESSAGES)
        {
            loklight_debug_print("TOTMSG: %u, TOTIDLE:%u, TOTALL:%u, TOTDEC:%u, TOTTHIS:%u, ENOSUP:%u, EINV:%u, ",
                dccDebugInfo_.RxMsgValidMsgs, 
                dccDebugInfo_.RxMsgIdle, 
                dccDebugInfo_.RxMsgBroadcast, 
                dccDebugInfo_.RxMsgForMpDecoder,
                dccDebugInfo_.RxMsgTotMsgsForThisUnit,  
                dccDebugInfo_.EMsUnsupportedMsgType,
                dccDebugInfo_.EMsgInvalidMsgType); 
        }
        loklight_debug_print("\r\n");
    }
}


void DccBitTimeQueue::resetQueue()
{
    qErrorFlag_ = true; //Set error flag to avoid writes during reset
    qReadIdx_ = 0;
    qWriteIdx_ = 0;
    qIsFull_ = false;
    qIsEmpty_ = true;
    // Resetting the error flag must happen last, from this point onwards writes are possible again
    qErrorFlag_ = false;
}

bool DccBitTimeQueue::addBitTime(uint32_t t)
{
    // Check if there is actually space to store a bit-time measurement
    if(qIsFull_ || qErrorFlag_)
    {
        qErrorFlag_ = true;
        return false;
    }

    // There is space, store the bitTime
    dccBitTimeQueue_[qWriteIdx_] = t;
    // Set new index, loop around end
    qWriteIdx_++;
    qWriteIdx_%=DCC_BITTIME_QUEUE_SIZE;

    //The queue now is not empty anymore. Here, we just reset the flag.
    //Mind that this function is dominant (can preempt) over the read function, as it is called from an IRQ and the reads happen in threaded/non-irq context.
    //As this function cannot be interrupted, there is no need for complex logic here.
    qIsEmpty_ = false;

    //Now check if the queue is full.
    //If both indexes are equal now, it is because the write function has just done that and the queue is full
    //i.e. there are no scenarios to end up here where the indexes are the same because the queue is empty rather than full
    if(qReadIdx_ == qWriteIdx_)
        qIsFull_ = true;

    //We do not reset this flag here, as this function can only make it full
    //The read function should reset the full-flag.

    return true;
}

uint32_t DccBitTimeQueue::elementsInQueue()
{
    //buffer writeIdx (as the latter can be updated in an IRQ)
    volatile uint16_t bufferedWriteIdx; //volatile to avoid weird optimizations by compiler
    uint32_t retElements = 0;
    do
    {
        bufferedWriteIdx = qWriteIdx_;

        if(qIsEmpty_ || qErrorFlag_)
        {
            // There is nothing to read
            retElements = 0;
        }
        else if(qIsFull_)
        {
            retElements =  DCC_BITTIME_QUEUE_SIZE;
        }
        else if(bufferedWriteIdx < qReadIdx_)
        {
            //Happens when both are not wrapped around
            retElements = DCC_BITTIME_QUEUE_SIZE - (qReadIdx_ - bufferedWriteIdx);
        }
        else if(bufferedWriteIdx > qReadIdx_)
        {
            //Read has wrapped around, write hasn't
            retElements = bufferedWriteIdx - qReadIdx_;
        }
        else
        {
            //We cannot end up here, unless a write has interrupted this function in the mean-time.
            //We will detect this and recalculate the number of readable elements.
            //For now, set to 0
            retElements = 0;
        }
        // Check if we have been interrupted by a write.
    } while(bufferedWriteIdx != qWriteIdx_);
    
    return retElements;
}

uint32_t DccBitTimeQueue::readBitTime()
{
    if(qIsEmpty_ || qErrorFlag_)
    {
        //Nothing to read or an error occured, should not end up here
        return 0;
    }

    //Mind, writing the queue is irq based, reading the queue happens in threaded mode. This function can therefore be interrupted by a write.
    //The main assumption is that this code is fast enough so that only one interrupt by a write can occur for any single read.
    //We therefore check if such an interrupt occured and handle any queue empty/full logic gracefully.
    volatile uint16_t bufferedWriteIdx = qWriteIdx_; //Volatile shouldn't needed here because qWriteIdx_ is too. But just to be sure..
    bool interruptedByWrite = false;
    // Keep track of our actions so we can reset them in case of an interrupted read
    uint16_t nextReadIdx = (qReadIdx_ + 1) % DCC_BITTIME_QUEUE_SIZE;
    bool qEmptyAfterRead = (nextReadIdx==bufferedWriteIdx);

    // Note that checking the qIsFull flag alone is not enough.
    // Between indexing bufferedWriteIdx and performing this check, it could be possible that a write has occured and the flag has been set false->true because the queue was ALMOST full
    // For this special case, we know are going to perform a read and therefore the queue would not actually be full anymore by the end of the function.
    // Therefore, verify that the queue was full by also checking the indexes.
    bool qWasFullBeforeRead = ((qReadIdx_==bufferedWriteIdx) && qIsFull_);
    
    // First extract the bit to be returned
    uint32_t ret = dccBitTimeQueue_[qReadIdx_];
    qReadIdx_++;
    qReadIdx_%=DCC_BITTIME_QUEUE_SIZE;

    // Set the flags, assuming we haven't been interrupted
    // Obviously queue can't be full anymore when we have removed one element
    // If the queue was full before reading, and we are interrupted before resetting the full-flag, 
    // the write function will still see a full queue and an error-flag will be set
    // The queue and dcc-sync will then be reset by another method, so it does not lead to undefined behaviour.
    qIsFull_ = false;

    if(qEmptyAfterRead)
    {
        qIsEmpty_ = true;
    }

    // The normal part is done here. Now check if we were actually interrupted and reset the flags if so
    interruptedByWrite = (bufferedWriteIdx != qWriteIdx_);
    if(interruptedByWrite)
    {
        if(qEmptyAfterRead)
        {
            //The queue can't be empty anymore: there was a write while we were reading
            qIsEmpty_ = false;
        }
        if(qWasFullBeforeRead)
        {
            //The queue was full before reading. We read one bit, but another has been added in the mean-time, so it's full again
            qIsFull_ = true;
        }
    }

    return ret;
}