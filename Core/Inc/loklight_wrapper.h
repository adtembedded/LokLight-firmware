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

#ifndef LOKLIGHT_WRAPPER_H
#define LOKLIGHT_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif


// Opaque handle type for C
typedef void* LokLightHandle;

// Create and destroy
LokLightHandle loklight_create(void);

void loklight_destroy(LokLightHandle handle);

// forward definition of return type of init. As enums can be forward declared, the result is stored in a struct with 1 member.
// This avoid multiple definitions of the same enum.
typedef struct LLWrapInitResult_s LLWrapInitResult_t;
// Initialization, call after creation and HW init
LLWrapInitResult_t loklight_init(LokLightHandle handle, uint32_t a);

#ifdef __cplusplus
}
#endif

#endif // LOKLIGHT_WRAPPER_H