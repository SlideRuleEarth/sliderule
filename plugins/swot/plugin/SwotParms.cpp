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
#include "SwotParms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SwotParms::VARIABLES = "variables";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int SwotParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "SWOT parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new SwotParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * deltatime2timestamp - returns nanoseconds since Unix epoch, no leap seconds
 *----------------------------------------------------------------------------*/
int64_t SwotParms::deltatime2timestamp (double delta_time)
{
    return TimeLib::gps2systimeex(delta_time + (double)SWOT_SDP_EPOCH_GPS);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SwotParms::SwotParms(lua_State* L, int index):
    NetsvcParms(L, index)
{
    bool provided = false;

    try
    {
        /* Variables */
        lua_getfield(L, index, SwotParms::VARIABLES);
        get_lua_string_list(L, -1, variables, &provided);
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
SwotParms::~SwotParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void SwotParms::cleanup (void)
{
}

/*----------------------------------------------------------------------------
 * get_lua_string_list
 *----------------------------------------------------------------------------*/
void SwotParms::get_lua_string_list (lua_State* L, int index, string_list_t& string_list, bool* provided)
{
    /* Reset provided */
    if(provided) *provided = false;

    /* Must be table of strings */
    if(lua_istable(L, index))
    {
        /* Get number of item in table */
        int num_strings = lua_rawlen(L, index);
        if(num_strings > 0 && provided)
        {
            *provided = true;
        }

        /* Iterate through each item in table */
        for(int i = 0; i < num_strings; i++)
        {
            /* Get item */
            lua_rawgeti(L, index, i+1);

            if(lua_isstring(L, -1))
            {
                const char* item_str = LuaObject::getLuaString(L, -1);
                SafeString item("%s", item_str);
                string_list.add(item);
                mlog(DEBUG, "Adding %s to list of strings", item_str);
            }
            else
            {
                mlog(ERROR, "Invalid item specified - must be a string");
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "Lists must be provided as a table");
    }
}
