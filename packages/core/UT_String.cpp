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

#include "UT_String.h"
#include "UnitTest.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_String::LUA_META_NAME = "UT_String";
const struct luaL_Reg UT_String::LUA_META_TABLE[] = {
    {"replace",     testReplace},
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_String::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_String(L));
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
UT_String::UT_String (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*--------------------------------------------------------------------------------------
 * testReplace
 *--------------------------------------------------------------------------------------*/
int UT_String::testReplace(lua_State* L)
{
    UT_String* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_String*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    // 1) Replace Single Character
    char* test1 = StringLib::replace("Hello World", "o", "X");
    ut_assert(lua_obj, StringLib::match(test1, "HellX WXrld"), "Failed single character test: %s", test1);
    delete [] test1;
    // 2) Replace String
    char* test2 = StringLib::replace("Hello World", "ello", "eal");
    ut_assert(lua_obj, StringLib::match(test2, "Heal World"), "Failed to replace string: %s", test2);
    delete [] test2;

    // 3) Replace Strings
    const char* oldtxt[2] = { "$1", "$2" };
    const char* newtxt[2] = { "sentence", "not" };
    char* test3 = StringLib::replace("This is a long $1 and I am $2 sure if this $1 will work or $2", oldtxt, newtxt, 2);
    ut_assert(lua_obj, StringLib::match(test3, "This is a long sentence and I am not sure if this sentence will work or not"), "Failed multiple replacements: %s", test3);
    delete [] test3;

    // return success or failure
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}
