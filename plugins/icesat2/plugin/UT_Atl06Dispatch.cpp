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
#include "UT_Atl06Dispatch.h"
#include "Atl06Dispatch.h"

#include <cmath>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_Atl06Dispatch::OBJECT_TYPE = "UT_Atl06Dispatch";

const char* UT_Atl06Dispatch::LuaMetaName = "UT_Atl06Dispatch";
const struct luaL_Reg UT_Atl06Dispatch::LuaMetaTable[] = {
    {"lsftest",         luaLsfTest},
    {"sorttest",        luaSortTest},
    {NULL,              NULL}
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
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
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

    /* Create Extent */
    const int num_photons = 4;
    int extent_bytes = sizeof(Atl03Reader::extent_t) + (sizeof(Atl03Reader::photon_t) * num_photons);
    RecordObject* record = new RecordObject(Atl03Reader::exRecType, extent_bytes);
    Atl03Reader::extent_t* extent = (Atl03Reader::extent_t*)record->getRecordData();
    extent->photons[0].x_atc = 1.0;
    extent->photons[1].x_atc = 2.0;
    extent->photons[2].x_atc = 3.0;
    extent->photons[3].x_atc = 4.0;

    /* Allocation Result Structure */
    Atl06Dispatch::result_t result;

    try
    {
        bool tests_passed = true;
        double tolerance = 0.0000001;

        /* Test 1 */
        extent->photons[0].height = 2.0;
        extent->photons[1].height = 4.0;
        extent->photons[2].height = 6.0;
        extent->photons[3].height = 8.0;
        Atl06Dispatch::point_t v1[num_photons] = { {0, 0.0}, {1, 0.0}, {2, 0.0}, {3, 0.0} };
        result.photons = v1;
        result.elevation.photon_count = num_photons;
        Atl06Dispatch::lsf_t fit1 = Atl06Dispatch::lsf(extent, result, false);
        if(fit1.height != 0.0 || fabs(fit1.slope - 2.0) > tolerance)
        {
            mlog(CRITICAL, "Failed LSF test01: %lf, %lf", fit1.height, fit1.slope);
            tests_passed = false;
        }

        /* Test 2 */
        extent->photons[0].height = 4.0;
        extent->photons[1].height = 5.0;
        extent->photons[2].height = 6.0;
        extent->photons[3].height = 7.0;
        Atl06Dispatch::point_t v2[num_photons] = { {0, 0.0}, {1, 0.0}, {2, 0.0}, {3, 0.0} };
        result.photons = v2;
        result.elevation.photon_count = num_photons;
        Atl06Dispatch::lsf_t fit2 = Atl06Dispatch::lsf(extent, result, false);
        if(fabs(fit2.height - 3.0) > tolerance || fabs(fit2.slope - 1.0) > tolerance)
        {
            mlog(CRITICAL, "Failed LSF test02: %lf, %lf", fit2.height, fit2.slope);
            tests_passed = false;
        }

        /* Set Status */
        status = tests_passed;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error executing test %s: %s", __FUNCTION__, e.what());
    }

    /* Clean Up Extent */
    delete record;

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
        Atl06Dispatch::point_t a1[10] = { {0,0}, {0,5}, {0,1}, {0,4}, {0,2}, {0,3}, {0,9}, {0,6}, {0,7}, {0,8} };
        Atl06Dispatch::point_t b1[10] = { {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8}, {0,9} };
        Atl06Dispatch::quicksort(a1, 0, 9);
        for(int i = 0; i < 10; i++)
        {
            if(a1[i].r != b1[i].r)
            {
                mlog(CRITICAL, "Failed sort test01 at: %d", i);
                tests_passed = false;
                break;
            }
        }

        /* Test 2 */
        Atl06Dispatch::point_t a2[10] = { {0,1}, {0,1}, {0,1}, {0,3}, {0,2}, {0,3}, {0,3}, {0,6}, {0,9}, {0,9} };
        Atl06Dispatch::point_t b2[10] = { {0,1}, {0,1}, {0,1}, {0,2}, {0,3}, {0,3}, {0,3}, {0,6}, {0,9}, {0,9} };
        Atl06Dispatch::quicksort(a2, 0, 9);
        for(int i = 0; i < 10; i++)
        {
            if(a2[i].r != b2[i].r)
            {
                mlog(CRITICAL, "Failed sort test02 at: %d", i);
                tests_passed = false;
                break;
            }
        }

        /* Test 3 */
        Atl06Dispatch::point_t a3[10] = { {0,9}, {0,8}, {0,1}, {0,7}, {0,6}, {0,3}, {0,5}, {0,4}, {0,2}, {0,0} };
        Atl06Dispatch::point_t b3[10] = { {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8}, {0,9} };
        Atl06Dispatch::quicksort(a3, 0, 9);
        for(int i = 0; i < 10; i++)
        {
            if(a3[i].r != b3[i].r)
            {
                mlog(CRITICAL, "Failed sort test03 at: %d", i);
                tests_passed = false;
                break;
            }
        }

        /* Set Status */
        status = tests_passed;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error executing test %s: %s", __FUNCTION__, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
