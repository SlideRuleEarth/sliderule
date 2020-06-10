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
        MathLib::lsf_t fit = MathLib::lsf(v1, l1);
        if(fit.intercept != 0.0 || fit.slope != 2.0)
        {
            mlog(CRITICAL, "Failed LSF test01: %lf, %lf\n", fit.intercept, fit.slope);
            tests_passed = false;
        }

        /* Set Success */
        status = tests_passed;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error executing test %s: %s\n", __FUNCTION__, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
