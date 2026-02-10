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
    dccVarState_ = {0, DCC_DIRECTION_FORWARD, 0};  //Set speed to 0, all functions off
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
        lastDccMsg_ = {0, false, no_new_dcc_msg, {0}, 0, DCC_DIRECTION_FORWARD, 0, 0, 0};
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
    bool ret = false;
    if(lastDccMsg_.addr == dccConfig_.addr)
    {   // Addressed to this unit
        // Check if the address was for a multipurpose decoder, not an accessory decoder with overlapping address space
        if(dccMsgBuf_[0] >= dcc_short_addr_accessory_start && dccMsgBuf_[0] <= dcc_short_addr_accessory_end)
        {   // This was actually a message for an accessory
            ret = false;
        }
        else
        {   // Message for this unit
            ret = true;
        }
    }
    if(lastDccMsg_.addr == dcc_short_addr_all)
    {   // Broadcast message, relevant for all units
        ret = true;
    }
    
    return ret;
}

bool DccInterface::processDccMsg()
{
    // Step 0: reset the buffer
    lastDccMsg_ = {0, false, no_new_dcc_msg, {0}, 0, DCC_DIRECTION_FORWARD, 0, 0, 0};
    
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

    // Step 3: Process the command
    bool cmdWasProcessed = false;
    switch(lastDccMsg_.msg_type)
    {
        case dcc_msg_aoi:
            cmdWasProcessed = processAdvancedMsg();
            break;
        case dcc_msg_sdir:
        case dcc_msg_sdif:
            cmdWasProcessed = processBaselineMsg();
            break;
        case dcc_msg_fgi1:
        case dcc_msg_fgi2:
            cmdWasProcessed = processFuncGroupMsg();
             break;
        case dcc_msg_cvai:
            cmdWasProcessed = processCvWriteMsg();
            break;
        case dcc_msg_fexp:
        case dcc_msg_dcci:
        // These messages are not supported
        // Note that the previous function should already have logged the error and returned false
        // The code below is just in case, it should never be reached
            dccDebugInfo_.EMsUnsupportedMsgType++;
        case no_new_dcc_msg:
        case dcc_msg_idle:
        // Do nothing with these message types
            lastDccMsg_.validMsg = false; // These message types are not relevant for Loklight, so we mark them as invalid to avoid processing them further down the line
            cmdWasProcessed = false;
            break;
        default:
            // Should not end up here, throw error
            lastDccMsg_.msg_type = dcc_reader_error;
            dccDebugInfo_.EMsgInvalidMsgType++;
            cmdWasProcessed = false; 
    }
    if(!cmdWasProcessed)
    {
        return false; // Command processing failed
    }

    lastDccMsg_.validMsg = true;
    dccDebugInfo_.RxMsgValidMsgs++;

    // Step 4: Copy state to runtime variables if the message is for this unit
    if(!isMsgForThisUnit())
    {
        // Message is not for this unit, stop processing
        // Return true because the reception has succesfully finished
        return true;
    }
    
    dccDebugInfo_.RxMsgTotMsgsForThisUnit++;
    // Apply message to state
    if(!applyMsgToState())
    {
        // Applying the message to the state failed, stop processing
        return false;
    }

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

// Note 1: When no speedSetting is provided (dcc_reinterpret_baseline_none), the function interprets the speed and F0 as if in 28SS mode
// Note 2: The function will interpret the message no matter what mode has been configured.
//          This is to be backwards compatible when 128SS mode is configured but an older DCC controller is in use or 28SS has been selected by mistake on the controller
bool DccInterface::processBaselineMsg(DccReinterpretBaseline_t speedSetting)
{
    // Sanity check, is this a baseline message?
    if(lastDccMsg_.msg_type != dcc_msg_sdir && lastDccMsg_.msg_type != dcc_msg_sdif)
    {
        return false;
    }
    
    // This is a message with 1 or 2 address bytes and a single command byte
    uint8_t cmdIdx = lastDccMsg_.longAddr ? 2 : 1;
    uint8_t cmdByte = dccMsgBuf_[cmdIdx];
    // Copy over to message buffer
    memset(lastDccMsg_.cmd_arg, 0, sizeof(lastDccMsg_.cmd_arg));
    lastDccMsg_.cmd_arg[0] = cmdByte;

    // Format of the command byte is as follows:
    // [0, 1, D, C, S3, S2, S1, S0]
    // For 14-step mode, the C indicates F0 on or off
    // For 28-step mode, the C is used as additional LSB for speed
    uint8_t dBit = (cmdByte >> 5) & 0x01;   // Direction bit
    uint8_t cBit = (cmdByte >> 4) & 0x01;   // C bit
    uint8_t speedBits = cmdByte & 0x0F;     // S3..S0
    // The speed bits need additional processing, as values 0 and 1 indicate a stop
    if((speedBits == 0x00) || (speedBits == 0x01))
    {
        lastDccMsg_.speed = 0; // Stop
    }
    else
    {
        // Check how we are supposed to interpret the speed bits, depending on the function argument
        if(speedSetting == dcc_reinterpret_baseline_14ss)
        {
            // In 14-step mode, the C bit indicates F0 on/off, and is not part of the speed. The speed is determined by S3..S0 as follows:
            // 0 and 1: stop
            // 2: step 1
            // 3: step 2
            // ...
            // 15: step 14
            speedBits -= 0x01; // Minus 1 because step 1 starts at value 2
            lastDccMsg_.speed = (uint8_t) speedBits;
        }
        else if((speedSetting == dcc_reinterpret_baseline_28ss) || (speedSetting == dcc_reinterpret_baseline_none))
        {
            // In 28-step mode or when no interpretation is provided, the C bit is used as an additional LSB for speed
            speedBits = (speedBits << 1) | cBit;
            // minus 3, because in 28-step mode with the c-bit as LSB,
            // The format is [S3 .. S0, C], Step 1 is 0b00100 in this format, i.e. 4.
            speedBits -= 0x03; 
            lastDccMsg_.speed = (uint8_t) speedBits;
        }
        else
        {
            // We cannot end up here, invalid argument
            return false;
        }
    }

    // Direction processing.
    lastDccMsg_.direction = dBit ? DCC_DIRECTION_FORWARD : DCC_DIRECTION_REVERSE;
    
    // Front/Rear light processing. Only do this when not in 28-step mode.
    // Note when no speedSetting is provided, the function interprets the speed and F0 as if in 28SS mode
    if(speedSetting == dcc_reinterpret_baseline_14ss && cBit)
    {
        // F0 is on
        lastDccMsg_.af_group1 = dBit ? DCC_FUNC_F0F : DCC_FUNC_F0R; // F0 on, direction determines if front or rear light
    }
    else
    {
        // Front light off
        lastDccMsg_.af_group1 = 0x00; // F0 off
    }
    
    return true;
}

bool DccInterface::processAdvancedMsg()
{
    // Sanity check, is this an advanced operation message?
    if(lastDccMsg_.msg_type != dcc_msg_aoi)
    {
        return false;
    }
    
    // This is a message with 1 or 2 address bytes and two command bytes
    uint8_t cmdIdx = lastDccMsg_.longAddr ? 2 : 1;
    uint8_t cmdByte = dccMsgBuf_[cmdIdx];
    // Copy over to message buffer
    memset(lastDccMsg_.cmd_arg, 0, sizeof(lastDccMsg_.cmd_arg));
    memcpy(lastDccMsg_.cmd_arg, &dccMsgBuf_[cmdIdx], sizeof(uint8_t)*2);
    
    // Sanity check, are we processing a speed instruction or an unsupported instruction?
    uint8_t cmdTypeBits = cmdByte & 0x1f; // Instruction bits are bits 4..0 of the command byte
    // For 128SS instructions, the instruction bits are 0b1 1111
    // For ZIMO east-west direction instructions, the bits are 0b 1 1110 (unsupported)
    // For analog instructions, the bits are 0b 1 1101 (unsupported)
    // Other instructions are reserved by the DCC standard and not supported
    if(cmdTypeBits != 0b11111)
    {
        // Unsupported instruction, return false
        lastDccMsg_.msg_type = dcc_reader_unsupported;
        dccDebugInfo_.EMsUnsupportedMsgType++;
        return false;
    }

    // If we are here, this is a valid 128SS instruction.
    uint8_t speedByte = dccMsgBuf_[cmdIdx + 1];
    // Extract direction bit
    bool dBit = (speedByte >> 7) & 0x01;   // Direction bit is bit 7 of the speed byte
    // We need to reverse the direction bit in case of reverse direction config
    lastDccMsg_.direction = dBit ? DCC_DIRECTION_FORWARD : DCC_DIRECTION_REVERSE;
    
    // Scale speed
    // In 128SS mode, steps 0 and 1 are stop and e-stop, respectively
    // The steps go from 1 to 126.
    uint8_t speed = (speedByte & 0x7F); // Speed bits are bits 6..0 of the speed byte, first step is 
    if((speed == 0) || (speed == 1))
    {
        lastDccMsg_.speed = 0; // Stop
    }
    else
    {
        speed -= 1; // Minus 1 because step 1 starts at value 2
        lastDccMsg_.speed = speed;
    }

    return true;
}

bool DccInterface::processFuncGroupMsg()
{
    return true; //TODO
}
bool DccInterface::processCvWriteMsg()
{
    return true; //TODO
}

bool DccInterface::applyMsgToState()
{
    bool ret = false;
    //Step 1: check if this is a valid message for this unit
    if(!lastDccMsg_.validMsg || !isMsgForThisUnit())
    {
        return ret;
    }
    //Step 2: check what message it is and apply it to the state
    switch(lastDccMsg_.msg_type)
    {
        case dcc_msg_sdir:
        case dcc_msg_sdif:
            ret = applyBasicMsgToState();
            break;
        case dcc_msg_aoi:
            ret = applyAdvancedMsgToState();
            break;
        case dcc_msg_fgi1:
        case dcc_msg_fgi2:
        case dcc_msg_cvai:
        default:
            // Should not end up here, return false
            ret = false;
    }
    
    return ret;
}

bool DccInterface::applyBasicMsgToState()
{
    // Step 1: check if this is a valid message for this function
    if(!lastDccMsg_.validMsg || !isMsgForThisUnit() || !(lastDccMsg_.msg_type == dcc_msg_sdir || lastDccMsg_.msg_type == dcc_msg_sdif))
    {
        return false;
    }
    // Step 2: Interpret the speed settings and update the F0 function
    // Check if we change direction so we can update F0 state accordingly.
    DccDirection_t newDir;
    if(dccConfig_.direction == DCC_DIRECTION_REVERSE)
    {
        // We need to reverse the direction bit in case of reverse direction config
        newDir = (lastDccMsg_.direction == DCC_DIRECTION_FORWARD) ? DCC_DIRECTION_REVERSE : DCC_DIRECTION_FORWARD;
    }
    else
    {
        // Normal direction, copy bit over
        newDir = lastDccMsg_.direction;
    }

    // Either apply it as a 14SS message or
    // as a 128SS instruction that has been sent through an older 28SS message format
    if(dccConfig_.controlMode == DCC_CONTROL_MODE_DCC_14SS)
    {
        // Reinterpret the message as in 14ss config
        if(!processBaselineMsg(dcc_reinterpret_baseline_14ss))
        {
            return false; // Reinterpretation failed, stop processing
        }
        // Apply F0 state to variable state
        // For 14SS, the F0 state is encoded directly in the message.
        // In this simplified scheme, either F0F is on or F0R but not both.
        uint16_t f0Msk = (DCC_FUNC_F0F | DCC_FUNC_F0R);   // Mask to reset F0 bits
        uint8_t f0Bits = 0;
        if(dccConfig_.direction==DCC_DIRECTION_REVERSE)
        {
            //We need to reverse the F0 bits in case of reverse direction config
            f0Bits |= (lastDccMsg_.af_group1 & DCC_FUNC_F0F) ? DCC_FUNC_F0R : 0;
            f0Bits |= (lastDccMsg_.af_group1 & DCC_FUNC_F0R) ? DCC_FUNC_F0F : 0;
        }
        else
        {
            //Normal direction, copy bits over
            f0Bits = lastDccMsg_.af_group1 & f0Msk;
        }
        dccVarState_.funcEnanbled &= ~f0Msk;     // Reset F0F and F0R in active state
        dccVarState_.funcEnanbled |= f0Bits;     // Set F0F and F0R
    }   // End of Step 2: setting F0 bits for 14SS
    else
    {
        // Interpret for 28SS/128SS mode
        if(!processBaselineMsg(dcc_reinterpret_baseline_28ss))
        {
            return false; // Processing failed, stop processing
        }

        // Compare newDir to the previous direction and change F0 if needed.
        // Note that the F0F and F0R bits themselves are set in another type of message for 28SS mode
        if(newDir != dccVarState_.direction)
        {
            reverseF0();
        }   
    }   // End of step 2: setting F0 bits for 28SS/128SS

    //Step 3: apply speed. The speed has been rescaled in step 2a if applicable.
    // If in 14SS mode, the speed is ready.

    // We do not actually supported 28SS mode, as it makes no sense given the role of CV29,
    // That is to use a basic message for lights too (14SS) or not (28SS or 128SS, seperate function cmds). There is no way
    // to configure 28SS specifically in a generic DCC decoder, therefore we just interpret this situation as 128SS.
    // Therefore, scale the speed up to 128SS range when not in 14SS mode
    if(dccConfig_.controlMode != DCC_CONTROL_MODE_DCC_14SS)
    {
        // This means we are in 128SS mode. It has 126 steps, so the conversion factor from 28 is
        // new = old * 4.5
        // The speed is scaled from 1-28 to 4-112 first
        // Then a (linearly scaled) offset of 0 to 14 is added to scale it to 4 - 126.
        uint8_t scaledSpeed = lastDccMsg_.speed;
        // Do not change speed 0 (stop)
        if(scaledSpeed > 0)
        {
            scaledSpeed = scaledSpeed * 4 + (scaledSpeed >> 1);
        }
        dccVarState_.speed = scaledSpeed;
    }   // End of step 3: speed scaling for 28SS/128SS
    else
    {
        // In 14SS mode, the speed is ready to be applied directly
        dccVarState_.speed = lastDccMsg_.speed;
    }   //End of step 3: speed setting for 14SS

    // Step 4: Update direction
    if(dccConfig_.direction == DCC_DIRECTION_REVERSE)
    {
        // We need to reverse the direction bit in case of reverse direction config
        dccVarState_.direction = (lastDccMsg_.direction == DCC_DIRECTION_FORWARD) ? DCC_DIRECTION_REVERSE : DCC_DIRECTION_FORWARD;
    }
    else
    {
        // Normal direction, copy bit over
        dccVarState_.direction = lastDccMsg_.direction;
    }

    return true;
}

bool DccInterface::applyAdvancedMsgToState()
{
    // Step 1: check if this is a valid message for this function
    if(!lastDccMsg_.validMsg || !isMsgForThisUnit() || !(lastDccMsg_.msg_type == dcc_msg_aoi) || (dccConfig_.controlMode != DCC_CONTROL_MODE_DCC_128SS))
    {
        return false;
    }

    // Step 2: copy the speed
    dccVarState_.speed = lastDccMsg_.speed;

    // Step 3: update the F0 function state
    // Check if we change direction and update F0 state accordingly.
    DccDirection_t newDir;
    if(dccConfig_.direction == DCC_DIRECTION_REVERSE)
    {
        // We need to reverse the direction bit in case of reverse direction config
        newDir = (lastDccMsg_.direction == DCC_DIRECTION_FORWARD) ? DCC_DIRECTION_REVERSE : DCC_DIRECTION_FORWARD;
    }
    else
    {
        // Normal direction, copy bit over
        newDir = lastDccMsg_.direction;
    }

    // Compare newDir to the previous direction and change F0 if needed.
    if(newDir != dccVarState_.direction)
    {
        reverseF0();
    }
    
    // Step 4: Update direction
    dccVarState_.direction = newDir;

    return true;
}

void DccInterface::reverseF0()
{
    // Toggle F0, useful for direction changes in 28SS/128SS mode. In 14SS mode.
    // Note that the code below also allows both to be on or off.
    bool f0FwasOn = (dccVarState_.funcEnanbled & DCC_FUNC_F0F) != 0;
    bool f0RwasOn = (dccVarState_.funcEnanbled & DCC_FUNC_F0R) != 0;

    // Reset F0 bits
    dccVarState_.funcEnanbled &= ~(DCC_FUNC_F0F | DCC_FUNC_F0R);
    if(f0FwasOn)
    {
        // F0 was on in forward direction, turn it on in reverse direction
        dccVarState_.funcEnanbled |= DCC_FUNC_F0R;
    }
    if(f0RwasOn)
    {
        // F0 was on in reverse direction, turn it on in forward direction
        dccVarState_.funcEnanbled |= DCC_FUNC_F0F;
    }
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
        if(DCC_DEBUG_STATE)
        {
            loklight_debug_print("SPEED:%u, DIR:%s, F0F:%u, F0R:%u, FUNC:%u, ", 
                dccVarState_.speed, 
                (dccVarState_.direction == DCC_DIRECTION_FORWARD) ? "FWD" : "REV",
                (dccVarState_.funcEnanbled & DCC_FUNC_F0F) ? 1 : 0, 
                (dccVarState_.funcEnanbled & DCC_FUNC_F0R) ? 1 : 0, 
                dccVarState_.funcEnanbled);
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