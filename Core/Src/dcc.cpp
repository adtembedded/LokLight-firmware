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

bool DccInterface::addBitTime(uint32_t t)
{
    // Check if there is actually space to store a bit-time measurement
    if(qIsFull_)
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
    //The read function will handle concurrent access to this flag gracefully.
    qIsEmpty_ = false;

    //Now check if the queue is full.
    //This function has priority (can preempt) the read by being called from an IRQ, but not vice versa
    //Therefore, if both indexes are equal now, it is because the write function has just done that and the queue is full
    //i.e. there are no scenarios to end up here where the indexes are the same because the queue is empty rather than full
    if(qReadIdx_ == qWriteIdx_)
        qIsFull_ = true;

    //We do not reset this flag here, as this function can only make it full
    //The read function should reset the flag.
    return true;
}

uint32_t DccInterface::readBitTime()
{
    if(qIsEmpty_)
    {
        //Nothing to read, should not end up here
        return 0;
    }
    
    // First extract the bit to be returned
    uint32_t ret = dccBitTimeQueue_[qReadIdx_];
    qReadIdx_++;
    qReadIdx_%=DCC_BITTIME_QUEUE_SIZE;

    //Mind, filling the queue is irq based. Therefore, the next part can potentially be interrupted
    //and has to be constructed in such a way that this would be safe.
    //To do that, we buffer the writeIdx and compare it at the end to detect if this loop has been interrupted.
    volatile uint16_t bufferedWriteIdx; //Volatile shouldn't needed here because qWriteIdx_ is too. But just to be sure..
    bool qWriteHappened;
    do
    {
        //Set the indexed value equal to the writeIdx
        bufferedWriteIdx = qWriteIdx_;

        //Check if all has been read
        //This happens when both indexes are equal, but it can also happen when the buffer is almost full and we have just been
        //interrupted by the addBitTime() method which has made the buffer completely buffer.
        //Therefore check qIsFull_, which is set when the latter scenario happens
        if(qReadIdx_==bufferedWriteIdx && !qIsFull_)
        {
            qIsEmpty_ = true;
        }
        if(qReadIdx_!=bufferedWriteIdx)
        {
            qIsEmpty_ = false;
        }
        
        //If the writeIdx was changed while performing above checks by handling the bit-time Irq, we will see it here.
        //In that case, the loop will iterate again and set qIsEmpty_ = false again if it happened to be true in the first round.
        qWriteHappened = (bufferedWriteIdx != qWriteIdx_);
    } while (qWriteHappened);

    return ret;
}
