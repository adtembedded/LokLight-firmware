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

#include "loklight_wrapper.h"
#include "loklight.h"
#include "led_control.h"
#include "config.h"
// use HAL includes here
#include "stm32l0xx_hal.h"

extern "C" uint32_t platform_get_tick_ms(void)
{
    // Change as needed to get a ms value for platform system time
    return HAL_GetTick();
}

extern "C" void platform_delay_ms(uint32_t ms)
{
    // Change as needed to implement a delay in ms
    HAL_Delay(ms);
}

extern "C" void platform_reset()
{
    // Change as needed to implement a system reset
    NVIC_SystemReset();
}

extern "C" bool config_erase_cv_flash_map()
{
    static_assert(CV_MEM_SIZE % FLASH_PAGE_SIZE == 0, "CV_MEM_SIZE must be a multiple of the flash page size");
    bool result = false;
    __disable_irq();    // Disable interrupts to prevent flash access conflicts
    FLASH_EraseInitTypeDef pEraseInit;
    pEraseInit.PageAddress = CV_MEM_START_ADDR;             // CV memory starts here
    HAL_FLASH_Unlock(); // Allow control of the flash registers to perform erase operation
    pEraseInit.NbPages = CV_MEM_SIZE / FLASH_PAGE_SIZE;     // Number of pages to erase based on the defined CV memory size
    pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;           // Erase by page
    uint32_t pageError; // Variable to store page error in case of failure. If erase is successful, contains 0xffff ffff
    if(HAL_FLASHEx_Erase(&pEraseInit, &pageError) == HAL_OK)
    {
        result = true;    // Erase successful
    }
    HAL_FLASH_Lock();   // Relock flash mem when done
    __enable_irq();     // Re-enable interrupts
    return result;
}

//TODO
// This function writes the CV map including the pre- and postfix. These are implementation specific details of the config class
// which do not really belong here. For now, however, it is the most convenient way to make sure a non-word aligned CV map (uint16 per entry) can be written
// to the word-accessed flash memory of an STM32 arm cortex-m device. Also, the (un)locking of flash memory and other platform-specific functions
// should ideally happen only once, which is the case for the implementation below.
//
// A future implementation must define the alignment size in loklight_wrapper.h, then generate a (default) cvmap with appropriate padding in config.h/config.cpp.
// It should then use a generalistic flash-writing function in the wrapper to write the CV map to flash by providing a generalized memory block & size spec.
// This could already have been done here by generating a new memory object that is [PREFIX CV-MAP POSTFIX] in the config class
// and passing it to the function below.
// However, there isn't enough RAM memory on the STM32C011 to support making a temporary CV-map copy when (memory-heavy) debugging features are also enabled.
//
// Alternatively, the config class can perform writes for the PREFIX, cv-map and POSTFIX seperately.

extern "C" bool config_write_cv_flash_map(uint8_t* cvMapPtr, size_t size)
{
    static_assert(CV_MEM_SIZE % sizeof(uint32_t) == 0, "CV_MEM_SIZE must be a multiple of 32-bit word size");
    if(size+sizeof(CV_MEM_PREFIX)+sizeof(CV_MEM_POSTFIX) > CV_MEM_SIZE)
    {
        return false;   // Size of data exceeds reserved flash memory for CVs
    }

    bool result = true;    // Assume success until a write operation fails
    __disable_irq();    // Disable interrupts to prevent flash access conflicts
    HAL_FLASH_Unlock(); // Allow control of the flash registers to perform write operation
    // Write the CV map to flash memory word by word (32-bit)
    // First recast the general byte pointer to the local word size
    uint32_t* flashPtr = reinterpret_cast<uint32_t*>(CV_MEM_START_ADDR);
    // Write prefix to flash
    if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), *reinterpret_cast<const uint32_t*>(CV_MEM_PREFIX)) == HAL_OK)
    {
        flashPtr++;   // Move to the next word after writing prefix
    }
    else
    {
        result = false;   // Write failed
    }

    // Only attempt to write data if prefix write was successful
    if(result)   
    {
        while(size > 3)   // While there are at least 4 bytes (32 bits) left to write
        {
            // Assemble a word from the data pointer
            // We need to do this, as the pointer does not have to be word-aligned
            // If we give a non-word-aligned pointer to HAL_FLASH_Program it will cause a hard fault
            uint32_t word = 0;
            word = (cvMapPtr[0] << 0) | (cvMapPtr[1] << 8) | (cvMapPtr[2] << 16) | (cvMapPtr[3] << 24);
            if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), word) == HAL_OK)
            {
                flashPtr++;   // Move to the next word after successful write
                cvMapPtr+= 4; // Move to the next word in the data pointer
                size -= 4;    // Decrease remaining size by 4 bytes
            }
            else
            {
                result = false;   // Write failed, exit loop
                break;
            }
        }
    }

    // Write last part and postfix to flash
    if(result)    // Only attempt to write postfix if all previous writes were successful
    {
        if (size > 0)  // There are remaining bytes
        {
            uint32_t lastWords[2] = {0,0};
            size_t i = 0;
            while(i < size) // This is the part that remains after an N x word sized block has been written
            {
                lastWords[0] |= (cvMapPtr[i] << (i*8)); // Assemble remaining bytes into a word. Convert to little endian format
                i++;
            }
            while(i < 4)    // For the remaining space in the word var, add the start of the postfix
            {
                lastWords[0] |= CV_MEM_POSTFIX[i-size] << (i*8); // After data bytes, fill the rest of the word with the beginning of the postfix. Convert to little endian format
                i++;
            }
            while(i-size < sizeof(CV_MEM_POSTFIX)) // Write the rest of the postfix into the next word var
            {
                lastWords[1] |= CV_MEM_POSTFIX[i-size] << ((i-4)*8); // Write remaining part of postfix into second word. Convert to little endian format
                i++;
            }
            while(i < 8)    // Pad the rest of the second word with 0xff
            {
                // Pad with 0xff, which is the default erased state of flash memory
                lastWords[1] |= 0xFF << ((i-4)*8);
                i++;
            }
            if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), lastWords[0]) != HAL_OK)
            {
                result = false;   // Write failed
            }
            flashPtr++;
            if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), lastWords[1]) != HAL_OK)
            {
                result = false;   // Write failed
            }
            flashPtr++;
        }
        else    // Happens when CV map was word-aligned
        {
            if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), *reinterpret_cast<const uint32_t*>(CV_MEM_POSTFIX)) != HAL_OK)
            {
                result = false;   // Write failed
            }
        }
    }

    HAL_FLASH_Lock();   // Relock flash mem when done
    __enable_irq();     // Re-enable interrupts    
    return result;
}

extern "C" bool config_write_to_flash_map(void* data, size_t size, uint32_t offset)
{
    static_assert(CV_MEM_SIZE % sizeof(uint32_t) == 0, "CV_MEM_SIZE must be a multiple of 32-bit word size");
    if(size+sizeof(CV_MEM_PREFIX)+sizeof(CV_MEM_POSTFIX) > CV_MEM_SIZE)
    {
        return false;   // Size of data exceeds reserved flash memory for CVs
    }

    bool result = true;    // Assume success until a write operation fails
    __disable_irq();    // Disable interrupts to prevent flash access conflicts
    HAL_FLASH_Unlock(); // Allow control of the flash registers to perform write operation
    // Write the CV map to flash memory word by word (32-bit)
    // First recast the general byte pointer to the local word size
    uint32_t* flashPtr = reinterpret_cast<uint32_t*>(CV_MEM_START_ADDR) + offset;
    
    // Write memory block to flash
    while(result && size > 0)
    {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), *reinterpret_cast<const uint32_t*>(data)) == HAL_OK)
        {
            flashPtr++;   // Move to the next word after writing prefix
        }
        else
        {
            result = false;   // Write failed
        }
        size--;
    }

    HAL_FLASH_Lock();   // Relock flash mem when done
    __enable_irq();     // Re-enable interrupts    
    return result;
}

extern "C" void loklight_debug_print(const char* sFormat, ...)
{
#if(PLATFORM_DEBUGGING)
    {
        // You can implement this using SEGGER RTT, UART, or any other debugging output method you prefer
        va_list ParamList;
        va_start(ParamList, sFormat);
        SEGGER_RTT_vprintf(0, sFormat, &ParamList);
        va_end(ParamList);
    }
#endif
}

// Initialization, call after creation and HW init
extern "C" bool loklight_init(LedHwInitCfg_t* ledHwInitCfg, DccHwInitCfg_t* dccHwInitCfg)
{
    LoklightInitResult_t result;
    Loklight& ll_instance = Loklight::getInstance();
    result = ll_instance.init(ledHwInitCfg, dccHwInitCfg);
    bool result_bool = (result == LOKLIGHT_INIT_OK)? true : false;
    return result_bool;
}

extern "C" bool loklight_init_status()
{
    Loklight& ll_instance = Loklight::getInstance();
    bool result = ll_instance.isInitialized();
    return result;
}

extern "C" bool loklight_step()
{
    Loklight& ll_instance = Loklight::getInstance();
    return ll_instance.step();
}

// Overwrite when using another hardware implementation
extern "C" void led_control_set_pwm(LedNumber_t led_number, uint8_t brightness)
{
    //Use the STM32 HAL to set the PWM duty cycle at runtime
    //In this implementation, TIM2 is used on hardware PWM mode on channel 3 and 4
    //The COMPARE reg resolution is 16-bit, therefore we need to scale brightness from 0-255 to 0-65535
    TIM_HandleTypeDef htimer;
    htimer.Instance = TIM2;
    uint16_t brightness_16b = (uint16_t) (brightness<<8) + brightness;  //Linearly scale 0x00-0xFF to 0x0000-0xFFFF
    
    if(led_number == LED1)
    {
        __HAL_TIM_SET_COMPARE(&htimer, TIM_CHANNEL_3, brightness_16b);
    }
    
    if(led_number == LED2)
    {
        __HAL_TIM_SET_COMPARE(&htimer, TIM_CHANNEL_4, brightness_16b);
    }
}

extern "C" bool dcc_bit_queue_add(uint32_t bit_time)
{
    DccInterface& dcc_itf = DccInterface::getInstance();
    bool result = dcc_itf.addBitTime(bit_time);
    return result;
}

extern "C" bool dcc_hw_read_analog_direction()
{
    // On the Loklight PCB, this pin is the DCC Sense pin.
    // It is connected to the glowbulb body (not tip) connection, which should be the right track.
    // On old inductor/variac based controllers this signal will fluctuate so we implement a memory efficient filter
    static uint8_t high_cnt = 0;
    GPIO_PinState dcc_sense_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
    if(dcc_sense_state == GPIO_PIN_SET)
    {
        if(high_cnt < 0xff)
        {
            high_cnt++;
        }
    }
    else
    {
        if(high_cnt > 0)
        {
            high_cnt--;
        }
    }
    // This last number is the noise tolerance
    return (high_cnt > 0x1f) ? true : false;
}