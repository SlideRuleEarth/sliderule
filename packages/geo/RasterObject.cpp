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
#include "Ordering.h"
#include "List.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RasterObject::OBJECT_TYPE  = "RasterObject";
const char* RasterObject::LuaMetaName  = "RasterObject";
const struct luaL_Reg RasterObject::LuaMetaTable[] = {
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
        _parms = (GeoParms*)getLuaObject(L, 1, GeoParms::OBJECT_TYPE);

        /* Get Factory */
        factory_t _create = NULL;
        factoryMut.lock();
        {
            factories.find(_parms->asset_name, &_create);
        }
        factoryMut.unlock();

        /* Check Factory */
        if(_create == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to find registered raster for %s", _parms->asset_name);

        /* Create Raster */
        RasterObject* _raster = _create(L, _parms);
        if(_raster == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create raster of type: %s", _parms->asset_name);

        /* Return Object */
        return createLuaObject(L, _raster);
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * registerDriver
 *----------------------------------------------------------------------------*/
bool RasterObject::registerRaster (const char* _name, factory_t create)
{
    bool status;

    factoryMut.lock();
    {
        status = factories.add(_name, create);
    }
    factoryMut.unlock();

    return status;
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
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    parms(_parms)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "sample", luaSamples);
    LuaEngine::setAttrFunc(L, "subset", luaSubset);
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
    bool status = false;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (RasterObject*)getLuaSelf(L, 1);

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
        std::vector<RasterSample> slist;
        lua_obj->getSamples(lon, lat, height, gps, slist, NULL);

        if(slist.size() > 0)
        {
            /* Create return table */
            lua_createtable(L, slist.size(), 0);

            for(uint32_t i = 0; i < slist.size(); i++)
            {
                const RasterSample& sample = slist[i];
                const char* fileName = "";

                /* Find fileName from fileId */
                Dictionary<uint64_t>::Iterator iterator(lua_obj->fileDictGet());
                for(int j = 0; j < iterator.length; j++)
                {
                    if(iterator[j].value == sample.fileId)
                    {
                        fileName = iterator[j].key;
                        break;
                    }
                }

                lua_createtable(L, 0, 2);
                LuaEngine::setAttrStr(L, "file", fileName);


                if(lua_obj->parms->zonal_stats) /* Include all zonal stats */
                {
                    LuaEngine::setAttrNum(L, "mad", sample.stats.mad);
                    LuaEngine::setAttrNum(L, "stdev", sample.stats.stdev);
                    LuaEngine::setAttrNum(L, "median", sample.stats.median);
                    LuaEngine::setAttrNum(L, "mean", sample.stats.mean);
                    LuaEngine::setAttrNum(L, "max", sample.stats.max);
                    LuaEngine::setAttrNum(L, "min", sample.stats.min);
                    LuaEngine::setAttrNum(L, "count", sample.stats.count);
                }

                if(lua_obj->parms->flags_file) /* Include flags */
                {
                    LuaEngine::setAttrNum(L, "flags", sample.flags);
                }

                LuaEngine::setAttrInt(L, "fileid", sample.fileId);
                LuaEngine::setAttrNum(L, "time", sample.time);
                LuaEngine::setAttrNum(L, "value", sample.value);
                lua_rawseti(L, -2, i+1);
            }
            num_ret++;
            status = true;
        } else mlog(DEBUG, "No samples read for (%.2lf, %.2lf)", lon, lat);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to read samples: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}


/*----------------------------------------------------------------------------
 * luaSubset - :subset(ulx, uly, lrx, lry) --> in|out
 *----------------------------------------------------------------------------*/
int RasterObject::luaSubset(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    RasterObject *lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (RasterObject*)getLuaSelf(L, 1);

        /* Get Coordinates */
        double upleft_x   = getLuaFloat(L, 2);
        double upleft_y   = getLuaFloat(L, 3);
        double lowright_x = getLuaFloat(L, 4);
        double lowright_y = getLuaFloat(L, 5);
        const char* closest_time_str = getLuaString(L, 6, true, NULL);

        /* Get gps closest time (overrides params provided closest time) */
        int64_t gps = 0;
        if(closest_time_str != NULL)
        {
            gps = TimeLib::str2gpstime(closest_time_str);
        }

        /* Get subset */
        std::vector<RasterSubset> slist;
        lua_obj->getSubsets(upleft_x, upleft_y, lowright_x, lowright_y, gps, slist, NULL);

        if(slist.size() > 0)
        {
            /* Create return table */
            lua_createtable(L, slist.size(), 0);

            for(uint32_t i = 0; i < slist.size(); i++)
            {
                const RasterSubset& subset = slist[i];
                const char* fileName = "";

                /* Find fileName from fileId */
                Dictionary<uint64_t>::Iterator iterator(lua_obj->fileDictGet());
                for(int j = 0; j < iterator.length; j++)
                {
                    if(iterator[j].value == subset.fileId)
                    {
                        fileName = iterator[j].key;
                        break;
                    }
                }

                lua_createtable(L, 0, 2);
                LuaEngine::setAttrStr(L, "file", fileName);
                LuaEngine::setAttrInt(L, "fileid", subset.fileId);
                LuaEngine::setAttrNum(L, "time", subset.time);
                LuaEngine::setAttrInt(L, "data", (uint64_t)subset.data);
                LuaEngine::setAttrInt(L, "cols", subset.cols);
                LuaEngine::setAttrInt(L, "rows", subset.rows);
                LuaEngine::setAttrNum(L, "datatype", subset.datatype);
                lua_rawseti(L, -2, i+1);
            }
            num_ret++;
            status = true;
        } else mlog(DEBUG, "No subsets read");
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to subset raster: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}