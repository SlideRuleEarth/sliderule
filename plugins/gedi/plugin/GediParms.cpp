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
 * INCLUDE
 ******************************************************************************/

#include "core.h"
#include "geo.h"
#include "GediParms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GediParms::BEAM             = "beam";
const char* GediParms::DEGRADE_FLAG     = "degrade_flag";
const char* GediParms::L2_QUALITY_FLAG  = "l2_quality_flag";
const char* GediParms::L4_QUALITY_FLAG  = "l4_quality_flag";
const char* GediParms::SURFACE_FLAG     = "surface_flag";

const uint8_t GediParms::BEAM_NUMBER[NUM_BEAMS] = {0, 1, 2, 3, 5, 6, 8, 11};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int GediParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Gedi parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new GediParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * beam2group
 *----------------------------------------------------------------------------*/
const char* GediParms::beam2group (int beam)
{
    switch((beam_t)beam)
    {
        case BEAM0000:  return "BEAM0000";
        case BEAM0001:  return "BEAM0001";
        case BEAM0010:  return "BEAM0010";
        case BEAM0011:  return "BEAM0011";
        case BEAM0101:  return "BEAM0101";
        case BEAM0110:  return "BEAM0110";
        case BEAM1000:  return "BEAM1000";
        case BEAM1011:  return "BEAM1011";
        default:        return "UNKNOWN";
    }
}

/*----------------------------------------------------------------------------
 * group2beam
 *----------------------------------------------------------------------------*/
int GediParms::group2beam (const char* group)
{
    if      (StringLib::match(group, "BEAM0000"))   return BEAM0000;
    else if (StringLib::match(group, "BEAM0001"))   return BEAM0001;
    else if (StringLib::match(group, "BEAM0010"))   return BEAM0010;
    else if (StringLib::match(group, "BEAM0011"))   return BEAM0011;
    else if (StringLib::match(group, "BEAM0101"))   return BEAM0101;
    else if (StringLib::match(group, "BEAM0110"))   return BEAM0110;
    else if (StringLib::match(group, "BEAM1000"))   return BEAM1000;
    else if (StringLib::match(group, "BEAM1011"))   return BEAM1011;
    else                                            return UNKNOWN_BEAM;
}

/*----------------------------------------------------------------------------
 * beam2index
 *----------------------------------------------------------------------------*/
int GediParms::beam2index (int beam)
{
    switch((beam_t)beam)
    {
        case BEAM0000:  return 0;
        case BEAM0001:  return 1;
        case BEAM0010:  return 2;
        case BEAM0011:  return 3;
        case BEAM0101:  return 4;
        case BEAM0110:  return 5;
        case BEAM1000:  return 6;
        case BEAM1011:  return 7;
        default:        return -1;
    }
}

/*----------------------------------------------------------------------------
 * index2group
 *----------------------------------------------------------------------------*/
const char* GediParms::index2group (int index)
{
    switch(index)
    {
        case 0:     return "BEAM0000";
        case 1:     return "BEAM0001";
        case 2:     return "BEAM0010";
        case 3:     return "BEAM0011";
        case 4:     return "BEAM0101";
        case 5:     return "BEAM0110";
        case 6:     return "BEAM1000";
        case 7:     return "BEAM1011";
        default:    return "UNKNOWN";
    }
}

/*----------------------------------------------------------------------------
 * deltatime2timestamp - returns nanoseconds since Unix epoch, no leap seconds
 *----------------------------------------------------------------------------*/
int64_t GediParms::deltatime2timestamp (double delta_time)
{
    return TimeLib::gps2systimeex(delta_time + (double)GEDI_SDP_EPOCH_GPS);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GediParms::GediParms(lua_State* L, int index):
    NetsvcParms                 (L, index),
    beams                       {true, true, true, true, true, true, true, true},
    degrade_filter              (DEGRADE_UNFILTERED),
    l2_quality_filter           (L2QLTY_UNFILTERED),
    l4_quality_filter           (L4QLTY_UNFILTERED),
    surface_filter              (SURFACE_UNFILTERED)
{
    bool provided = false;

    try
    {
        /* Beams */
        lua_getfield(L, index, GediParms::BEAM);
        get_lua_beams(L, -1, &provided);
        lua_pop(L, 1);

        /* Degrade Flag */
        lua_getfield(L, index, GediParms::DEGRADE_FLAG);
        degrade_filter = (degrade_t)LuaObject::getLuaInteger(L, -1, true, degrade_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::DEGRADE_FLAG, degrade_filter);
        lua_pop(L, 1);

        /* L2 Quality Flag */
        lua_getfield(L, index, GediParms::L2_QUALITY_FLAG);
        l2_quality_filter = (l2_quality_t)LuaObject::getLuaInteger(L, -1, true, l2_quality_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::L2_QUALITY_FLAG, l2_quality_filter);
        lua_pop(L, 1);

        /* L4 Quality Flag */
        lua_getfield(L, index, GediParms::L4_QUALITY_FLAG);
        l4_quality_filter = (l4_quality_t)LuaObject::getLuaInteger(L, -1, true, l4_quality_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::L4_QUALITY_FLAG, l4_quality_filter);
        lua_pop(L, 1);

        /* Surface Flag */
        lua_getfield(L, index, GediParms::SURFACE_FLAG);
        surface_filter = (surface_t)LuaObject::getLuaInteger(L, -1, true, surface_filter, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", GediParms::SURFACE_FLAG, surface_filter);
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
GediParms::~GediParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void GediParms::cleanup (void)
{
}

/*----------------------------------------------------------------------------
 * set_beam
 *----------------------------------------------------------------------------*/
bool GediParms::set_beam(int beam)
{
    int index = beam2index(beam);
    if(index >= 0)
    {
        beams[index] = true;
        mlog(DEBUG, "Selecting beam %d", beam);
        return true;
    }
    else
    {
        mlog(ERROR, "Invalid beam: %d", beam);
        return false;
    }
}

/*----------------------------------------------------------------------------
 * get_lua_beams
 *----------------------------------------------------------------------------*/
void GediParms::get_lua_beams (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of beams or a single beam */
    if(lua_istable(L, index))
    {
        /* Clear beam table (sets all to false) */
        memset(beams, 0, sizeof(beams));

        /* Get number of entries in table */
        int num_entries = lua_rawlen(L, index);
        if(num_entries > 0 && provided) *provided = true;

        /* Iterate through each entry in table */
        for(int i = 0; i < num_entries; i++)
        {
            /* Get entry */
            lua_rawgeti(L, index, i+1);

            /* Set beam */
            if(lua_isinteger(L, -1))
            {
                int beam = LuaObject::getLuaInteger(L, -1);
                set_beam(beam);
            }
            else if(lua_isstring(L, -1))
            {
                const char* group = LuaObject::getLuaString(L, -1);
                int beam = group2beam(group);
                set_beam(beam);
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(lua_isinteger(L, index))
    {
        /* Clear beam table (sets all to false) */
        memset(beams, 0, sizeof(beams));

        /* Set Beam */
        int beam = LuaObject::getLuaInteger(L, -1);
        if(beam == ALL_BEAMS) // special case
        {
            for(int i = 0; i < NUM_BEAMS; i++)
            {
                beams[i] = true;
            }
            mlog(DEBUG, "Selecting all beams");
        }
        else
        {
            set_beam(beam);
        }
    }
    else if(lua_isstring(L, index))
    {
        /* Clear beam table (sets all to false) */
        memset(beams, 0, sizeof(beams));

        /* Set Beam */
        const char* group = LuaObject::getLuaString(L, -1);
        int beam = group2beam(group);
        set_beam(beam);
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "Beams must be provided as a table or single integer or string");
    }
}
