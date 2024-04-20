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
#include "UT_RasterSubset.h"
#include "RemaDemMosaicRaster.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_RasterSubset::OBJECT_TYPE = "UT_RasterSubset";

const char* UT_RasterSubset::LUA_META_NAME = "UT_RasterSubset";
const struct luaL_Reg UT_RasterSubset::LUA_META_TABLE[] = {
    {"test",         luaSubsetTest},
    {NULL,           NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :UT_RasterSubset()
 *----------------------------------------------------------------------------*/
int UT_RasterSubset::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new UT_RasterSubset(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_RasterSubset::UT_RasterSubset (lua_State* L):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_RasterSubset::~UT_RasterSubset(void)
{
}


/*----------------------------------------------------------------------------
 * luaSubsetTest
 *----------------------------------------------------------------------------*/
int UT_RasterSubset::luaSubsetTest(lua_State* L)
{
    bool status = false;
    uint32_t errors = 0;

    RasterObject* robj = NULL;
    try
    {
        UT_RasterSubset* lua_obj = dynamic_cast<UT_RasterSubset*>(getLuaSelf(L, 1));
        if(lua_obj == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create UT_RasterObject object");

        /* Get Parameters */
        robj = dynamic_cast<RasterObject*>(getLuaObject(L, 2, RasterObject::OBJECT_TYPE));
        if(robj == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create RasterObject object");

        const double llx = 149.80;
        const double lly = -70.00;
        const double urx = 150.00;
        const double ury = -69.95;

        /* Get subsets */
        std::vector<RasterSubset*> subsetsList;
        OGRPolygon poly = GdalRaster::makeRectangle(llx, lly, urx, ury);
        robj->getSubsets(&poly, 0, subsetsList, NULL);

        std::vector<SampleInfo_t>  rasterSamples;
        std::vector<SampleInfo_t>  subRasterSamples;

        /* Get samples from parent RasterObject */
        std::vector<RasterSample*> samplesList;

        const double lon = (llx + urx) / 2.0;
        const double lat = (lly + ury) / 2.0;
        const double height = 0.0;

        OGRPoint poi(lon, lat, height);
        errors += robj->getSamples(&poi, 0, samplesList, NULL);
        for(uint32_t i = 0; i < samplesList.size(); i++)
        {
            const RasterSample* sample = samplesList[i];
            SampleInfo_t si = { *sample, getRasterName(robj, sample->fileId) };
            rasterSamples.push_back(si);
            delete sample;
        }

        /* Get samples from subsets */
        for(uint32_t i = 0; i < subsetsList.size(); i++)
        {
            const RasterSubset* subset = subsetsList[i];
            RasterObject* srobj = dynamic_cast<RasterObject*>(subset->robj);

            /* Get samples */
            samplesList.clear();

            OGRPoint _poi(lon, lat, height);
            errors += srobj->getSamples(&_poi, 0, samplesList, NULL);

            for(uint32_t j = 0; j < samplesList.size(); j++)
            {
                const RasterSample* sample = samplesList[j];
                SampleInfo_t si = { *sample, getRasterName(srobj, sample->fileId) };
                subRasterSamples.push_back(si);
                delete sample;
            }
        }

        // for(uint32_t indx = 0; indx < subRasterSamples.size(); indx++)
        // {
        //     const RasterSample& srsample = subRasterSamples[indx].sample;
        //     const char* srfileName = subRasterSamples[indx].fileName;
        //     print2term("SRSample: %lf, %lf, %lf, %lf, %lf, %s\n", srsample.time, srsample.value, srsample.stats.mean, srsample.stats.stdev, srsample.stats.mad, srfileName);
        // }

        /* Compare samples */
        if(rasterSamples.size() != subRasterSamples.size())
        {
            mlog(ERROR, "Number of samples differ: %lu != %lu", rasterSamples.size(), subRasterSamples.size());
            errors++;
            return errors;
        }

        for(uint32_t i = 0; i < rasterSamples.size(); i++)
        {
            const RasterSample& rsample = rasterSamples[i].sample;
            const char* rfileName = rasterSamples[i].fileName;

            const RasterSample& srsample = subRasterSamples[i].sample;
            const char* srfileName = subRasterSamples[i].fileName;

            print2term("RSample:  %lf, %lf, %lf, %lf, %lf, %s\n", rsample.time, rsample.value, rsample.stats.mean, rsample.stats.stdev, rsample.stats.mad, rfileName);
            print2term("SRSample: %lf, %lf, %lf, %lf, %lf, %s\n", srsample.time, srsample.value, srsample.stats.mean, srsample.stats.stdev, srsample.stats.mad, srfileName);

            if(rsample.time != srsample.time)
            {
                print2term("Time differ: %lf != %lf\n", rsample.time, srsample.time);
                errors++;
            }

            if(rsample.value != srsample.value)
            {
                print2term("Value differ: %lf != %lf\n", rsample.value, srsample.value);
                errors++;
            }

            if(rsample.stats.mean != srsample.stats.mean)
            {
                print2term("Mean differ: %lf != %lf\n", rsample.stats.mean, srsample.stats.mean);
                errors++;
            }

            if(rsample.stats.stdev != srsample.stats.stdev)
            {
                print2term("Stdev differ: %lf != %lf\n", rsample.stats.stdev, srsample.stats.stdev);
                errors++;
            }

            if(rsample.stats.mad != srsample.stats.mad)
            {
                print2term("Mad differ: %lf != %lf\n", rsample.stats.mad, srsample.stats.mad);
                errors++;
            }
            print2term("\n");
        }



    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
    }


    if(errors == 0) status = true;
    else status = false;

    /* Return Status */
    if(robj) robj->releaseLuaObject();
    return returnLuaStatus(L, status);
}


/*----------------------------------------------------------------------------
 * getRasterName
 *----------------------------------------------------------------------------*/
const char* UT_RasterSubset::getRasterName(RasterObject* robj, uint64_t fileId)
{
    const char* fileName = "";

    /* Find fileName from fileId */
    Dictionary<uint64_t>::Iterator iterator(robj->fileDictGet());
    for(int i = 0; i < iterator.length; i++)
    {
        if(iterator[i].value == fileId)
        {
            fileName = iterator[i].key;
            break;
        }
    }

    return fileName;
}