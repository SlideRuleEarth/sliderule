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

#include "UT_Atl06Dispatch.h"
#include "Atl06Dispatch.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Atl06Dispatch::OBJECT_TYPE = "UT_Atl06Dispatch";

const char* UT_Atl06Dispatch::LuaMetaName = "UT_Atl06Dispatch";
const struct luaL_Reg UT_Atl06Dispatch::LuaMetaTable[] = {
    {"lsftest",     luaLsfTest},
    {"sorttest",    luaSortTest},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :UT_Atl06Dispatch()
 *----------------------------------------------------------------------------*/
int UT_Atl06Dispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Create Math Library Unit Test */
        return createLuaObject(L, new UT_Atl06Dispatch(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_Atl06Dispatch::UT_Atl06Dispatch (lua_State* L):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_Atl06Dispatch::~UT_Atl06Dispatch(void)
{
}

/*----------------------------------------------------------------------------
 * luaLsfTest
 *----------------------------------------------------------------------------*/
int UT_Atl06Dispatch::luaLsfTest (lua_State* L)
{
    bool status = false;

    try
    {
        bool tests_passed = true;

        /* Test 1 */
        const int l1 = 4;
        Atl06Dispatch::point_t v1[l1] = { {1.0, 2.0, 0.0}, {2.0, 4.0, 0.0}, {3.0, 6.0, 0.0}, {4.0, 8.0, 0.0} };
        Atl06Dispatch::lsf_t fit1 = Atl06Dispatch::lsf(v1, l1);
        if(fit1.intercept != 0.0 || fit1.slope != 2.0)
        {
            mlog(CRITICAL, "Failed LSF test01: %lf, %lf\n", fit1.intercept, fit1.slope);
            tests_passed = false;
        }

        /* Test 2 */
        const int l2 = 4;
        Atl06Dispatch::point_t v2[l2] = { {1.0, 4.0, 0.0}, {2.0, 5.0, 0.0}, {3.0, 6.0, 0.0}, {4.0, 7.0, 0.0} };
        Atl06Dispatch::lsf_t fit2 = Atl06Dispatch::lsf(v2, l2);
        if(fit2.intercept != 3.0 || fit2.slope != 1.0)
        {
            mlog(CRITICAL, "Failed LSF test02: %lf, %lf\n", fit2.intercept, fit2.slope);
            tests_passed = false;
        }

        /* Set Status */
        status = tests_passed;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error executing test %s: %s\n", __FUNCTION__, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSortTest
 *----------------------------------------------------------------------------*/
int UT_Atl06Dispatch::luaSortTest (lua_State* L)
{
    bool status = false;

    try
    {
        bool tests_passed = true;

        /* Test 1 */
        Atl06Dispatch::point_t a1[10] = { {0,0,0}, {0,0,5}, {0,0,1}, {0,0,4}, {0,0,2}, {0,0,3}, {0,0,9}, {0,0,6}, {0,0,7}, {0,0,8} };
        Atl06Dispatch::point_t b1[10] = { {0,0,0}, {0,0,1}, {0,0,2}, {0,0,3}, {0,0,4}, {0,0,5}, {0,0,6}, {0,0,7}, {0,0,8}, {0,0,9} };
        Atl06Dispatch::quicksort(a1, 0, 9);
        for(int i = 0; i < 10; i++)
        {
            if(a1[i].r != b1[i].r)
            {
                mlog(CRITICAL, "Failed sort test01 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Test 2 */
        Atl06Dispatch::point_t a2[10] = { {0,0,1}, {0,0,1}, {0,0,1}, {0,0,3}, {0,0,2}, {0,0,3}, {0,0,3}, {0,0,6}, {0,0,9}, {0,0,9} };
        Atl06Dispatch::point_t b2[10] = { {0,0,1}, {0,0,1}, {0,0,1}, {0,0,2}, {0,0,3}, {0,0,3}, {0,0,3}, {0,0,6}, {0,0,9}, {0,0,9} };
        Atl06Dispatch::quicksort(a2, 0, 9);
        for(int i = 0; i < 10; i++)
        {
            if(a2[i].r != b2[i].r)
            {
                mlog(CRITICAL, "Failed sort test02 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Test 3 */
        Atl06Dispatch::point_t a3[10] = { {0,0,9}, {0,0,8}, {0,0,1}, {0,0,7}, {0,0,6}, {0,0,3}, {0,0,5}, {0,0,4}, {0,0,2}, {0,0,0} };
        Atl06Dispatch::point_t b3[10] = { {0,0,0}, {0,0,1}, {0,0,2}, {0,0,3}, {0,0,4}, {0,0,5}, {0,0,6}, {0,0,7}, {0,0,8}, {0,0,9} };
        Atl06Dispatch::quicksort(a3, 0, 9);
        for(int i = 0; i < 10; i++)
        {
            if(a3[i].r != b3[i].r)
            {
                mlog(CRITICAL, "Failed sort test03 at: %d\n", i);
                tests_passed = false;            
                break;
            }
        }

        /* Set Status */
        status = tests_passed;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error executing test %s: %s\n", __FUNCTION__, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
