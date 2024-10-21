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
#include "UT_BathyRefractionCorrector.h"
#include "BathyRefractionCorrector.h"
#include "BathyDataFrame.h"
#include "BathyFields.h"

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


#define PH_REF_NUM_EXPECTED 11

typedef struct { double dE; double dN; double dZ; double w; double z; double ref_az; double ref_el; } ph_ref_t;

const ph_ref_t PH_REF_EXPECTED[PH_REF_NUM_EXPECTED] = {
    {   0.1986984759569168,  0.19957982003688812,   2.2672932147979736, -0.05,  -9.0,  -5.5,   1.5 }, // 0
    {   -4.621915817260742,   0.9966756105422974, 0.005589773412793875, -0.04,  -8.0,  -4.5,   2.5 }, // 1
    {   -6.234014511108398,   16.642427444458008,    7.826122283935547, -0.03,  -7.0,  -3.5,   3.5 }, // 2
    {    2.274247646331787,    3.044417142868042,    -9.78983211517334, -0.02,  -6.0,  -2.5,   4.5 }, // 3
    {   -9.453969955444336,   0.6704267859458923,  -2.1820807456970215, -0.01,  -5.0,  -1.5,   5.5 }, // 4
    {                  0.0,    8.057555198669434,   -5.502264022827148,  0.00,  -4.0,   0.0,   6.5 }, // 5
    {   0.4923339784145355,  0.03491378575563431,   0.6980035305023193,  0.01,  -3.0,   1.5,   7.5 }, // 6
    {  -0.4044313132762909,   0.5413911938667297,  0.33407679200172424,  0.02,  -2.0,   2.5,   8.5 }, // 7
    {   2.5166611671447754,    6.718520164489746,    6.865662097930908,  0.03,  -1.0,   3.5,   9.5 }, // 8
    {  0.05351031944155693, 0.011539030820131302, -0.04730435833334923,  0.04,   0.0,   4.5,  10.5 }, // 9
    { -0.16418953239917755,  0.16491781175136566,                  0.0,  0.05,   1.0,   5.5,  11.5 }, // 10
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_BathyRefractionCorrector::OBJECT_TYPE = "UT_BathyRefractionCorrector";
const char* UT_BathyRefractionCorrector::LUA_META_NAME = "UT_BathyRefractionCorrector";
const struct luaL_Reg UT_BathyRefractionCorrector::LUA_META_TABLE[] = {
    {"riwater",         luaRiWaterTest},
    {"refraction",      luaRefractionTest},
    {NULL,              NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :UT_BathyRefractionCorrector()
 *----------------------------------------------------------------------------*/
int UT_BathyRefractionCorrector::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new UT_BathyRefractionCorrector(L));
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
UT_BathyRefractionCorrector::UT_BathyRefractionCorrector (lua_State* L):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_BathyRefractionCorrector::~UT_BathyRefractionCorrector(void) = default;

/*----------------------------------------------------------------------------
 * luaRiWaterTest
 *----------------------------------------------------------------------------*/
int UT_BathyRefractionCorrector::luaRiWaterTest (lua_State* L)
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


/*----------------------------------------------------------------------------
 * luaRefractionTest
 *----------------------------------------------------------------------------*/
int UT_BathyRefractionCorrector::luaRefractionTest (lua_State* L)
{
    // test variables
    bool status = true;
    BathyFields* parms = NULL;
    BathyRefractionCorrector* refraction = NULL;

    try
    {
        // get refraction object
        parms = dynamic_cast<BathyFields*>(getLuaObject(L, 2, BathyFields::OBJECT_TYPE));
        refraction = dynamic_cast<BathyRefractionCorrector*>(getLuaObject(L, 3, BathyRefractionCorrector::OBJECT_TYPE));

        // build inputs
        BathyDataFrame dataframe(parms);
        for(int i = 0; i < PH_REF_NUM_EXPECTED; i++)
        {
            dataframe.addRow();
            dataframe.surface_h.append(PH_REF_EXPECTED[i].w);
            dataframe.geoid_corr_h.append(PH_REF_EXPECTED[i].z);
            dataframe.ref_az.append(PH_REF_EXPECTED[i].ref_az);
            dataframe.ref_el.append(PH_REF_EXPECTED[i].ref_el);

            dataframe.refracted_dZ.append(0.0);
            dataframe.x_ph.append(0.0);
            dataframe.y_ph.append(0.0);
            dataframe.refracted_lat.append(0.0);
            dataframe.refracted_lon.append(0.0);
        }

        // run refraction code
        status = refraction->run(&dataframe);
        if(!status) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to run refraction code");

        // check results
        double acc_err = 0.0;
        long cnt_err = 0;
        for(int i = 0; i < PH_REF_NUM_EXPECTED; i++)
        {
            const double err = fabs(dataframe.refracted_dZ[i] - PH_REF_EXPECTED[i].dZ);
            if(err > 0.0001)
            {
                mlog(CRITICAL, "Mistached delta Z at row %d: %lf != %lf", i, dataframe.refracted_dZ[i], PH_REF_EXPECTED[i].dZ);
                status = false;
            }
            acc_err += err;
            cnt_err++;
        }

        mlog(CRITICAL, "Total mismatched values = %ld, mean error = %lf\n", cnt_err, acc_err / cnt_err);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        if(parms) parms->releaseLuaObject();
        status =false;
    }

    if(refraction) refraction->releaseLuaObject();
    return returnLuaStatus(L, status);
}
