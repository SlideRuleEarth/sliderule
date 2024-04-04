/*
 * Copyright (c) 2023, University of Texas
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
 * 3. Neither the name of the University of Texas nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "core.h"
#include "geo.h"
#include "BathyParms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyParms::PH_IN_EXTENT = "ph_in_extent";
const char* BathyParms::MAX_ALONG_TRACK_SPREAD = "max_along_track_spread";
const char* BathyParms::BEAM_FILE_PREFIX = "beam_file_prefix";

const double BathyParms::DEFAULT_MAX_ALONG_TRACK_SPREAD = 10000.0;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int BathyParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Requests parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new BathyParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyParms::BathyParms(lua_State* L, int index):
    Icesat2Parms (L, index),
    ph_in_extent (DEFAULT_PH_IN_EXTENT),
    max_along_track_spread (DEFAULT_MAX_ALONG_TRACK_SPREAD),
    beam_file_prefix(NULL)
{
    bool provided = false;

    try
    {
        /* photons in extent */
        lua_getfield(L, index, BathyParms::PH_IN_EXTENT);
        ph_in_extent = LuaObject::getLuaInteger(L, -1, true, ph_in_extent, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::PH_IN_EXTENT, ph_in_extent);
        lua_pop(L, 1);

        /* maximum along track spread */
        lua_getfield(L, index, BathyParms::MAX_ALONG_TRACK_SPREAD);
        max_along_track_spread = LuaObject::getLuaFloat(L, -1, true, max_along_track_spread, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::MAX_ALONG_TRACK_SPREAD, max_along_track_spread);
        lua_pop(L, 1);

        /* beam file prefix */
        lua_getfield(L, index, BathyParms::BEAM_FILE_PREFIX);
        beam_file_prefix = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, beam_file_prefix, &provided));
        if(provided) mlog(DEBUG, "Setting %s to %s", BathyParms::BEAM_FILE_PREFIX, beam_file_prefix);
        lua_pop(L, 1);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw; // rethrow exception
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyParms::~BathyParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void BathyParms::cleanup (void) const
{
    delete [] beam_file_prefix;
}
