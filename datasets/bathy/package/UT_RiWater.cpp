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

#include <math.h>

#include "OsApi.h"
#include "UT_RiWater.h"
#include "BathyRefractionCorrector.h"

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

#define RI_WATER_NUM_EXPECTED 53

typedef struct {double lat; double lon; double pixel; } ri_water_t;

const ri_water_t RI_WATER_EXPECTED[RI_WATER_NUM_EXPECTED] = {
    { 90.0, 90.0, 1.3422080542553265 }, //(1080, 0)
    { 80.0, 80.0, 1.3423509939784113 }, //(1040, 40)
    { 70.0, 70.0, -1.7976931348623157e+308 }, //(1000, 80)
    { 60.0, 60.0, -1.7976931348623157e+308 }, //(960, 120)
    { 50.0, 50.0, -1.7976931348623157e+308 }, //(920, 160)
    { 40.0, 40.0, -1.7976931348623157e+308 }, //(880, 200)
    { 30.0, 30.0, -1.7976931348623157e+308 }, //(840, 240)
    { 20.0, 20.0, -1.7976931348623157e+308 }, //(800, 280)
    { 10.0, 10.0, -1.7976931348623157e+308 }, //(760, 320)
    { 0.0, 0.0, 1.3406470291930777 }, //(720, 360)
    { -10.0, -10.0, 1.3411573733845101 }, //(680, 400)
    { -20.0, -20.0, 1.3413915227801096 }, //(640, 440)
    { -30.0, -30.0, 1.3415340226339163 }, //(600, 480)
    { -40.0, -40.0, 1.3418633795687718 }, //(560, 520)
    { -50.0, -50.0, 1.3425161455497452 }, //(520, 560)
    { -60.0, -60.0, 1.342730894352594 }, //(480, 600)
    { -70.0, -70.0, -1.7976931348623157e+308 }, //(440, 640)
    { 0.0, -179.0, 1.3407177122259128 }, //(4, 360)
    { 0.0, -169.0, 1.3407812584824115 }, //(44, 360)
    { 0.0, -159.0, 1.3408570255003933 }, //(84, 360)
    { 0.0, -149.0, 1.3408971026095646 }, //(124, 360)
    { 0.0, -139.0, 1.3409568497465774 }, //(164, 360)
    { 0.0, -129.0, 1.3410211263779968 }, //(204, 360)
    { 0.0, -119.0, 1.3410872090163461 }, //(244, 360)
    { 0.0, -109.0, 1.3411200219433457 }, //(284, 360)
    { 0.0, -99.0, 1.3410983798043399 }, //(324, 360)
    { 0.0, -89.0, 1.3409195768957127 }, //(364, 360)
    { 0.0, -79.0, -1.7976931348623157e+308 }, //(404, 360)
    { 0.0, -69.0, -1.7976931348623157e+308 }, //(444, 360)
    { 0.0, -59.0, -1.7976931348623157e+308 }, //(484, 360)
    { 0.0, -49.0, 1.3385913083832446 }, //(524, 360)
    { 0.0, -39.0, 1.3407963474141062 }, //(564, 360)
    { 0.0, -29.0, 1.3408519255063123 }, //(604, 360)
    { 0.0, -19.0, 1.3409171285576065 }, //(644, 360)
    { 0.0, -9.0, 1.340905969994844 }, //(684, 360)
    { 0.0, 9.0, 1.3401900414100394 }, //(756, 360)
    { 0.0, 19.0, -1.7976931348623157e+308 }, //(796, 360)
    { 0.0, 29.0, -1.7976931348623157e+308 }, //(836, 360)
    { 0.0, 39.0, -1.7976931348623157e+308 }, //(876, 360)
    { 0.0, 49.0, 1.3407421917218392 }, //(916, 360)
    { 0.0, 59.0, 1.3405742386690225 }, //(956, 360)
    { 0.0, 69.0, 1.3404481308528755 }, //(996, 360)
    { 0.0, 79.0, 1.3404054838046964 }, //(1036, 360)
    { 0.0, 89.0, 1.3402996072843225 }, //(1076, 360)
    { 0.0, 99.0, 1.3401480780880304 }, //(1116, 360)
    { 0.0, 109.0, 1.3400061190379688 }, //(1156, 360)
    { 0.0, 119.0, 1.3401077213611579 }, //(1196, 360)
    { 0.0, 129.0, 1.340226480831073 }, //(1236, 360)
    { 0.0, 139.0, 1.3403023164677348 }, //(1276, 360)
    { 0.0, 149.0, 1.3403351515828283 }, //(1316, 360)
    { 0.0, 159.0, 1.340489978341053 }, //(1356, 360)
    { 0.0, 169.0, 1.3406340833058044 }, //(1396, 360)
    { 0.0, 179.0, 1.3407032685157025 }, //(1436, 360)
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_RiWater::OBJECT_TYPE = "UT_RiWater";
const char* UT_RiWater::LUA_META_NAME = "UT_RiWater";
const struct luaL_Reg UT_RiWater::LUA_META_TABLE[] = {
    {"test",            luaTest},
    {NULL,              NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :UT_RiWater()
 *----------------------------------------------------------------------------*/
int UT_RiWater::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new UT_RiWater(L));
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
UT_RiWater::UT_RiWater (lua_State* L):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_RiWater::~UT_RiWater(void) = default;

/*----------------------------------------------------------------------------
 * luaTest
 *----------------------------------------------------------------------------*/
int UT_RiWater::luaTest (lua_State* L)
{
    bool status = true;

    GeoLib::TIFFImage mask(NULL, BathyRefractionCorrector::GLOBAL_WATER_RI_MASK, GeoLib::TIFFImage::GDAL_DRIVER);

    for(int i = 0; i < RI_WATER_NUM_EXPECTED; i++)
    {
        const ri_water_t& entry = RI_WATER_EXPECTED[i];
        const double pixel = BathyRefractionCorrector::sampleWaterMask(&mask, entry.lon, entry.lat);
        if((pixel < 0.0) && (entry.pixel >= 0.0))
        {
            mlog(CRITICAL, "Invalid pixel returned when a valid pixel was expected at (%lf, %lf): %lf != %lf", entry.lat, entry.lon, pixel, entry.pixel);
            status = false;
        }
        else if(pixel != entry.pixel)
        {
            mlog(CRITICAL, "Mismatched water mask value at (%lf, %lf): %lf != %lf", entry.lat, entry.lon, pixel, entry.pixel);
            status = false;
        }
    }

    return returnLuaStatus(L, status);
}

