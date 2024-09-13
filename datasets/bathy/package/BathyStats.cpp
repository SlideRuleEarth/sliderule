/*
 * Copyright (c) 2021, University of Washington
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University of Washington nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "FieldDictionary.h"
#include "FieldElement.h"
#include "BathyStats.h"

/******************************************************************************
 * CLASS DATA
 ******************************************************************************/

const char* BathyStats::OBJECT_TYPE = "BathyStats";
const char* BathyStats::LUA_META_NAME = "BathyStats";
const struct luaL_Reg BathyStats::LUA_META_TABLE[] = {
    {NULL, NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyStats::BathyStats(lua_State* L): 
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary({   {"valid",                   &valid},
                        {"photon_count",            &photonCount},
                        {"subaqueous_photons",      &subaqueousPhotons},
                        {"corrections_duration",    &correctionsDuration},
                        {"qtrees_duration",         &qtreesDuration},
                        {"coastnet_duration",       &coastnetDuration},
                        {"openoceanspp_duration",   &openoceansppDuration}  })
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
void BathyStats::update (const BathyStats& stats) 
{
    statsLock.lock();
    {
        valid.value                 = valid.value && stats.valid.value;
        photonCount.value           += stats.photonCount.value;
        subaqueousPhotons.value     += stats.subaqueousPhotons.value;
        correctionsDuration.value   += stats.correctionsDuration.value;
        qtreesDuration.value        += stats.qtreesDuration.value;
        coastnetDuration.value      += stats.coastnetDuration.value;
        openoceansppDuration.value  += stats.openoceansppDuration.value;
    }
    statsLock.unlock();
}
