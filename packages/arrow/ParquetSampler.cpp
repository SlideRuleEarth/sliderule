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
#include "ParquetSampler.h"
#include "ArrowImpl.h"
#include <filesystem>

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ParquetSampler::OBJECT_TYPE = "ParquetSampler";
const char* ParquetSampler::LUA_META_NAME = "ParquetSampler";
const struct luaL_Reg ParquetSampler::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :parquetsampler(<rasters_cnt>, <rasterobj1> <rasterobj2> ... <input_file_path>, <output_file_path>
 *----------------------------------------------------------------------------*/
int ParquetSampler::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* input_file  = getLuaString(L, 1);
        const char* output_file = getLuaString(L, 2);

        std::vector<RasterObject*> raster_objects;

        /* Check if the third parameter is a table */
        luaL_checktype(L, 3, LUA_TTABLE);

        /* first key for iteration */
        lua_pushnil(L);

        while(lua_next(L, 3) != 0)
        {
            /* uses 'key' (at index -2) and 'value' (at index -1) */
            printf("Found element %s\n", lua_tostring(L, -1));

            /* Get Raster Object */
            RasterObject* robj = static_cast<RasterObject*>(getLuaObject(L, -1, RasterObject::OBJECT_TYPE));
            raster_objects.push_back(robj);

            /* removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }

        /* Create Dispatch */
        return createLuaObject(L, new ParquetSampler(L, input_file, output_file, raster_objects));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}


/*----------------------------------------------------------------------------
 * luaSamples - :sample([gps]) --> in|out
 *----------------------------------------------------------------------------*/
int ParquetSampler::luaSample (lua_State* L)
{
    try
    {
        /* Get Self */
        ParquetSampler* lua_obj = dynamic_cast<ParquetSampler*>(getLuaSelf(L, 1));

        const char* closest_time_str = getLuaString(L, 2, true, NULL);

        /* Get gps closest time (overrides params provided closest time) */
        int64_t gps = 0;
        if(closest_time_str != NULL)
        {
            gps = TimeLib::str2gpstime(closest_time_str);
        }

        lua_obj->sample(gps);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error sampling%s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }

    return returnLuaStatus(L, true);
}




/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ParquetSampler::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ParquetSampler::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * Sampler Constructor
 *----------------------------------------------------------------------------*/
ParquetSampler::Sampler::Sampler (RasterObject* _robj, ParquetSampler* _obj):
    robj(_robj),
    gps(0),
    obj(_obj)
{
}


/*----------------------------------------------------------------------------
 * Sampler Destructor
 *----------------------------------------------------------------------------*/
ParquetSampler::Sampler::~Sampler (void)
{
    clearSamples();
    robj->releaseLuaObject();
}


/*----------------------------------------------------------------------------
 * clearSamples
 *----------------------------------------------------------------------------*/
void ParquetSampler::Sampler::clearSamples(void)
{
    for(ParquetSampler::sample_list_t* slist : samples)
    {
        for(RasterSample* sample : *slist)
        {
            delete sample;
        }
        delete slist;
    }
    samples.clear();
}

/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
void ParquetSampler::sample(int64_t gps)
{
    /* Remove outputPath file if it exists */
    if (std::filesystem::exists(outputPath))
    {
        int rc = std::remove(outputPath);
        if (rc != 0)
        {
            mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, outputPath, strerror(errno));
        }
    }

    /* Start Sampler Threads */
    for(sampler_t* sampler : samplers)
    {
        sampler->gps = gps;
        Thread* pid = new Thread(samplerThread, sampler);
        samplerPids.push_back(pid);
    }

    /* Wait for all sampler threads to finish */
    for(Thread* pid : samplerPids)
    {
        delete pid;
    }
    samplerPids.clear();

    /* Create new parquet file with columns/samples from all raster objects */
    impl->createParquetFile(inputPath, outputPath, samplers);

    /* User may call this method with different gps time, clear the samples */
    for(sampler_t* sampler : samplers)
    {
        sampler->clearSamples();
    }
}


/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ParquetSampler::ParquetSampler (lua_State* L, const char* input_file, const char* output_file, const std::vector<RasterObject*>& robjs):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
    /* Add Lua sample function */
    LuaEngine::setAttrFunc(L, "sample", luaSample);

    if (!input_file)  throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid input file");
    if (!output_file) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid output file");

    for(RasterObject* robj : robjs)
    {
        if(!robj) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid raster object");

        sampler_t* sampler = new sampler_t(robj, this);
        samplers.push_back(sampler);
    }

    inputPath  = StringLib::duplicate(input_file);
    outputPath = StringLib::duplicate(output_file);

    /* Allocate Implementation */
    impl = new ArrowImpl(NULL);

    /* Get Points from input geoparquet file */
    impl->getPointsFromFile(inputPath, points);
    mlog(DEBUG, "Input Parquet File: %s with %lu points\n", inputPath, points.size());
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ParquetSampler::~ParquetSampler(void)
{
    for(Thread* pid : samplerPids)
        delete pid;

    for(sampler_t* sampler : samplers)
        delete sampler;

    delete [] inputPath;
    delete [] outputPath;
    delete impl;
}

/*----------------------------------------------------------------------------
 * samplerThread
 *----------------------------------------------------------------------------*/
void* ParquetSampler::samplerThread(void* parm)
{
    sampler_t* sampler = static_cast<sampler_t*>(parm);

    for(auto point : sampler->obj->points)
    {
        OGRPoint poi = point; /* Must make a copy of point for this thread */

        sample_list_t* slist = new sample_list_t;
        bool listvalid = true;
        uint32_t err = sampler->robj->getSamples(&poi, sampler->gps, *slist, NULL);

        if(err & SS_THREADS_LIMIT_ERROR)
        {
            listvalid = false;
            mlog(CRITICAL, "Too many rasters to sample");
        }

        if(!listvalid)
        {
            /* Clear sample list */
            for( auto sample : *slist)
                delete sample;

            /* Clear the list but don't delete it, empty slist indicates no samples for this point */
            slist->clear();
            slist->shrink_to_fit();
        }

        /* Add sample list to sampler */
        sampler->samples.push_back(slist);
    }

    /* Exit Thread */
    return NULL;
}
