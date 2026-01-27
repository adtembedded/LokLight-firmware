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

#ifndef LOKLIGHT_HPP
#define LOKLIGHT_HPP

#include <cstdint>  // For standard integer types
#include "loklight_wrapper.h"   // For types
#include "config.h"
#include "dcc.h"
#include "led_control.h"       

// return type of init
typedef enum {
    LOKLIGHT_INIT_OK = 0,
    LOKLIGHT_INIT_ERROR = -1
} LoklightInitResult_t;

class Loklight
{
public:
    Loklight();
    ~Loklight();

    // Prevent copying and moving. This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    Loklight(const Loklight&) = delete;
    Loklight(Loklight&&) = delete;
    Loklight& operator=(const Loklight&) = delete;
    Loklight& operator=(Loklight&&) = delete;

    LoklightInitResult_t init(LedControlInitCfg_t* ledInitCfg = nullptr);
    bool step();

private:
    LedControl ledControl_; // LED control instance
};

#endif // LOKLIGHT_HPP