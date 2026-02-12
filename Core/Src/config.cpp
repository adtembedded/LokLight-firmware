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
#include "config.h"

LoklightConfigInitResult_t LoklightConfig::init(void)
{
    isInitialized_ = false;

    //TODO
    
    isInitialized_ = true;
    return LL_CFG_INIT_NO_STORED_CFG_DEFAULTS_LOADED;
}