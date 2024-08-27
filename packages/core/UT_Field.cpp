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

#include <stdlib.h>
#include "UT_Field.h"
#include "UnitTest.h"
#include "OsApi.h"
#include "Field.h"
#include "FieldElement.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Field::LUA_META_NAME = "UT_Field";
const struct luaL_Reg UT_Field::LUA_META_TABLE[] = {
    {"basic",       testBasic},
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_Field::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_Field(L));
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
UT_Field::UT_Field (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_Field::~UT_Field(void) = default;

/*--------------------------------------------------------------------------------------
 * testBasic
 *--------------------------------------------------------------------------------------*/
int UT_Field::testBasic(lua_State* L)
{
    UT_Field* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
        ut_initialize(lua_obj);


        FieldElement<double> val1 = 5.3;
        mlog(INFO, "Val1 = %lf", val1.value);

        lua_pushboolean(L, ut_status(lua_obj));
        return 1;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }
}
