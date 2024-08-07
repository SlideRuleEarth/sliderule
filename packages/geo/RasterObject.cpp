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
 * cppCreate
 *----------------------------------------------------------------------------*/
RasterObject* RasterObject::cppCreate(GeoParms* _parms)
{
    /* Check Parameters */
    if(!_parms) return NULL;

    /* Get Factory */
    factory_t factory;
    bool found = false;

    factoryMut.lock();
    {
        found = factories.find(_parms->asset_name, &factory);
    }
    factoryMut.unlock();

    /* Check Factory */
    if(!found)
    {
        mlog(CRITICAL, "Failed to find registered raster for %s", _parms->asset_name);
        return NULL;
    }

    /* Create Raster */
    RasterObject* _raster = factory.create(NULL, _parms);
    if(!_raster)
    {
        mlog(CRITICAL, "Failed to create raster for %s", _parms->asset_name);
        return NULL;
    }

    /* Bump Lua Reference (for releasing in destructor) */
    referenceLuaObject(_parms);

    /* Return Raster */
    return _raster;
}

/*----------------------------------------------------------------------------
 * cppCreate
 *----------------------------------------------------------------------------*/
RasterObject* RasterObject::cppCreate(const RasterObject* obj)
{
    return cppCreate(obj->parms);
}

/*----------------------------------------------------------------------------
 * registerDriver
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
 * getPixels
 *----------------------------------------------------------------------------*/
uint8_t* RasterObject::getPixels(uint32_t ulx, uint32_t uly, uint32_t xsize, uint32_t ysize, void* param)
{
    static_cast<void>(ulx);
    static_cast<void>(uly);
    static_cast<void>(xsize);
    static_cast<void>(ysize);
    static_cast<void>(param);
    return NULL;
}

/*----------------------------------------------------------------------------
 * getMaxBatchThreads
 *----------------------------------------------------------------------------*/
uint32_t RasterObject::getMaxBatchThreads(void)
{
    /* Maximum number of batch threads.
     * Each batch thread creates multiple raster reading threads.
     */
    return 16;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RasterObject::~RasterObject(void)
{
    /* Release GeoParms LuaObject */
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * fileDictAdd
 *----------------------------------------------------------------------------*/
uint64_t RasterObject::fileDictAdd(const string& fileName)
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
 * fileDictGetFile
 *----------------------------------------------------------------------------*/
const char* RasterObject::fileDictGetFile (uint64_t fileId)
{
    Dictionary<uint64_t>::Iterator iterator(fileDict);
    for(int i = 0; i < iterator.length; i++)
    {
        if(fileId == iterator[i].value)
            return iterator[i].key;
    }
    return NULL;
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
    LuaEngine::setAttrFunc(L, "sample", luaSamples);
    LuaEngine::setAttrFunc(L, "subset", luaSubsets);
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
            gps = TimeLib::str2gpstime(closest_time_str);
        }

        /* Get samples */
        bool listvalid = true;
        const MathLib::point_3d_t point = {lon, lat, height};
        err = lua_obj->getSamples(point, gps, slist, NULL);

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
            for(int i = 0; i < slist.length(); i++)
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