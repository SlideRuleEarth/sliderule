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
#include "CreFields.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CreFields::OBJECT_TYPE = "CreFields";
const char* CreFields::LUA_META_NAME = "CreFields";
const struct luaL_Reg CreFields::LUA_META_TABLE[] = {
    {"image",       luaImage},
    {"export",      luaExport},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int CreFields::luaCreate (lua_State* L)
{
    CreFields* cre_fields = NULL;
    try
    {
        cre_fields = new CreFields(L);
        cre_fields->fromLua(L, 1);
        return createLuaObject(L, cre_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        delete cre_fields;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport - export() --> lua table
 *----------------------------------------------------------------------------*/
int CreFields::luaExport (lua_State* L)
{
    int num_rets = 1;

    try
    {
        CreFields* lua_obj = dynamic_cast<CreFields*>(getLuaSelf(L, 1));
        num_rets = lua_obj->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return num_rets;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void CreFields::fromLua (lua_State* L, int index)
{
    FieldDictionary::fromLua(L, index);

    // check image for ONLY legal characters
    for (auto c_iter = image.value.begin(); c_iter < image.value.end(); ++c_iter)
    {
        const int c = *c_iter;
        if(!isalnum(c) && (c != '/') && (c != '.') && (c != ':') && (c != '-'))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid character found in image name: %c", c);
        }
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CreFields::CreFields (lua_State* L):
    LuaObject           (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary ({
        {"image",   &image},
        {"name",    &name},
        {"command", &command},
        {"timeout", &timeout},
    })
{
}

/*----------------------------------------------------------------------------
 * luaImage
 *----------------------------------------------------------------------------*/
int CreFields::luaImage (lua_State* L)
{
    try
    {
        const CreFields* lua_obj = dynamic_cast<CreFields*>(getLuaSelf(L, 1));
        if(!lua_obj->image.value.empty()) lua_pushstring(L, lua_obj->image.value.c_str());
        else lua_pushnil(L);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}
