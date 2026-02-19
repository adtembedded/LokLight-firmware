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

extern "C" void loklight_debug_print(const char* sFormat, ...)
{
    // Placeholder implementation
    // You can implement this using SEGGER RTT, UART, or any other debugging output method you prefer
    va_list ParamList;
    va_start(ParamList, sFormat);
    SEGGER_RTT_vprintf(0, sFormat, &ParamList);
    va_end(ParamList);
}

// Re-enable when create and destroy are used
// struct LoklightWrapper_s
// {
//     Loklight ll;                    // Actual C++ Loklight instance
//     LoklightWrapper_s() : ll() {}   // Initializer list adds Loklight object to the wrapper
// };

// Create and destroy
// Enable when class isn't a singleton
// extern "C" LoklightHandle loklight_create(void)
// {
//     return reinterpret_cast<LoklightHandle>(new LoklightWrapper_s()); //Need unsafe conversion to allow C code to use C++ object
// }

// Enable when class isn't a singleton
// extern "C" void loklight_destroy(LoklightHandle handle)
// {
//     delete reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
// }

// Get the handle of the singleton instance (not needed)
// extern "C" LoklightHandle loklight_get_instance(void)
// {
//     return reinterpret_cast<LoklightHandle>(Loklight::getInstancePtr()); //Need unsafe conversion to allow C code to use C++ object
// }

// Initialization, call after creation and HW init
// extern "C" bool loklight_init(LoklightHandle handle, LedControlInitCfg_t* ledInitCfg)
extern "C" bool loklight_init(LedHwInitCfg_t* ledHwInitCfg, DccHwInitCfg_t* dccHwInitCfg)
{
    LoklightInitResult_t result;
    //LoklightWrapper_s* wrapper = reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
    // result = wrapper->ll.init(ledInitCfg);
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

// extern "C" bool loklight_step(LoklightHandle handle)
extern "C" bool loklight_step()
{
    // LoklightWrapper_s* wrapper = reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
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