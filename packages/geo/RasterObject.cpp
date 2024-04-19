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
#include "core.h"
#include "RasterObject.h"
#include "GdalRaster.h"
#include "GeoIndexedRaster.h"
#include "Ordering.h"
#include "List.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RasterObject::OBJECT_TYPE  = "RasterObject";
const char* RasterObject::LUA_META_NAME  = "RasterObject";
const struct luaL_Reg RasterObject::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

Mutex RasterObject::factoryMut;
Dictionary<RasterObject::factory_t> RasterObject::factories;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void RasterObject::init( void )
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void RasterObject::deinit( void )
{
}

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int RasterObject::luaCreate( lua_State* L )
{
    GeoParms* _parms = NULL;
    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<GeoParms*>(getLuaObject(L, 1, GeoParms::OBJECT_TYPE));
        if(_parms == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create GeoParms object");

        /* Get Factory */
        factory_t factory;
        bool found = false;
        factoryMut.lock();
        {
            found = factories.find(_parms->asset_name, &factory);
        }
        factoryMut.unlock();

        /* Check Factory */
        if(!found) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to find registered raster for %s", _parms->asset_name);

        /* Create Raster */
        RasterObject* _raster = factory.create(L, _parms);
        if(_raster == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create raster of type: %s", _parms->asset_name);

        /* Return Object */
        return createLuaObject(L, _raster);
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * registerDriver
 *----------------------------------------------------------------------------*/
bool RasterObject::registerRaster (const char* _name, factory_f create)
{
    bool status;

    factoryMut.lock();
    {
        factory_t factory = { .create = create };
        status = factories.add(_name, factory);
    }
    factoryMut.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * getPixels
 *----------------------------------------------------------------------------*/
uint8_t* RasterObject::getPixels(uint32_t ulx, uint32_t uly, uint32_t xsize, uint32_t ysize, void* param)
{
    std::ignore = ulx;
    std::ignore = uly;
    std::ignore = xsize;
    std::ignore = ysize;
    std::ignore = param;

    return NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RasterObject::~RasterObject(void)
{
    /* Release GeoParms LuaObject */
    parms->releaseLuaObject();
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterObject::RasterObject(lua_State *L, GeoParms* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "sample",     luaSamples);
    LuaEngine::setAttrFunc(L, "subsettest", luaSubsetTest);
}

/*----------------------------------------------------------------------------
 * fileDictAdd
 *----------------------------------------------------------------------------*/
uint64_t RasterObject::fileDictAdd(const std::string& fileName)
{
    uint64_t id;

    if(!fileDict.find(fileName.c_str(), &id))
    {
        id = (parms->key_space << 32) | fileDict.length();
        fileDict.add(fileName.c_str(), id);
    }

    return id;
}

/*----------------------------------------------------------------------------
 * luaSamples - :sample(lon, lat, [height], [gps]) --> in|out
 *----------------------------------------------------------------------------*/
int RasterObject::luaSamples(lua_State *L)
{
    uint32_t err = SS_NO_ERRORS;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;
    std::vector<RasterSample*> slist;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<RasterObject*>(getLuaSelf(L, 1));

        /* Get Coordinates */
        double lon    = getLuaFloat(L, 2);
        double lat    = getLuaFloat(L, 3);
        double height = getLuaFloat(L, 4, true, 0.0);
        const char* closest_time_str = getLuaString(L, 5, true, NULL);

        /* Get gps closest time (overrides params provided closest time) */
        int64_t gps = 0;
        if(closest_time_str != NULL)
        {
            gps = TimeLib::str2gpstime(closest_time_str);
        }

        /* Get samples */
        bool listvalid = true;
        OGRPoint poi(lon, lat, height);
        err = lua_obj->getSamples(&poi, gps, slist, NULL);

        if(err & SS_THREADS_LIMIT_ERROR)
        {
            listvalid = false;
            mlog(CRITICAL, "Too many rasters to sample, max allowed: %d, limit your AOI/temporal range or use filters", GeoIndexedRaster::MAX_READER_THREADS);
        }

        /* Create return table */
        lua_createtable(L, slist.size(), 0);
        num_ret++;

        /* Populate samples */
        if(listvalid && !slist.empty())
        {
            for(uint32_t i = 0; i < slist.size(); i++)
            {
                const RasterSample* sample = slist[i];
                const char* fileName = "";

                /* Find fileName from fileId */
                Dictionary<uint64_t>::Iterator iterator(lua_obj->fileDictGet());
                for(int j = 0; j < iterator.length; j++)
                {
                    if(iterator[j].value == sample->fileId)
                    {
                        fileName = iterator[j].key;
                        break;
                    }
                }

                lua_createtable(L, 0, 4);
                LuaEngine::setAttrStr(L, "file", fileName);

                if(lua_obj->parms->zonal_stats) /* Include all zonal stats */
                {
                    LuaEngine::setAttrNum(L, "mad", sample->stats.mad);
                    LuaEngine::setAttrNum(L, "stdev", sample->stats.stdev);
                    LuaEngine::setAttrNum(L, "median", sample->stats.median);
                    LuaEngine::setAttrNum(L, "mean", sample->stats.mean);
                    LuaEngine::setAttrNum(L, "max", sample->stats.max);
                    LuaEngine::setAttrNum(L, "min", sample->stats.min);
                    LuaEngine::setAttrNum(L, "count", sample->stats.count);
                }

                if(lua_obj->parms->flags_file) /* Include flags */
                {
                    LuaEngine::setAttrNum(L, "flags", sample->flags);
                }

                LuaEngine::setAttrInt(L, "fileid", sample->fileId);
                LuaEngine::setAttrNum(L, "time", sample->time);
                LuaEngine::setAttrNum(L, "value", sample->value);
                lua_rawseti(L, -2, i+1);
            }
        } else mlog(DEBUG, "No samples read for (%.2lf, %.2lf)", lon, lat);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to read samples: %s", e.what());
    }

    /* Free samples */
    for (const RasterSample* sample : slist)
        delete sample;

    /* Return Errors and Table of Samples */
    lua_pushinteger(L, err);
    return num_ret;
}


/*----------------------------------------------------------------------------
 * luaSubsetTest - :subsettest(lon_min, lat_min, lon_max, lat_max) --> in|out
 *----------------------------------------------------------------------------*/
int RasterObject::luaSubsetTest(lua_State *L)
{
    uint32_t err = SS_NO_ERRORS;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;
    std::vector<RasterSubset*> slist;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<RasterObject*>(getLuaSelf(L, 1));

        /* Get extent */
        double lon_min = getLuaFloat(L, 2);
        double lat_min = getLuaFloat(L, 3);
        double lon_max = getLuaFloat(L, 4);
        double lat_max = getLuaFloat(L, 5);
        const char* closest_time_str = getLuaString(L, 6, true, NULL);

        /* Get gps closest time (overrides params provided closest time) */
        int64_t gps = 0;
        if(closest_time_str != NULL)
        {
            gps = TimeLib::str2gpstime(closest_time_str);
        }

        GeoParms::bbox_t bbox = {lon_min, lat_min, lon_max, lat_max};

        /* Get subset */
        OGRPolygon poly = GdalRaster::makeRectangle(lon_min, lat_min, lon_max, lat_max);
        err = lua_obj->getSubsets(&poly, gps, slist, NULL);
        if(err == SS_NO_ERRORS)
        {
            err = lua_obj->subsetTest(lua_obj, bbox, slist);
        }
        else if(err & SS_THREADS_LIMIT_ERROR)
        {
            mlog(CRITICAL, "Too many rasters to subset, max allowed: %d, limit your AOI/temporal range or use filters", GeoIndexedRaster::MAX_READER_THREADS);
        }
        else if(err & SS_MEMPOOL_ERROR)
        {
            mlog(CRITICAL, "Some rasters could not be subset, requested memory size > max allowed: %ld MB", RasterSubset::MAX_SIZE / (1024 * 1024));
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to subset raster: %s", e.what());
    }

    /* Free subsets */
    for (const RasterSubset* subset : slist)
        delete subset;

    /* Return Errors and Table of Samples */
    lua_pushinteger(L, err);

    return num_ret;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

const char* getRasterName(RasterObject* robj, uint64_t fileId)
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

/*----------------------------------------------------------------------------
 * subsetTest
 *----------------------------------------------------------------------------*/
int RasterObject::subsetTest(RasterObject* robj, const GeoParms::bbox_t& bbox, const std::vector<RasterSubset*>& subsetsList)
{
    uint32_t errors = 0;

    if(subsetsList.empty())
    {
        mlog(DEBUG, "No subsets read");
        return 0;
    }

    double lon = bbox.lon_min + (bbox.lon_max - bbox.lon_min) / 2.0;
    double lat = bbox.lat_min + (bbox.lat_max - bbox.lat_min) / 2.0;
    double height = 0.0;

    typedef struct
    {
        RasterSample  sample;
        const char*   fileName;
    } SampleInfo_t;

    std::vector<SampleInfo_t> rasterSamples;
    std::vector<SampleInfo_t> subRasterSamples;

    /* Get samples from parent RasterObject */
    std::vector<RasterSample*> samplesList;

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

    return errors;
}

