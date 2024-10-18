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
Mutex RasterObject::fileDictMut;

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
 * getSamples
 *----------------------------------------------------------------------------*/
uint32_t RasterObject::getSamples(const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param)
{
    static_cast<void>(param);
    uint32_t ssErrors = SS_NO_ERRORS;

    samplingMut.lock();
    try
    {
        /* Get maximum number of batch processing threads allowed */
        const uint32_t maxNumThreads = getMaxBatchThreads();

        /* Get readers ranges */
        std::vector<range_t> ranges;
        getThreadsRanges(ranges, points.size(), 5, maxNumThreads);

        for(uint32_t i = 0; i < ranges.size(); i++)
        {
            const range_t& range = ranges[i];
            mlog(DEBUG, "ragne-%u: %u to %u", i, range.start, range.end);
        }

        const uint32_t numThreads = ranges.size();
        mlog(INFO, "Number of reader threads: %u", numThreads);

        if(numThreads == 1)
        {
            /* Single thread, read all samples in one thread using this RasterObject */
            std::vector<sample_list_t*> samples;
            ssErrors = readSamples(this, ranges[0], points, samples);
            for(sample_list_t* slist : samples)
            {
                sllist.add(slist);
            }
        }
        else
        {
            /* Start reader threads */
            std::vector<Thread*> pids;

            for(uint32_t i = 0; i < numThreads; i++)
            {
                /* Create a RasterObject for each reader thread.
                 * These objects are local and will be deleted in the reader destructor.
                 * The user's (this) RasterObject is not directly used for sampling; it is used to accumulate samples from all readers.
                 */
                RasterObject* _robj = RasterObject::cppCreate(this);
                reader_t* reader = new reader_t(_robj, points);
                reader->range = ranges[i];
                readersMut.lock();
                {
                    readers.push_back(reader);
                }
                readersMut.unlock();
                Thread* pid = new Thread(readerThread, reader);
                pids.push_back(pid);
            }

            /* Wait for all reader threads to finish */
            for(Thread* pid : pids)
            {
                delete pid;
            }

            /* Copy samples lists (slist pointers only) from each reader. */
            for(const reader_t* reader : readers)
            {
                /* Acumulate errors from all reader threads */
                ssErrors |= reader->ssErrors;

                for(sample_list_t* slist : reader->samples)
                {
                    for(int32_t i = 0; i < slist->length(); i++)
                    {
                        /* NOTE: sample.fileId is an index of the file name in the reader's file dictionary.
                         *        we need to convert it to the index in the batch sampler's dictionary (user's RasterObject dict).
                         */
                        RasterSample* sample = slist->get(i);

                        /* Find the file name for the sample id in reader's dictionary */
                        const char* name = reader->robj->fileDictGetFile(sample->fileId);

                        /* Use user's RasterObject dictionary to store the file names. */
                        const uint64_t id = fileDictAdd(name);

                        /* Update the sample file id */
                        sample->fileId = id;
                    }

                    sllist.add(slist);
                }
            }

            /* Clear raders */
            readersMut.lock();
            {
                for(const reader_t* reader : readers)
                    delete reader;

                readers.clear();
            }
            readersMut.unlock();

        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }
    samplingMut.unlock();

    return ssErrors;
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
     * Each batch thread may create multiple raster reading threads.
     */
    const uint32_t maxThreads = 16;
    return std::min(std::thread::hardware_concurrency(), maxThreads);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RasterObject::~RasterObject(void)
{
    /* Release GeoParms LuaObject */
    parms->releaseLuaObject();
}

void RasterObject::stopSampling(void)
{
    samplingEnabled = false;
    readersMut.lock();
    {
        for(const reader_t* reader : readers)
            reader->robj->stopSampling();
    }
    readersMut.unlock();
}

/*----------------------------------------------------------------------------
 * fileDictAdd
 *----------------------------------------------------------------------------*/
uint64_t RasterObject::fileDictAdd(const string& fileName)
{
    uint64_t id;

    fileDictMut.lock();
    {
        if(!fileDict.find(fileName.c_str(), &id))
        {
            id = (parms->key_space << 32) | fileDict.length();
            fileDict.add(fileName.c_str(), id);
        }
    }
    fileDictMut.unlock();
    return id;
}

/*----------------------------------------------------------------------------
 * fileDictGetFile
 *----------------------------------------------------------------------------*/
const char* RasterObject::fileDictGetFile (uint64_t fileId)
{
    const char* fileName = NULL;
    fileDictMut.lock();
    {
        Dictionary<uint64_t>::Iterator iterator(fileDict);
        for(int i = 0; i < iterator.length; i++)
        {
            if(fileId == iterator[i].value)
            {
                fileName = iterator[i].key;
                break;
            }
        }
    }
    fileDictMut.unlock();
    return fileName;
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

    uint32_t numThreads = std::min(maxNumThreads, num / minPerThread);

    /* Ensure at least two threads if num > minPerThread */
    if(numThreads == 1 && maxNumThreads > 1)
    {
        numThreads = 2;
    }

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

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterObject::RasterObject(lua_State *L, GeoParms* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    samplingEnabled(true)
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
 * Reader Constructor
 *----------------------------------------------------------------------------*/
RasterObject::Reader::Reader(RasterObject* _robj, const std::vector<RasterObject::point_info_t>& _points) :
    robj(_robj),
    range({0, 0}),
    points(_points),
    ssErrors(SS_NO_ERRORS)
{
}

/*----------------------------------------------------------------------------
 * Reader Destructor
 *----------------------------------------------------------------------------*/
RasterObject::Reader::~Reader(void)
{
    delete robj;  /* This is locally created RasterObject, not lua created */
}



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

/*----------------------------------------------------------------------------
 * readerThread
 *----------------------------------------------------------------------------*/
void* RasterObject::readerThread(void* parm)
{
    reader_t* reader = static_cast<reader_t*>(parm);
    reader->ssErrors = readSamples(reader->robj, reader->range, reader->points, reader->samples);

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * readSamples
 *----------------------------------------------------------------------------*/
uint32_t RasterObject::readSamples(RasterObject* robj, const range_t& range,
                                   const std::vector<point_info_t>& points,
                                   std::vector<sample_list_t*>& samples)
{
    uint32_t ssErrors = SS_NO_ERRORS;

    for(uint32_t i = range.start; i < range.end; i++)
    {
        if(!robj->sampling())
        {
            mlog(DEBUG, "Sampling stopped");
            samples.clear();
            break;
        }

        const RasterObject::point_info_t& pinfo = points[i];
        const MathLib::point_3d_t& point = pinfo.point;
        const int64_t gps = robj->usePOItime() ? pinfo.gps : 0.0;

        sample_list_t* slist = new sample_list_t;
        bool listvalid = true;
        const uint32_t err = robj->getSamples(point, gps, *slist, NULL);

        /* Acumulate errors from all getSamples calls */
        ssErrors |= err;

        if(err & SS_THREADS_LIMIT_ERROR)
        {
            listvalid = false;
            mlog(CRITICAL, "Too many rasters to sample");
        }

        if(!listvalid)
        {
            /* Clear the list but don't delete it, empty slist indicates no samples for this point */
            slist->clear();
        }

        /* Add sample list */
        samples.push_back(slist);
    }

    return ssErrors;
}

