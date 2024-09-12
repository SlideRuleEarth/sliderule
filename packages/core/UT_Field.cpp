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
#include <cmath>

#include "UT_Field.h"
#include "UnitTest.h"
#include "OsApi.h"
#include "Field.h"
#include "FieldElement.h"
#include "FieldArray.h"
#include "FieldEnumeration.h"
#include "FieldList.h"
#include "FieldColumn.h"
#include "FieldDictionary.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Field::LUA_META_NAME = "UT_Field";
const struct luaL_Reg UT_Field::LUA_META_TABLE[] = {
    {"element",     testElement},
    {"array",       testArray},
    {"enumeration", testEnumeration},
    {"list",        testList},
    {"column",      testColumn},
    {"dictionary",  testDictionary},
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

 /*--------------------------------------------------------------------------------------
  * testElement
  *--------------------------------------------------------------------------------------*/
 int UT_Field::testElement(lua_State* L)
 {
     UT_Field* lua_obj = NULL;
     try
     {
         // initialize test
         lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
         ut_initialize(lua_obj);

        const struct hi {
            FieldElement<bool>      p0{true};
            FieldElement<int8_t>    p1{10};
            FieldElement<int16_t>   p2{11};
            FieldElement<int32_t>   p3{12};
            FieldElement<int64_t>   p4{13};
            FieldElement<uint8_t>   p5{14};
            FieldElement<uint16_t>  p6{15};
            FieldElement<uint32_t>  p7{16};
            FieldElement<uint64_t>  p8{17};
            FieldElement<float>     p9{2.3};
            FieldElement<double>    p10{3.14};
            FieldElement<string>    p11{"good"};
        } bye;

        ut_assert(lua_obj, bye.p0 == true, "failed to initialize parameter");

        // return status
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

/*--------------------------------------------------------------------------------------
 * testArray
 *--------------------------------------------------------------------------------------*/
int UT_Field::testArray(lua_State* L)
{
    UT_Field* lua_obj = NULL;
    try
    {
        // initialize test
        lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
        ut_initialize(lua_obj);

        const struct hi {
            FieldArray<bool,2>      p0 = {true, false};
            FieldArray<int8_t,2>    p1 = {10, 100};
            FieldArray<int16_t,2>   p2 = {11, 110};
            FieldArray<int32_t,2>   p3 = {12, 120};
            FieldArray<int64_t,2>   p4 = {13, 130};
            FieldArray<uint8_t,2>   p5 = {14, 140};
            FieldArray<uint16_t,2>  p6 = {15, 150};
            FieldArray<uint32_t,2>  p7 = {16, 160};
            FieldArray<uint64_t,2>  p8 = {17, 170};
            FieldArray<float,2>     p9 = {2.3, 4.3};
            FieldArray<double,2>    p10 = {3.14, 9.2};
            FieldArray<string,2>    p11 = {"good", "bad"};
        } bye;

        ut_assert(lua_obj, bye.p0[0] == true, "failed to initialize parameter");

        // return status
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

/*--------------------------------------------------------------------------------------
 * testEnumeration
 *--------------------------------------------------------------------------------------*/
typedef enum {
    UT_FIELD_ENUM_0 = 0,
    UT_FIELD_ENUM_1 = 10,
    UT_FIELD_ENUM_2 = 20,
    NUM_UT_FIELD_ENUMS = 3
} ut_field_enum_t;

int convertToLua(lua_State* L, const ut_field_enum_t& v) {
    switch(v) {
        case UT_FIELD_ENUM_0:   lua_pushstring(L, "enum0");  break;
        case UT_FIELD_ENUM_1:   lua_pushstring(L, "enum1");  break;
        case UT_FIELD_ENUM_2:   lua_pushstring(L, "enum2");  break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid enumeration: %d", static_cast<int>(v));
    }
    return 1;
}

void convertFromLua(lua_State* L, int index, ut_field_enum_t& v) {
    if(lua_isstring(L, index)) {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "enum0")) v = UT_FIELD_ENUM_0;
        else if(StringLib::match(str, "enum1")) v = UT_FIELD_ENUM_1;
        else if(StringLib::match(str, "enum2")) v = UT_FIELD_ENUM_2;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "ground track is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index)) {
        throw RunTimeException(CRITICAL, RTE_ERROR, "ground track is an invalid type: %d", lua_type(L, index));
    }
}

int convertToIndex(const ut_field_enum_t& v) {
    return static_cast<int>(v) / 10;
}

void convertFromIndex(int index, ut_field_enum_t& v) {
    v = static_cast<ut_field_enum_t>(index * 10);
}

int UT_Field::testEnumeration(lua_State* L)
{
    UT_Field* lua_obj = NULL;
    try
    {
        // initialize test
        lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
        ut_initialize(lua_obj);

        FieldEnumeration<ut_field_enum_t,NUM_UT_FIELD_ENUMS> e = {true, false, true};

        ut_assert(lua_obj, e.values[0] == true, "failed to initialize parameter");
        ut_assert(lua_obj, e.values[1] == false, "failed to initialize parameter");
        ut_assert(lua_obj, e.values[2] == true, "failed to initialize parameter");

        ut_assert(lua_obj, e[UT_FIELD_ENUM_0] == true, "failed to initialize parameter");
        ut_assert(lua_obj, e[UT_FIELD_ENUM_1] == false, "failed to initialize parameter");
        ut_assert(lua_obj, e[UT_FIELD_ENUM_0] == true, "failed to initialize parameter");

        // return status
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

/*--------------------------------------------------------------------------------------
 * testList
 *--------------------------------------------------------------------------------------*/
int UT_Field::testList(lua_State* L)
{
    UT_Field* lua_obj = NULL;
    try
    {
        // initialize test
        lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
        ut_initialize(lua_obj);

        FieldList<string>   pstring;

        // populate string list
        ut_assert(lua_obj, pstring.append("good") == 1, "failed to append");
        ut_assert(lua_obj, pstring.append("guys") == 2, "failed to append");
        ut_assert(lua_obj, pstring.append("always") == 3, "failed to append");
        ut_assert(lua_obj, pstring.append("win") == 4, "failed to append");

        // check list
        ut_assert(lua_obj, pstring.length() == 4, "failed to return size of list");

        // return status
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

/*--------------------------------------------------------------------------------------
 * testColumn
 *--------------------------------------------------------------------------------------*/
int UT_Field::testColumn(lua_State* L)
{
    UT_Field* lua_obj = NULL;
    try
    {
        // initialize test
        lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
        ut_initialize(lua_obj);

        FieldColumn<bool>      pbool;
        FieldColumn<string>    pstring;
        FieldColumn<int64_t>   pint;
        FieldColumn<double>    pdouble;

        // populate bool column
        ut_assert(lua_obj, pbool.append(true) == 1, "failed to append");
        ut_assert(lua_obj, pbool.append(true) == 2, "failed to append");
        ut_assert(lua_obj, pbool.append(false) == 3, "failed to append");

        // populate string column
        ut_assert(lua_obj, pstring.append("good") == 1, "failed to append");
        ut_assert(lua_obj, pstring.append("guys") == 2, "failed to append");
        ut_assert(lua_obj, pstring.append("always") == 3, "failed to append");
        ut_assert(lua_obj, pstring.append("win") == 4, "failed to append");

        // populate int column
        ut_assert(lua_obj, pint.append(1) == 1, "failed to append");
        ut_assert(lua_obj, pint.append(2) == 2, "failed to append");
        ut_assert(lua_obj, pint.append(3) == 3, "failed to append");
        ut_assert(lua_obj, pint.append(4) == 4, "failed to append");
        ut_assert(lua_obj, pint.append(5) == 5, "failed to append");
        
        // populate double column
        ut_assert(lua_obj, pdouble.append(1.1) == 1, "failed to append");
        ut_assert(lua_obj, pdouble.append(2.2) == 2, "failed to append");
        
        // return status
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

/*--------------------------------------------------------------------------------------
 * testDictionary
 *--------------------------------------------------------------------------------------*/
int UT_Field::testDictionary(lua_State* L)
{
    UT_Field* lua_obj = NULL;
    try
    {
        // initialize test
        lua_obj = dynamic_cast<UT_Field*>(getLuaSelf(L, 1));        
        ut_initialize(lua_obj);

        struct parms: public FieldDictionary
        {
            FieldElement<bool>              e{true};
            FieldArray<bool,2>              a = {true, false};
            FieldColumn<bool>               c;
            FieldColumn<FieldColumn<bool>>  cc;

            parms(): FieldDictionary({
                {"e", &e},
                {"a", &a},
                {"c", &c},
                {"cc", &cc}
            }) {}
        };

        struct parms bye;

        // populate columns
        bye.c.append(true);
        bye.c.append(false);
        bye.c.append(true);

        FieldColumn<bool> cc1;
        cc1.append(true);
        cc1.append(true);
        bye.cc.append(cc1);
        FieldColumn<bool> cc2;
        cc2.append(true);
        cc2.append(true);
        bye.cc.append(cc2);

        // return status
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
