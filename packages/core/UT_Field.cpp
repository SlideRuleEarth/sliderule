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
#include "FieldColumn.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Field::LUA_META_NAME = "UT_Field";
const struct luaL_Reg UT_Field::LUA_META_TABLE[] = {
    {"element",     testElement},
    {"array",       testArray},
    {"column",      testColumn},
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

        struct hi {
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

        // test encodings
        ut_assert(lua_obj, bye.p0.getEncoding() == Field::BOOLEAN,  "failed to set encoding for bool: %d",      bye.p0.getEncoding());
        ut_assert(lua_obj, bye.p1.getEncoding() == Field::INT8,     "failed to set encoding for int8_t: %d",    bye.p1.getEncoding());
        ut_assert(lua_obj, bye.p2.getEncoding() == Field::INT16,    "failed to set encoding for int16_t: %d",   bye.p2.getEncoding());
        ut_assert(lua_obj, bye.p3.getEncoding() == Field::INT32,    "failed to set encoding for int32_t: %d",   bye.p3.getEncoding());
        ut_assert(lua_obj, bye.p4.getEncoding() == Field::INT64,    "failed to set encoding for int64_t: %d",   bye.p4.getEncoding());
        ut_assert(lua_obj, bye.p5.getEncoding() == Field::UINT8,    "failed to set encoding for uint8_t: %d",   bye.p5.getEncoding());
        ut_assert(lua_obj, bye.p6.getEncoding() == Field::UINT16,   "failed to set encoding for uint16_t: %d",  bye.p6.getEncoding());
        ut_assert(lua_obj, bye.p7.getEncoding() == Field::UINT32,   "failed to set encoding for uint32_t: %d",  bye.p7.getEncoding());
        ut_assert(lua_obj, bye.p8.getEncoding() == Field::UINT64,   "failed to set encoding for uint64_t: %d",  bye.p8.getEncoding());
        ut_assert(lua_obj, bye.p9.getEncoding() == Field::FLOAT,    "failed to set encoding for float: %d",     bye.p9.getEncoding());
        ut_assert(lua_obj, bye.p10.getEncoding() == Field::DOUBLE,  "failed to set encoding for double: %d",    bye.p10.getEncoding());
        ut_assert(lua_obj, bye.p11.getEncoding() == Field::STRING,  "failed to set encoding for string: %d",    bye.p11.getEncoding());

        // test toJson
        ut_assert(lua_obj, bye.p0.toJson() == "true",      "failed to convert to json: %s", bye.p0.toJson().c_str());
        ut_assert(lua_obj, bye.p1.toJson() == "10",        "failed to convert to json: %s", bye.p1.toJson().c_str());
        ut_assert(lua_obj, bye.p2.toJson() == "11",        "failed to convert to json: %s", bye.p2.toJson().c_str());
        ut_assert(lua_obj, bye.p3.toJson() == "12",        "failed to convert to json: %s", bye.p3.toJson().c_str());
        ut_assert(lua_obj, bye.p4.toJson() == "13",        "failed to convert to json: %s", bye.p4.toJson().c_str());
        ut_assert(lua_obj, bye.p5.toJson() == "14",        "failed to convert to json: %s", bye.p5.toJson().c_str());
        ut_assert(lua_obj, bye.p6.toJson() == "15",        "failed to convert to json: %s", bye.p6.toJson().c_str());
        ut_assert(lua_obj, bye.p7.toJson() == "16",        "failed to convert to json: %s", bye.p7.toJson().c_str());
        ut_assert(lua_obj, bye.p8.toJson() == "17",        "failed to convert to json: %s", bye.p8.toJson().c_str());
        ut_assert(lua_obj, bye.p9.toJson() == "2.300000",  "failed to convert to json: %s", bye.p9.toJson().c_str());
        ut_assert(lua_obj, bye.p10.toJson() == "3.140000", "failed to convert to json: %s", bye.p10.toJson().c_str());
        ut_assert(lua_obj, bye.p11.toJson() == "\"good\"", "failed to convert to json: %s", bye.p11.toJson().c_str());

        // test fromJson
        const string s0("false");     bye.p0.fromJson(s0);   ut_assert(lua_obj, bye.p0.value == false, "failed to convert from json: %s", bye.p0.toJson().c_str());
        const string s1("90");        bye.p1.fromJson(s1);   ut_assert(lua_obj, bye.p1.value == 90,    "failed to convert from json: %s", bye.p1.toJson().c_str());
        const string s2("91");        bye.p2.fromJson(s2);   ut_assert(lua_obj, bye.p2.value == 91,    "failed to convert from json: %s", bye.p2.toJson().c_str());
        const string s3("92");        bye.p3.fromJson(s3);   ut_assert(lua_obj, bye.p3.value == 92,    "failed to convert from json: %s", bye.p3.toJson().c_str());
        const string s4("93");        bye.p4.fromJson(s4);   ut_assert(lua_obj, bye.p4.value == 93,    "failed to convert from json: %s", bye.p4.toJson().c_str());
        const string s5("94");        bye.p5.fromJson(s5);   ut_assert(lua_obj, bye.p5.value == 94,    "failed to convert from json: %s", bye.p5.toJson().c_str());
        const string s6("95");        bye.p6.fromJson(s6);   ut_assert(lua_obj, bye.p6.value == 95,    "failed to convert from json: %s", bye.p6.toJson().c_str());
        const string s7("96");        bye.p7.fromJson(s7);   ut_assert(lua_obj, bye.p7.value == 96,    "failed to convert from json: %s", bye.p7.toJson().c_str());
        const string s8("97");        bye.p8.fromJson(s8);   ut_assert(lua_obj, bye.p8.value == 97,    "failed to convert from json: %s", bye.p8.toJson().c_str());
        const string s9("5.4");       bye.p9.fromJson(s9);   ut_assert(lua_obj, fabs(bye.p9.value - 5.4) < 0.01,   "failed to convert from json: %s", bye.p9.toJson().c_str());
        const string s10("8.3");      bye.p10.fromJson(s10); ut_assert(lua_obj, fabs(bye.p10.value - 8.3) < 0.01,  "failed to convert from json: %s", bye.p10.toJson().c_str());
        const string s11("\"bad\"");  bye.p11.fromJson(s11); ut_assert(lua_obj, bye.p11.value == "bad", "failed to convert from json: %s", bye.p11.toJson().c_str());

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

        struct hi {
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

        // test encodings
        ut_assert(lua_obj, bye.p0.getEncoding() == Field::BOOLEAN,  "failed to set encoding for bool: %d",      bye.p0.getEncoding());
        ut_assert(lua_obj, bye.p1.getEncoding() == Field::INT8,     "failed to set encoding for int8_t: %d",    bye.p1.getEncoding());
        ut_assert(lua_obj, bye.p2.getEncoding() == Field::INT16,    "failed to set encoding for int16_t: %d",   bye.p2.getEncoding());
        ut_assert(lua_obj, bye.p3.getEncoding() == Field::INT32,    "failed to set encoding for int32_t: %d",   bye.p3.getEncoding());
        ut_assert(lua_obj, bye.p4.getEncoding() == Field::INT64,    "failed to set encoding for int64_t: %d",   bye.p4.getEncoding());
        ut_assert(lua_obj, bye.p5.getEncoding() == Field::UINT8,    "failed to set encoding for uint8_t: %d",   bye.p5.getEncoding());
        ut_assert(lua_obj, bye.p6.getEncoding() == Field::UINT16,   "failed to set encoding for uint16_t: %d",  bye.p6.getEncoding());
        ut_assert(lua_obj, bye.p7.getEncoding() == Field::UINT32,   "failed to set encoding for uint32_t: %d",  bye.p7.getEncoding());
        ut_assert(lua_obj, bye.p8.getEncoding() == Field::UINT64,   "failed to set encoding for uint64_t: %d",  bye.p8.getEncoding());
        ut_assert(lua_obj, bye.p9.getEncoding() == Field::FLOAT,    "failed to set encoding for float: %d",     bye.p9.getEncoding());
        ut_assert(lua_obj, bye.p10.getEncoding() == Field::DOUBLE,  "failed to set encoding for double: %d",    bye.p10.getEncoding());
        ut_assert(lua_obj, bye.p11.getEncoding() == Field::STRING,  "failed to set encoding for string: %d",    bye.p11.getEncoding());

        // test toJson
        ut_assert(lua_obj, bye.p0.toJson() == "[true,false]",         "failed to convert to json: %s", bye.p0.toJson().c_str());
        ut_assert(lua_obj, bye.p1.toJson() == "[10,100]",             "failed to convert to json: %s", bye.p1.toJson().c_str());
        ut_assert(lua_obj, bye.p2.toJson() == "[11,110]",             "failed to convert to json: %s", bye.p2.toJson().c_str());
        ut_assert(lua_obj, bye.p3.toJson() == "[12,120]",             "failed to convert to json: %s", bye.p3.toJson().c_str());
        ut_assert(lua_obj, bye.p4.toJson() == "[13,130]",             "failed to convert to json: %s", bye.p4.toJson().c_str());
        ut_assert(lua_obj, bye.p5.toJson() == "[14,140]",             "failed to convert to json: %s", bye.p5.toJson().c_str());
        ut_assert(lua_obj, bye.p6.toJson() == "[15,150]",             "failed to convert to json: %s", bye.p6.toJson().c_str());
        ut_assert(lua_obj, bye.p7.toJson() == "[16,160]",             "failed to convert to json: %s", bye.p7.toJson().c_str());
        ut_assert(lua_obj, bye.p8.toJson() == "[17,170]",             "failed to convert to json: %s", bye.p8.toJson().c_str());
        ut_assert(lua_obj, bye.p9.toJson() == "[2.300000,4.300000]",  "failed to convert to json: %s", bye.p9.toJson().c_str());
        ut_assert(lua_obj, bye.p10.toJson() == "[3.140000,9.200000]", "failed to convert to json: %s", bye.p10.toJson().c_str());
        ut_assert(lua_obj, bye.p11.toJson() == "[\"good\",\"bad\"]",  "failed to convert to json: %lu", bye.p11.toJson().length());

        // test fromJson
        const string s0("[false, true]");        bye.p0.fromJson(s0);   ut_assert(lua_obj, bye.p0[1] == true, "failed to convert from json: %s", bye.p0.toJson().c_str());
        const string s1("[15, 90]");             bye.p1.fromJson(s1);   ut_assert(lua_obj, bye.p1[1] == 90,    "failed to convert from json: %s", bye.p1.toJson().c_str());
        const string s2("[15, 91]");             bye.p2.fromJson(s2);   ut_assert(lua_obj, bye.p2[1] == 91,    "failed to convert from json: %s", bye.p2.toJson().c_str());
        const string s3("[15, 92]");             bye.p3.fromJson(s3);   ut_assert(lua_obj, bye.p3[1] == 92,    "failed to convert from json: %s", bye.p3.toJson().c_str());
        const string s4("[15, 93]");             bye.p4.fromJson(s4);   ut_assert(lua_obj, bye.p4[1] == 93,    "failed to convert from json: %s", bye.p4.toJson().c_str());
        const string s5("[15, 94]");             bye.p5.fromJson(s5);   ut_assert(lua_obj, bye.p5[1] == 94,    "failed to convert from json: %s", bye.p5.toJson().c_str());
        const string s6("[15, 95]");             bye.p6.fromJson(s6);   ut_assert(lua_obj, bye.p6[1] == 95,    "failed to convert from json: %s", bye.p6.toJson().c_str());
        const string s7("[15, 96]");             bye.p7.fromJson(s7);   ut_assert(lua_obj, bye.p7[1] == 96,    "failed to convert from json: %s", bye.p7.toJson().c_str());
        const string s8("[15, 97]");             bye.p8.fromJson(s8);   ut_assert(lua_obj, bye.p8[1] == 97,    "failed to convert from json: %s", bye.p8.toJson().c_str());
        const string s9("[3.3, 5.4]");           bye.p9.fromJson(s9);   ut_assert(lua_obj, fabs(bye.p9[1] - 5.4) < 0.01,   "failed to convert from json: %s", bye.p9.toJson().c_str());
        const string s10("[1.1, 8.3]");          bye.p10.fromJson(s10); ut_assert(lua_obj, fabs(bye.p10[1] - 8.3) < 0.01,  "failed to convert from json: %s", bye.p10.toJson().c_str());
        const string s11("[\"bad\", \"good\"]"); bye.p11.fromJson(s11); ut_assert(lua_obj, bye.p11[1] == "good", "failed to convert from json: %s", bye.p11.toJson().c_str());

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

        // test encodings
        ut_assert(lua_obj, pbool.getEncoding() == Field::BOOLEAN,   "failed to set encoding for bool: %d",      pbool.getEncoding());
        ut_assert(lua_obj, pstring.getEncoding() == Field::STRING,  "failed to set encoding for string: %d",    pstring.getEncoding());
        ut_assert(lua_obj, pint.getEncoding() == Field::INT64,      "failed to set encoding for int64_t: %d",   pint.getEncoding());
        ut_assert(lua_obj, pdouble.getEncoding() == Field::DOUBLE,  "failed to set encoding for double: %d",    pdouble.getEncoding());

        // populate bool column
        pbool.append(true);
        pbool.append(true);
        pbool.append(false);

        // populate string column
        pstring.append("good");
        pstring.append("guys");
        pstring.append("always");
        pstring.append("win");

        // populate int column
        pint.append(1);
        pint.append(2);
        pint.append(3);
        pint.append(4);
        pint.append(5);
        
        // populate double column
        pdouble.append(1.1);
        pdouble.append(2.2);
        
        // test toJson
        ut_assert(lua_obj, pbool.toJson() == "[true,true,false]", "failed to convert to json: %s", pbool.toJson().c_str());
        ut_assert(lua_obj, pstring.toJson() == "[\"good\",\"guys\",\"always\",\"win\"]", "failed to convert to json: %s", pstring.toJson().c_str());
        ut_assert(lua_obj, pint.toJson() == "[1,2,3,4,5]", "failed to convert to json: %s", pint.toJson().c_str());
        ut_assert(lua_obj, pdouble.toJson() == "[1.100000,2.200000]", "failed to convert to json: %s", pdouble.toJson().c_str());

        // // test fromJson
        const string s0("[false, true]");        pbool.fromJson(s0);    ut_assert(lua_obj, pbool[1] == true, "failed to convert from json: %s", pbool.toJson().c_str());
        const string s11("[\"bad\", \"good\"]"); pstring.fromJson(s11); ut_assert(lua_obj, pstring[1] == "good", "failed to convert from json: %s", pstring.toJson().c_str());
        const string s1("[15, 90]");             pint.fromJson(s1);     ut_assert(lua_obj, pint[1] == 90,    "failed to convert from json: %s", pint.toJson().c_str());
        const string s9("[3.3, 5.4]");           pdouble.fromJson(s9);  ut_assert(lua_obj, fabs(pdouble[1] - 5.4) < 0.01,   "failed to convert from json: %s", pdouble.toJson().c_str());

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
