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

#include "OsApi.h"
#include "GdalRaster.h"
#include "UT_RasterSubset.h"

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
 * luaCreate - :test(<raster>)
 *----------------------------------------------------------------------------*/
int UT_RasterSubset::luaCreate (lua_State* L)
{
    RasterObject* _raster = NULL;
    try
    {
        /* Get Parameters */
        _raster = dynamic_cast<RasterObject*>(getLuaObject(L, 1, RasterObject::OBJECT_TYPE));

        return createLuaObject(L, new UT_RasterSubset(L, _raster));
    }
    catch(const RunTimeException& e)
    {
        if(_raster) _raster->releaseLuaObject();
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
UT_RasterSubset::UT_RasterSubset (lua_State* L, RasterObject* _raster):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    raster(_raster)
{
    assert(raster);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_RasterSubset::~UT_RasterSubset(void)
{
    raster->releaseLuaObject();
}


/*----------------------------------------------------------------------------
 * luaSubsetTest
 *----------------------------------------------------------------------------*/
int UT_RasterSubset::luaSubsetTest(lua_State* L)
{
    bool status = false;
    uint32_t errors = 0;

    UT_RasterSubset* lua_obj = NULL;

    try
    {
        lua_obj = dynamic_cast<UT_RasterSubset*>(getLuaSelf(L, 1));

        const double llx = 149.80;
        const double lly = -70.00;
        const double urx = 150.00;
        const double ury = -69.95;

        /* Get subsets */
        List<RasterSubset*> subsetsList;
        const MathLib::extent_t extent = {{llx, lly}, {urx, ury}};
        lua_obj->raster->getSubsets(extent, 0, subsetsList, NULL);

        std::vector<SampleInfo_t*>  rasterSamples;
        std::vector<SampleInfo_t*>  subRasterSamples;

        /* Get samples from parent RasterObject */
        List<RasterSample*> samplesList;

        const double lon = (llx + urx) / 2.0;
        const double lat = (lly + ury) / 2.0;
        const double height = 0.0;
        print2term("Point: %.2lf, %.2lf, %.2lf\n", lon, lat, height);

        const RasterObject::point_info_t pinfo = {{lon, lat, height}, 0};
        errors += lua_obj->raster->getSamples(pinfo, samplesList, NULL);
        for(int i = 0; i < samplesList.length(); i++)
        {
            const RasterSample* sample = samplesList[i];
            SampleInfo_t* si = new SampleInfo_t(sample, getRasterName(lua_obj->raster, sample->fileId));
            rasterSamples.push_back(si);
        }

        /* Get samples from subsets */
        for(int i = 0; i < subsetsList.length(); i++)
        {
            const RasterSubset* subset = subsetsList[i];
            RasterObject* srobj = dynamic_cast<RasterObject*>(subset->robj);

            /* Get samples */
            samplesList.clear();

            const RasterObject::point_info_t _pinfo = { {lon, lat, height}, 0 };
            errors += srobj->getSamples(_pinfo, samplesList, NULL);

            for(int j = 0; j < samplesList.length(); j++)
            {
                const RasterSample* sample = samplesList[j];
                SampleInfo_t* si = new SampleInfo_t(sample, getRasterName(srobj, sample->fileId));
                subRasterSamples.push_back(si);
            }
        }

        /*
         * Compare samples - the number of samples should be the same as long as 'with_flags' is not set
         * If it is bimask (flags rasters) will be included in subsetted rasters and this test will fail
         */
        if(rasterSamples.size() != subRasterSamples.size())
        {
            mlog(ERROR, "Number of samples differ: %lu != %lu", rasterSamples.size(), subRasterSamples.size());
            errors++;
            return errors;
        }

        for(uint32_t i = 0; i < rasterSamples.size(); i++)
        {
            const RasterSample& rsample = rasterSamples[i]->sample;
            const char* rfileName = rasterSamples[i]->fileName;

            const RasterSample& srsample = subRasterSamples[i]->sample;
            const char* srfileName = subRasterSamples[i]->fileName;

            /* Subraster cannot have the same file name/path as parent raster */
            if(StringLib::match(rfileName, srfileName))
            {
                print2term("Parent raster and subraster have the same filename: %s\n", rfileName);
                errors++;
            }

            print2term("RSample:  %.2lf, %.2lf, %.2lf, %.2lf, %s\n", rsample.value, rsample.stats.mean, rsample.stats.stdev, rsample.stats.mad, rfileName);
            print2term("SRSample: %.2lf, %.2lf, %.2lf, %.2lf, %s\n", srsample.value, srsample.stats.mean, srsample.stats.stdev, srsample.stats.mad, srfileName);

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
            delete rasterSamples[i];
            delete subRasterSamples[i];
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        errors++;
    }


    if(errors == 0) status = true;
    else status = false;

    /* Return Status */
    return returnLuaStatus(L, status);
}


/*----------------------------------------------------------------------------
 * getRasterName
 *----------------------------------------------------------------------------*/
const char* UT_RasterSubset::getRasterName(RasterObject* robj, uint64_t fileId)
{
    const char* fileName = robj->fileDictGet(fileId);
    return StringLib::duplicate(fileName);
}