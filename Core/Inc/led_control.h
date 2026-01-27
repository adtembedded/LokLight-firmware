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

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "loklight_wrapper.h"

// class led
class LedControl
{
public:
    LedControl(){;}
    ~LedControl(){;}

    // Prevent copying and moving. This object interfaces with C-code, it should exist only once and be managed strictly per instance.
    LedControl(const LedControl&) = delete;
    LedControl(LedControl&&) = delete;
    LedControl& operator=(const LedControl&) = delete;
    LedControl& operator=(LedControl&&) = delete;

    bool init(LedControlInitCfg_t* initCfg = nullptr);
    bool isInitialized() const { return isInitialized_; }
    void setBrightness(LedNumber_t ledNumber, uint8_t brightness);
    uint8_t getBrightness(LedNumber_t ledNumber) const;

private:
    uint8_t ledBrightness_[LED_COUNT] = {};
    bool isInitialized_ = false;
};

#endif