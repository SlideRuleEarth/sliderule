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

        // test encodings
        const struct hi {
            FieldElement<int8_t>        p0{10};
            FieldElement<int16_t>       p1{11};
            FieldElement<int32_t>       p2{12};
            FieldElement<int64_t>       p3{13};
            FieldElement<uint8_t>       p4{14};
            FieldElement<uint16_t>      p5{15};
            FieldElement<uint32_t>      p6{16};
            FieldElement<uint64_t>      p7{17};
            FieldElement<float>         p8{2.3};
            FieldElement<double>        p9{3.14};
            FieldElement<const char*>   p10{"good"};
        } bye;
        ut_assert(lua_obj, bye.p0.getEncoding() == Field::INT8, "failed to set encoding for int8_t: %d", bye.p0.getEncoding());
        ut_assert(lua_obj, bye.p1.getEncoding() == Field::INT16, "failed to set encoding for int16_t: %d", bye.p1.getEncoding());
        ut_assert(lua_obj, bye.p2.getEncoding() == Field::INT32, "failed to set encoding for int32_t: %d", bye.p2.getEncoding());
        ut_assert(lua_obj, bye.p3.getEncoding() == Field::INT64, "failed to set encoding for int64_t: %d", bye.p3.getEncoding());
        ut_assert(lua_obj, bye.p4.getEncoding() == Field::UINT8, "failed to set encoding for uint8_t: %d", bye.p4.getEncoding());
        ut_assert(lua_obj, bye.p5.getEncoding() == Field::UINT16, "failed to set encoding for uint16_t: %d", bye.p5.getEncoding());
        ut_assert(lua_obj, bye.p6.getEncoding() == Field::UINT32, "failed to set encoding for uint32_t: %d", bye.p6.getEncoding());
        ut_assert(lua_obj, bye.p7.getEncoding() == Field::UINT64, "failed to set encoding for uint64_t: %d", bye.p7.getEncoding());
        ut_assert(lua_obj, bye.p8.getEncoding() == Field::FLOAT, "failed to set encoding for float: %d", bye.p8.getEncoding());
        ut_assert(lua_obj, bye.p9.getEncoding() == Field::DOUBLE, "failed to set encoding for double: %d", bye.p9.getEncoding());
        ut_assert(lua_obj, bye.p10.getEncoding() == Field::STRING, "failed to set encoding for char: %d", bye.p10.getEncoding());

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
