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

extern "C" bool config_write_to_flash_map(const void* data, size_t size, uint32_t offset)
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
    // First recast the general void pointer to the local word size
    uint32_t* flashPtr = reinterpret_cast<uint32_t*>(CV_MEM_START_ADDR) + offset;
    uint32_t* nativeDataPtr = reinterpret_cast<uint32_t*>(const_cast<void*>(data));
    
    // Write memory block to flash
    while(result && size > 0)
    {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, reinterpret_cast<uint32_t>(flashPtr), *nativeDataPtr) == HAL_OK)
        {   // Move to the next word
            flashPtr++;   
            nativeDataPtr++;
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