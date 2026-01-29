/*
* Loklight
* Copyright (C) 2026 ADT Embedded
* 
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "loklight_wrapper.h"
#include "loklight.h"
#include "led_control.h"
// use HAL includes here
#include "stm32l0xx_hal.h"

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

// Get the handle of the singleton instance
extern "C" LoklightHandle loklight_get_instance(void)
{
    return reinterpret_cast<LoklightHandle>(Loklight::getInstancePtr()); //Need unsafe conversion to allow C code to use C++ object
}

// Initialization, call after creation and HW init
extern "C" bool loklight_init(LoklightHandle handle, LedControlInitCfg_t* ledInitCfg)
{
    LoklightInitResult_t result;
    //LoklightWrapper_s* wrapper = reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
    // result = wrapper->ll.init(ledInitCfg);
    Loklight& ll_instance = Loklight::getInstance();
    result = ll_instance.init(ledInitCfg);
    bool result_bool = (result == LOKLIGHT_INIT_OK)? true : false;
    return result_bool;
}

extern "C" bool loklight_step(LoklightHandle handle)
{
    // LoklightWrapper_s* wrapper = reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
    Loklight& ll_instance = Loklight::getInstance();
    return ll_instance.step();
}

// Overwrite when using another hardware implementation
extern "C" void led_control_set_pwm(LedNumber_t ledNumber, uint8_t brightness)
{
    //Use the STM32 HAL to set the PWM duty cycle at runtime
    //In this implementation, TIM2 is used on hardware PWM mode on channel 3 and 4
    //The COMPARE reg resolution is 16-bit, therefore we need to scale brightness from 0-255 to 0-65535
    TIM_HandleTypeDef htimer;
    htimer.Instance = TIM2;
    uint16_t brightness_16b = (uint16_t) (brightness<<8) + brightness;  //Linearly scale 0x00-0xFF to 0x0000-0xFFFF
    
    if(ledNumber == LED1)
    {
        __HAL_TIM_SET_COMPARE(&htimer, TIM_CHANNEL_3, brightness_16b);
    }
    
    if(ledNumber == LED2)
    {
        __HAL_TIM_SET_COMPARE(&htimer, TIM_CHANNEL_4, brightness_16b);
    }
}