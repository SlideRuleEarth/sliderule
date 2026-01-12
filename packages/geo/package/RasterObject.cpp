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
#include "RequestFields.h"
#include "RasterObject.h"
#include "GdalRaster.h"
#include "GeoIndexedRaster.h"
#include "GeoFields.h"
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
 * luaCreate
 *----------------------------------------------------------------------------*/
int RasterObject::luaCreate( lua_State* L )
{
    RequestFields* rqst_parms = NULL;
    try
    {
        /* Get Parameters */
        rqst_parms = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        if(rqst_parms == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to get request parameters");

        const char* key = getLuaString(L, 2, true, GeoFields::DEFAULT_KEY);
        const GeoFields* geo_fields = &rqst_parms->samplers[key];

        /* Get Factory */
        factory_t factory;
        bool found = false;
        factoryMut.lock();
        {
            found = factories.find(geo_fields->asset.getName(), &factory);
        }
        factoryMut.unlock();

        /* Check Factory */
        if(!found) throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to find registered raster for %s", geo_fields->asset.getName());

        /* Create Raster */
        RasterObject* _raster = factory.create(L, rqst_parms, key);
        if(_raster == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create raster of type: %s", geo_fields->asset.getName());

        /* Return Object */
        return createLuaObject(L, _raster);
    }
    catch(const RunTimeException& e)
    {
        if(rqst_parms) rqst_parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * cppCreate
 *----------------------------------------------------------------------------*/
RasterObject* RasterObject::cppCreate(RequestFields* rqst_parms, const char* key)
{
    /* Check Parameters */
    if(!rqst_parms) return NULL;

    /* Get Geo Fields */
    const GeoFields* geo_fields = NULL;
    try
    {
        geo_fields = &rqst_parms->samplers[key];
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to retrieve %s from samplers: %s", key, e.what());
        return NULL;
    }

    /* Get Factory */
    factory_t factory;
    bool found = false;

    factoryMut.lock();
    {
        found = factories.find(geo_fields->asset.getName(), &factory);
    }
    factoryMut.unlock();

    /* Check Factory */
    if(!found)
    {
        mlog(CRITICAL, "Failed to find registered raster %s for %s", key, geo_fields->asset.getName());
        return NULL;
    }

    /* Create Raster */
    RasterObject* _raster = factory.create(NULL, rqst_parms, key);
    if(!_raster)
    {
        mlog(CRITICAL, "Failed to create raster %s for %s", key, geo_fields->asset.getName());
        return NULL;
    }

    /* Bump Lua Reference (for releasing in destructor) */
    referenceLuaObject(rqst_parms);

    /* Return Raster */
    return _raster;
}

/*----------------------------------------------------------------------------
 * cppCreate
 *----------------------------------------------------------------------------*/
RasterObject* RasterObject::cppCreate(const RasterObject* obj)
{
    RasterObject* robj = cppCreate(obj->rqstParms, obj->samplerKey);

    /* Copy CRS */
    if(robj) robj->crs = obj->crs;
    return robj;
}

/*----------------------------------------------------------------------------
 * registerRaster
 *----------------------------------------------------------------------------*/
bool RasterObject::registerRaster (const char* _name, factory_f create)
{
    bool status;

    factoryMut.lock();
    {
        const factory_t factory = { .create = create };
        status = factories.add(_name, factory);
    }
    factoryMut.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * luaFatories
 *----------------------------------------------------------------------------*/
int RasterObject::luaFatories (lua_State* L)
{
    lua_newtable(L);

    char** registered_rasters = NULL;
    const int num_registered = factories.getKeys(&registered_rasters);
    for(int i = 0; i < num_registered; i++)
    {
        lua_pushstring(L, registered_rasters[i]);
        lua_rawseti(L, -2, i + 1);
        delete [] registered_rasters[i];
    }
    delete [] registered_rasters;

    return 1;
}

/*----------------------------------------------------------------------------
 * getSamples
 *----------------------------------------------------------------------------*/
uint32_t RasterObject::getSamples(const point_info_t& pinfo, sample_list_t& slist, void* param)
{
    std::vector<point_info_t> points;
    points.push_back(pinfo);

    List<sample_list_t*> sllist;
    const uint32_t ssErrors = getSamples(points, sllist, param);

    if(sllist.length() != 1)
    {
        if(sllist.length() > 1)
        {
            mlog(CRITICAL, "getSamples returned %u elements", sllist.length());
        }
        return ssErrors;
    }

    sample_list_t* batch_slist = sllist[0];
    if(batch_slist)
    {
        for(int i = 0; i < batch_slist->length(); i++)
        {
            RasterSample* sample = batch_slist->get(i);
            slist.add(sample);

            /* Set sample to NULL, don't delete since slist owns samples now */
            batch_slist->set(i, NULL, false);
        }
    }

    return ssErrors;
}

/*----------------------------------------------------------------------------
 * getPixels
 *----------------------------------------------------------------------------*/
uint8_t* RasterObject::getPixels(uint32_t ulx, uint32_t uly, uint32_t xsize, uint32_t ysize, int bandNum, void* param)
{
    static_cast<void>(ulx);
    static_cast<void>(uly);
    static_cast<void>(xsize);
    static_cast<void>(ysize);
    static_cast<void>(bandNum);
    static_cast<void>(param);
    return NULL;
}

/*----------------------------------------------------------------------------
 * getSubsets
 *----------------------------------------------------------------------------*/
uint32_t RasterObject::getSubsets(const MathLib::extent_t& extent, int64_t gps, List<RasterSubset*>& slist, void* param)
{
    static_cast<void>(extent);
    static_cast<void>(gps);
    static_cast<void>(slist);
    static_cast<void>(param);
    return SS_NO_ERRORS;
}

/*----------------------------------------------------------------------------
 * getBands
 *----------------------------------------------------------------------------*/
void RasterObject::getBands(std::vector<std::string>& bands)
{
    for(long i = 0; i < parms->bands.length(); i++)
    {
        const std::string& bandName = parms->bands[i];
        bands.push_back(bandName);
    }
}

/*----------------------------------------------------------------------------
 * getInnerBands
 *----------------------------------------------------------------------------*/
void RasterObject::getInnerBands(std::vector<std::string>& bands)
{
    return getBands(bands);
}

/*----------------------------------------------------------------------------
 * getInnerBands
 *----------------------------------------------------------------------------*/
void RasterObject::getInnerBands(void* rptr, std::vector<int>& bands)
{
    GdalRaster* raster = static_cast<GdalRaster*>(rptr);

    std::vector<std::string> bandsNames;
    getInnerBands(bandsNames);

    if(bandsNames.empty())
    {
        /* Default to first band */
        bands.push_back(1);
    }
    else
    {
        for(const std::string& bname : bandsNames)
        {
            const int bandNum = raster->getBandNumber(bname);
            if(bandNum > 0)
            {
                bands.push_back(bandNum);
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RasterObject::~RasterObject(void)
{
    /* Release RequestFields LuaObject */
    rqstParms->releaseLuaObject();

    /* Delete Key */
    delete [] samplerKey;
}

/*----------------------------------------------------------------------------
 * stopSampling
 *----------------------------------------------------------------------------*/
void RasterObject::stopSampling(void)
{
    samplingEnabled = false;
    onStopSampling();
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterObject::RasterObject(lua_State *L, RequestFields* rqst_parms, const char* key):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    rqstParms(rqst_parms),
    parms(&rqstParms->samplers[key]),
    samplerKey(StringLib::duplicate(key)),
    fileDict(rqstParms->keySpace.value),
    samplingEnabled(true)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "batchsample", luaBatchSamples);
    LuaEngine::setAttrFunc(L, "sample", luaSamples);
    LuaEngine::setAttrFunc(L, "subset", luaSubsets);
}

/*----------------------------------------------------------------------------
 * luaBatchSamples - :batchsample(lons, lats, heights, [gps]) --> in|out
 *----------------------------------------------------------------------------*/
int RasterObject::luaBatchSamples(lua_State *L)
{
    uint32_t err = SS_NO_ERRORS;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;
    List<sample_list_t*> sllist;

    try
    {
        /* Validate Input Arguments */
        if (!lua_istable(L, 2) || !lua_istable(L, 3) || !lua_istable(L, 4))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Expected three arrays (tables) as arguments for lon, lat, and height");
        }

        /* Get Self */
        lua_obj = dynamic_cast<RasterObject*>(getLuaSelf(L, 1));

        std::vector<double> lonVec;
        std::vector<double> latVec;
        std::vector<double> heightVec;

        /* Helper Lambda to Read Lua Table into Vector */
        auto luaTableToVector = [&](int tableIndex, std::vector<double> &vec)
        {
            lua_pushnil(L); // Start at the beginning of the table
            while (lua_next(L, tableIndex) != 0)
            {
                if (lua_isnumber(L, -1))
                {
                    vec.push_back(lua_tonumber(L, -1));
                }
                else
                {
                    throw RunTimeException(CRITICAL, RTE_FAILURE, "Non-numeric value found in table");
                }
                lua_pop(L, 1); // Remove value, keep key for next iteration
            }
        };

        /* Read Tables */
        luaTableToVector(2, lonVec);     // lon table
        luaTableToVector(3, latVec);     // lat table
        luaTableToVector(4, heightVec);  // height table

        /* Validate Sizes */
        if (lonVec.size() != latVec.size() || lonVec.size() != heightVec.size())
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Input arrays (lon, lat, height) must have the same size");
        }

        const char* closest_time_str = getLuaString(L, 5, true, NULL);

        /* Get gps closest time (overrides params provided closest time) */
        int64_t gps = 0;
        if(closest_time_str != NULL)
        {
            gps = TimeLib::str2gpstime(closest_time_str) / 1000;
        }

        /* Create point_info_t vector from tables */
        std::vector<point_info_t> points;
        for (size_t i = 0; i < lonVec.size(); i++)
        {
            points.push_back({{lonVec[i], latVec[i], heightVec[i]}, gps});
        }

        mlog(DEBUG, "Batch sample received %lu points", points.size());

        /* Get samples */
        err = lua_obj->getSamples(points, sllist, NULL);
        if(err == SS_NO_ERRORS)
        {
            mlog(DEBUG, "Batch sample received %d samples lists", sllist.length());

            /* Create parent table with space for `points.size()` entries */
            lua_createtable(L, points.size(), 0);
            num_ret++;

            /* Process samples list for each point */
            for(size_t i = 0; i < points.size(); i++)
            {
                const sample_list_t* slist = sllist[i];

                /* Create sample table for this point */
                lua_createtable(L, slist->length(), 0);

                /* Populate table with samples */
                setLuaSamples(L, *slist, lua_obj);

                /* Insert table into parent table */
                lua_rawseti(L, -2, i + 1);
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to get samples");
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to read samples: %s", e.what());
    }

    /* Return Errors and Table of Samples */
    lua_pushinteger(L, err);
    return num_ret;
}

/*----------------------------------------------------------------------------
 * luaSamples - :sample(lon, lat, [height], [gps]) --> in|out
 *----------------------------------------------------------------------------*/
int RasterObject::luaSamples(lua_State *L)
{
    uint32_t err = SS_NO_ERRORS;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;
    List<RasterSample*> slist;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<RasterObject*>(getLuaSelf(L, 1));

        /* Get Coordinates */
        const double lon    = getLuaFloat(L, 2);
        const double lat    = getLuaFloat(L, 3);
        const double height = getLuaFloat(L, 4, true, 0.0);
        const char* closest_time_str = getLuaString(L, 5, true, NULL);

        /* Get gps closest time (overrides params provided closest time) */
        int64_t gps = 0;
        if(closest_time_str != NULL)
        {
            gps = TimeLib::str2gpstime(closest_time_str) / 1000;
        }

        /* Get samples */
        bool listvalid = true;
        const point_info_t pinfo = {{lon, lat, height}, gps};
        err = lua_obj->getSamples(pinfo, slist, NULL);

        if(err & SS_THREADS_LIMIT_ERROR)
        {
            listvalid = false;
            mlog(CRITICAL, "Too many rasters to sample, max allowed: %d, limit your AOI/temporal range or use filters", GeoIndexedRaster::MAX_READER_THREADS);
        }

        if(err & SS_RESOURCE_LIMIT_ERROR)
        {
            listvalid = false;
            mlog(CRITICAL, "System resource limit reached, could not sample rasters");
        }

        /* Create return table */
        lua_createtable(L, slist.length(), 0);
        num_ret++;

        /* Populate samples */
        if(listvalid && !slist.empty())
        {
            setLuaSamples(L, slist, lua_obj);
        } else mlog(DEBUG, "No samples read for (%.2lf, %.2lf)", lon, lat);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to read samples: %s", e.what());
    }

    /* Return Errors and Table of Samples */
    lua_pushinteger(L, err);
    return num_ret;
}

/*----------------------------------------------------------------------------
 * luaSubsets - :subset(lon_min, lat_min, lon_max, lat_max) --> in|out
 *----------------------------------------------------------------------------*/
int RasterObject::luaSubsets(lua_State *L)
{
    uint32_t err = SS_NO_ERRORS;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;
    List<RasterSubset*> slist;

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

        /* Get subset */
        const MathLib::extent_t extent = {{lon_min, lat_min}, {lon_max, lat_max}};
        err = lua_obj->getSubsets(extent, gps, slist, NULL);
        num_ret += slist2table(slist, err, L);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to subset raster: %s", e.what());
    }

    /* Return Errors and Table of Samples */
    lua_pushinteger(L, err);

    return num_ret;
}

/*----------------------------------------------------------------------------
 * getThreadsRanges
 *----------------------------------------------------------------------------*/
void RasterObject::getThreadsRanges(std::vector<range_t>& ranges, uint32_t num,
                                    uint32_t minPerThread, uint32_t maxNumThreads)
{
    ranges.clear();

    /* Determine how many threads to use */
    if(num <= minPerThread)
    {
        ranges.emplace_back(range_t{0, num});
        return;
    }

    /* Divide and round up to get needed threads, clamp to [1, maxNumThreads] */
    uint32_t numThreads = (minPerThread > 0) ? ((num + minPerThread - 1) / minPerThread) : 1;
    if(numThreads == 0) numThreads = 1;
    numThreads = std::min(maxNumThreads, numThreads);

    const uint32_t pointsPerThread = num / numThreads;
    uint32_t remainingPoints = num % numThreads;

    uint32_t start = 0;
    for(uint32_t i = 0; i < numThreads; i++)
    {
        const uint32_t end = start + pointsPerThread + (remainingPoints > 0 ? 1 : 0);
        ranges.emplace_back(range_t{start, end});

        start = end;
        if(remainingPoints > 0)
        {
            remainingPoints--;
        }
    }
}


/*----------------------------------------------------------------------------
 * fileDictSetSamples
 *----------------------------------------------------------------------------*/
void RasterObject::fileDictSetSamples(List<RasterSample*>* slist)
{
    for(int i = 0; i < slist->length(); i++)
    {
        const RasterSample *sample = slist->get(i);
        fileDict.setSample(sample->fileId);
    }
}


/*----------------------------------------------------------------------------
 * setLuaSamples
 *----------------------------------------------------------------------------*/
void RasterObject::setLuaSamples(lua_State *L, const List<RasterSample*> &slist, RasterObject *lua_obj)
{
    if (!lua_obj || slist.empty())
    {
        mlog(DEBUG, "No samples to populate");
        return;
    }

    /* Populate samples */
    for (int i = 0; i < slist.length(); i++)
    {
        const RasterSample* sample = slist[i];
        const char* fileName = lua_obj->fileDict.get(sample->fileId);

        lua_createtable(L, 0, 4); // Create a new table for the sample

        /* Add basic attributes */
        LuaEngine::setAttrStr(L, "file", fileName);
        LuaEngine::setAttrNum(L, "value", sample->value);
        LuaEngine::setAttrNum(L, "time", sample->time);
        LuaEngine::setAttrStr(L, "band", sample->bandName.c_str());

        /* Add zonal statistics if enabled */
        if (lua_obj->parms->zonal_stats)
        {
            LuaEngine::setAttrNum(L, "mad", sample->stats.mad);
            LuaEngine::setAttrNum(L, "stdev", sample->stats.stdev);
            LuaEngine::setAttrNum(L, "median", sample->stats.median);
            LuaEngine::setAttrNum(L, "mean", sample->stats.mean);
            LuaEngine::setAttrNum(L, "max", sample->stats.max);
            LuaEngine::setAttrNum(L, "min", sample->stats.min);
            LuaEngine::setAttrNum(L, "count", sample->stats.count);
        }

        if(lua_obj->parms->slope_aspect)
        {
            LuaEngine::setAttrNum(L, "slope", sample->derivs.slopeDeg);
            LuaEngine::setAttrNum(L, "aspect", sample->derivs.aspectDeg);
            LuaEngine::setAttrNum(L, "slope_count", sample->derivs.count);
        }

        /* Add flags if enabled */
        if (lua_obj->parms->flags_file)
        {
            LuaEngine::setAttrNum(L, "flags", sample->flags);
        }

        /* Add sample table to parent Lua table, insert at index i+1 */
        lua_rawseti(L, -2, i + 1);
    }
}


/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * slist2table
 *----------------------------------------------------------------------------*/
int RasterObject::slist2table(const List<RasterSubset*>& slist, uint32_t errors, lua_State *L)
{
    int num_ret = 0;

    bool listvalid = true;
    if(errors & SS_THREADS_LIMIT_ERROR)
    {
        listvalid = false;
        mlog(CRITICAL, "Too many rasters to subset, max allowed: %d, limit your AOI/temporal range or use filters", GeoIndexedRaster::MAX_READER_THREADS);
    }

    if(errors & SS_MEMPOOL_ERROR)
    {
        listvalid = false;
        mlog(CRITICAL, "Some rasters could not be subset, requested memory size > max allowed: %ld MB", RasterSubset::MAX_SIZE / (1024 * 1024));
    }

    if(errors & SS_RESOURCE_LIMIT_ERROR)
    {
        listvalid = false;
        mlog(CRITICAL, "System resource limit reached, could not subset rasters");
    }

    /* Create return table */
    lua_createtable(L, slist.length(), 0);
    num_ret++;

    /* Populate subsets */
    if(listvalid && !slist.empty())
    {
        const List<RasterSubset*>::Iterator lit(slist);
        for(int i = 0; i < lit.length; i++)
        {
            const RasterSubset* subset = lit[i];

            /* Populate Return Results */
            lua_createtable(L, 0, 2);
            LuaEngine::setAttrStr(L, "robj", "", 0);  /* For now, figure out how to return RasterObject* */
            LuaEngine::setAttrStr(L, "file",     subset->rasterName.c_str());
            LuaEngine::setAttrInt(L, "size",     subset->getSize());
            LuaEngine::setAttrInt(L, "poolsize", RasterSubset::getPoolSize());
            lua_rawseti(L, -2, i + 1);
        }
    }
    else mlog(DEBUG, "No subsets read");

    return num_ret;
}
