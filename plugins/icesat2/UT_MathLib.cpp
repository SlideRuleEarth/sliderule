/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "UT_MathLib.h"
#include "MathLib.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_MathLib::OBJECT_TYPE = "UT_MathLib";

const char* UT_MathLib::LuaMetaName = "UT_MathLib";
const struct luaL_Reg UT_MathLib::LuaMetaTable[] = {
    {"lsftest",     luaLsfTest},
    {"sorttest",    luaSortTest},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :ut_mathlib()
 *----------------------------------------------------------------------------*/
int UT_MathLib::luaCreate (lua_State* L)
{
    try
    {
        /* Create Math Library Unit Test */
        return createLuaObject(L, new UT_MathLib(L));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_MathLib::UT_MathLib (lua_State* L):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_MathLib::~UT_MathLib(void)
{
}

/*----------------------------------------------------------------------------
 * luaLsfTest
 *----------------------------------------------------------------------------*/
int UT_MathLib::luaLsfTest (lua_State* L)
{
    bool status = false;

    try
    {
        bool tests_passed = true;

        /* Test 1 */
        const int l1 = 4;
        MathLib::point_t v1[l1] = { {1.0, 2.0}, {2.0, 4.0}, {3.0, 6.0}, {4.0, 8.0} };
        MathLib::lsf_t fit1 = MathLib::lsf(v1, l1);
        if(fit1.intercept != 0.0 || fit1.slope != 2.0)
        {
            mlog(CRITICAL, "Failed LSF test01: %lf, %lf\n", fit1.intercept, fit1.slope);
            tests_passed = false;
        }

        /* Test 2 */
        const int l2 = 4;
        MathLib::point_t v2[l2] = { {1.0, 4.0}, {2.0, 5.0}, {3.0, 6.0}, {4.0, 7.0} };
        MathLib::lsf_t fit2 = MathLib::lsf(v2, l2);
        if(fit2.intercept != 3.0 || fit2.slope != 1.0)
        {
            mlog(CRITICAL, "Failed LSF test02: %lf, %lf\n", fit2.intercept, fit2.slope);
            tests_passed = false;
        }

        /* Set Status */
        status = tests_passed;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error executing test %s: %s\n", __FUNCTION__, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSortTest
 *----------------------------------------------------------------------------*/
int UT_MathLib::luaSortTest (lua_State* L)
{
    bool status = false;

    try
    {
        bool tests_passed = true;

        /* Test 1 */
        double a1[10] = { 0.0, 5.0, 1.0, 4.0, 2.0, 3.0, 9.0, 6.0, 7.0, 8.0 };
        double b1[10] = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
        MathLib::sort(a1, 10);
        for(int i = 0; i < 10; i++)
        {
            if(a1[i] != b1[i])
            {
                mlog(CRITICAL, "Failed sort test01 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Test 2 */
        double a2[10] = { 1.0, 1.0, 1.0, 3.0, 2.0, 3.0, 3.0, 6.0, 9.0, 9.0 };
        double b2[10] = { 1.0, 1.0, 1.0, 2.0, 3.0, 3.0, 3.0, 6.0, 9.0, 9.0 };
        MathLib::sort(a2, 10);
        for(int i = 0; i < 10; i++)
        {
            if(a2[i] != b2[i])
            {
                mlog(CRITICAL, "Failed sort test02 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Test 3 */
        double a3[10] = { 9.0, 8.0, 1.0, 7.0, 6.0, 3.0, 5.0, 4.0, 2.0, 0.0 };
        double b3[10] = { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };
        MathLib::sort(a3, 10);
        for(int i = 0; i < 10; i++)
        {
            if(a3[i] != b3[i])
            {
                mlog(CRITICAL, "Failed sort test03 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Test 4 */
        double a4[10] = { 9.0, 8.0, 1.0, 7.0, 6.0, 3.0, 5.0, 4.0, 2.0, 0.0 };
        int     x[10] = { 0,   1,   2,   3,   4,   5,   6,   7,   8,   9 };
        int     s[10] = { 9,   2,   8,   5,   7,   6,   4,   3,   1,   0 };
        MathLib::sort(a4, 10, x);
        for(int i = 0; i < 10; i++)
        {
            if(x[i] != s[i])
            {
                mlog(CRITICAL, "Failed sort test04 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Set Status */
        status = tests_passed;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error executing test %s: %s\n", __FUNCTION__, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
