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
#include "ArrowSamplerImpl.h"
#include <filesystem>


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
 * luaCreate - :parquetsampler(input_file_path, output_file_path, {["mosaic"]: dem1, ["strips"]: dem2})
 *----------------------------------------------------------------------------*/
int ParquetSampler::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* input_file  = getLuaString(L, 1);
        const char* output_file = getLuaString(L, 2);

        std::vector<raster_info_t> rasters;

        /* Check if the third parameter is a table */
        luaL_checktype(L, 3, LUA_TTABLE);

        /* first key for iteration */
        lua_pushnil(L);

        while(lua_next(L, 3) != 0)
        {
            const char*   rkey = getLuaString(L, -2);
            RasterObject* robj = dynamic_cast<RasterObject*>(getLuaObject(L, -1, RasterObject::OBJECT_TYPE));
            rasters.push_back({rkey, robj});

            /* Pop value */
            lua_pop(L, 1);
        }

        /* Create Dispatch */
        return createLuaObject(L, new ParquetSampler(L, input_file, output_file, rasters));
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
        lua_obj->sample();
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error sampling %s: %s", LUA_META_NAME, e.what());
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
ParquetSampler::Sampler::Sampler (const char* _rkey, RasterObject* _robj, ParquetSampler* _obj):
    robj(_robj),
    obj(_obj)
{
    rkey = StringLib::duplicate(_rkey);
}


/*----------------------------------------------------------------------------
 * Sampler Destructor
 *----------------------------------------------------------------------------*/
ParquetSampler::Sampler::~Sampler (void)
{
    clearSamples();
    delete [] rkey;
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
void ParquetSampler::sample(void)
{
    if(alreadySampled) return;
    alreadySampled = true;

    if(std::filesystem::exists(outputPath))
    {
        int rc = std::remove(outputPath);
        if(rc != 0)
        {
            mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, outputPath, strerror(errno));
        }
    }

    /* Start Sampler Threads */
    for(sampler_t* sampler : samplers)
    {
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
    try
    {
        impl->createParquetFile(inputPath, outputPath);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating parquet file: %s", e.what());
        throw;
    }
}


/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ParquetSampler::ParquetSampler (lua_State* L, const char* input_file, const char* output_file, const std::vector<raster_info_t>& rasters):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    alreadySampled(false)
{
    /* Add Lua sample function */
    LuaEngine::setAttrFunc(L, "sample", luaSample);

    if (!input_file)  throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid input file");
    if (!output_file) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid output file");

    try
    {
        for(std::size_t i = 0; i < rasters.size(); i++)
        {
            const raster_info_t& raster = rasters[i];
            const char* rkey = raster.rkey;
            RasterObject* robj = raster.robj;

            if(!rkey) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid raster key");
            if(!robj) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid raster object");

            sampler_t* sampler = new sampler_t(rkey, robj, this);
            samplers.push_back(sampler);
        }

        inputPath  = StringLib::duplicate(input_file);
        outputPath = StringLib::duplicate(output_file);

        /* Allocate Implementation */
        impl = new ArrowSamplerImpl(this);

        impl->getPointsFromFile(inputPath, points);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ParquetSampler: %s", e.what());
        Delete();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ParquetSampler::~ParquetSampler(void)
{
    Delete();
}

/*----------------------------------------------------------------------------
 * Delete -
 *----------------------------------------------------------------------------*/
void ParquetSampler::Delete(void)
{
    for(Thread* pid : samplerPids)
        delete pid;

    for(sampler_t* sampler : samplers)
        delete sampler;

    for(point_info_t* pinfo : points)
        delete pinfo;

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
    RasterObject* robj = sampler->robj;

    for(point_info_t* pinfo : sampler->obj->points)
    {
        OGRPoint poi = pinfo->point; /* Must make a copy of point for this thread */
        double   gps = robj->usePOItime() ? pinfo->gps_time : 0;

        sample_list_t* slist = new sample_list_t;
        bool listvalid = true;
        uint32_t err = robj->getSamples(&poi, gps, *slist, NULL);

        if(err & SS_THREADS_LIMIT_ERROR)
        {
            listvalid = false;
            mlog(CRITICAL, "Too many rasters to sample");
        }

        if(!listvalid)
        {
            /* Clear sample list */
            for( RasterSample* sample : *slist)
                delete sample;

            /* Clear the list but don't delete it, empty slist indicates no samples for this point */
            slist->clear();
            slist->shrink_to_fit();
        }

        /* Add sample list to sampler */
        sampler->samples.push_back(slist);
    }

    /* Convert samples into new columns */
    if(sampler->obj->impl->processSamples(sampler))
    {
        /* Create raster file map <id, filename> */
        Dictionary<uint64_t>::Iterator iterator(robj->fileDictGet());
        for(int i = 0; i < iterator.length; i++)
        {
            const char* name = iterator[i].key;
            const uint64_t id = iterator[i].value;

            /* For some data sets, dictionary contains quality mask rasters in addition to data rasters.
             * Only add rasters with id present in the samples
            */
            if(sampler->file_ids.find(id) != sampler->file_ids.end())
            {
                sampler->filemap.push_back(std::make_pair(id, name));
            }
        }

        /* Sort the map with increasing file id */
        std::sort(sampler->filemap.begin(), sampler->filemap.end(),
            [](const std::pair<uint64_t, std::string>& a, const std::pair<uint64_t, std::string>& b)
            { return a.first < b.first; });
    }

    /* Release since not needed anymore */
    sampler->clearSamples();
    sampler->file_ids.clear();

    /* Exit Thread */
    return NULL;
}