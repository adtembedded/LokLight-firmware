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

// return type of init
typedef enum {
    LOKLIGHT_INIT_OK = 0,
    LOKLIGHT_INIT_ERROR = -1
} LoklightInitResult_t;

class LokLight
{
public:
    LokLight();
    ~LokLight();

    // Prevent copying and moving. This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    LokLight(const LokLight&) = delete;
    LokLight(LokLight&&) = delete;
    LokLight& operator=(const LokLight&) = delete;
    LokLight& operator=(LokLight&&) = delete;

    LoklightInitResult_t init(uint32_t a);

private:
    uint32_t dummyVal_; // Placeholder member variable
};

#endif // LOKLIGHT_HPP