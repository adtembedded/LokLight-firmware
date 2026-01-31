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

DccInterface::DccInterface()
    : dccBitTimeQueue_()
{
}

DccInterface::~DccInterface()
{
}

bool DccInterface::init(DccHwInitCfg_t* initHwCfg /*= nullptr*/, DccConfig_t* initCfg /*= nullptr*/)
{
    //Set software config. This is optional
    if(initCfg)
    {
        dccConfig_ = *initCfg;  //Copy the data
    }
    
    //Initialize hardware, this is mandatory
    if(initHwCfg)
    {
        // Initialize PWM timer for DCC reading
        isInitialized_ = false;

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
        return isInitialized_;
    }
    // Hardware init config missing
    return false;
}

bool DccInterface::addBitTime(uint32_t t)
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

uint32_t DccInterface::elementsInQueue()
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

uint32_t DccInterface::readBitTime()
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