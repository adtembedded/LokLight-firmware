/*
* LokLight
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
#include "loklight.hpp"

struct LokLightWrapper_s
{
    LokLight ll;                    // Actual C++ LokLight instance
    LokLightWrapper_s() : ll() {}   // Initializer list adds loklight object to the wrapper
};

typedef struct LLWrapInitResult_s{
    LoklightInitResult_t result_code;   // Can now be fully defined as loklight.hpp is included
}LLWrapInitResult_t;

// Create and destroy
extern "C" LokLightHandle loklight_create(void)
{
    return reinterpret_cast<LokLightHandle>(new LokLightWrapper_s()); //Need unsafe conversion to allow C code to use C++ object
}

extern "C" void loklight_destroy(LokLightHandle handle)
{
    delete reinterpret_cast<LokLightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
}

// Initialization, call after creation and HW init
extern "C" LLWrapInitResult_t loklight_init(LokLightHandle handle, uint32_t a)
{
    LokLightWrapper_s* wrapper = reinterpret_cast<LokLightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
    LLWrapInitResult_t result;
    result.result_code = wrapper->ll.init(a);
    return result;
}
