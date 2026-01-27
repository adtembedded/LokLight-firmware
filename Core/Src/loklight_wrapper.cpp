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

struct LoklightWrapper_s
{
    LokLight ll;                    // Actual C++ LokLight instance
    LoklightWrapper_s() : ll() {}   // Initializer list adds loklight object to the wrapper
};

// Create and destroy
LoklightHandle loklight_create(void)
{
    return reinterpret_cast<LoklightHandle>(new LoklightWrapper_s()); //Need unsafe conversion to allow C code to use C++ object
}

void loklight_destroy(LoklightHandle handle)
{
    delete reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
}

// Initialization, call after creation and HW init
LoklightInitResult_t loklight_init(LoklightHandle handle, uint32_t a)
{
    LoklightWrapper_s* wrapper = reinterpret_cast<LoklightWrapper_s*>(handle); //Need unsafe conversion to allow C code to use C++ object
    return wrapper->ll.init(a);
}
