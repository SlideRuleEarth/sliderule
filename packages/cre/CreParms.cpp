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

#include "core.h"
#include "CreParms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CreParms::SELF                = "output";
const char* CreParms::IMAGE               = "image";
const char* CreParms::COMMAND             = "command";
const char* CreParms::TIMEOUT             = "timeout";

const char* CreParms::OBJECT_TYPE = "CreParms";
const char* CreParms::LUA_META_NAME = "CreParms";
const struct luaL_Reg CreParms::LUA_META_TABLE[] = {
    {"image",       luaImage},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int CreParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Cre parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new CreParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CreParms::CreParms (lua_State* L, int index):
    LuaObject           (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    image               (NULL),
    command             (NULL),
    timeout             (DEFAULT_TIMEOUT)
{
    /* Populate Object from Lua */
    try
    {
        /* Must be a Table */
        if(lua_istable(L, index))
        {
            bool field_provided = false;

            /* Image */
            lua_getfield(L, index, IMAGE);
            image = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, image, &field_provided));
            if(field_provided)
            {
                mlog(DEBUG, "Setting %s to %s", IMAGE, image);

                /* Check Image for ONLY Legal Characters */
                string s(image);
                for (auto c_iter = s.begin(); c_iter < s.end(); ++c_iter)
                {
                    int c = *c_iter;
                    if(!isalnum(c) && (c != '/') && (c != '.') && (c != ':') && (c != '-'))
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid character found in image name: %c", c);
                    }
                }
            }
            lua_pop(L, 1);

            /* Script */
            lua_getfield(L, index, COMMAND);
            command = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, command, &field_provided));
            if(field_provided) mlog(DEBUG, "Setting %s to %s", COMMAND, COMMAND);
            lua_pop(L, 1);

            /* Timeout */
            lua_getfield(L, index, TIMEOUT);
            timeout = LuaObject::getLuaInteger(L, -1, true, timeout, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", TIMEOUT, timeout);
            lua_pop(L, 1);
        }
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CreParms::~CreParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void CreParms::cleanup (void)
{
    if(image)
    {
        delete [] image;
        image = NULL;
    }

    if(command)
    {
        delete [] command;
        command = NULL;
    }
}

/*----------------------------------------------------------------------------
 * luaImage
 *----------------------------------------------------------------------------*/
int CreParms::luaImage (lua_State* L)
{
    try
    {
        CreParms* lua_obj = dynamic_cast<CreParms*>(getLuaSelf(L, 1));
        if(lua_obj->image) lua_pushstring(L, lua_obj->image);
        else lua_pushnil(L);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}
