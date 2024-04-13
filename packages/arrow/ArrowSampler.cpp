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
#include "ArrowSampler.h"
#include "ArrowSamplerImpl.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowSampler::OBJECT_TYPE   = "ArrowSampler";
const char* ArrowSampler::LUA_META_NAME = "ArrowSampler";
const struct luaL_Reg ArrowSampler::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :arrowsampler(input_file_path, output_file_path, {["mosaic"]: dem1, ["strips"]: dem2})
 *----------------------------------------------------------------------------*/
int ArrowSampler::luaCreate(lua_State* L)
{
    ArrowParms* _parms = NULL;

    try
    {
        /* Get Parameters */
        _parms                  = dynamic_cast<ArrowParms*>(getLuaObject(L, 1, ArrowParms::OBJECT_TYPE));
        const char* input_file  = getLuaString(L, 2);
        const char* outq_name   = getLuaString(L, 3);

        std::vector<raster_info_t> rasters;

        /* Check if the parameter is a table */
        luaL_checktype(L, 4, LUA_TTABLE);

        /* first key for iteration */
        lua_pushnil(L);

        while(lua_next(L, 4) != 0)
        {
            const char*   rkey = getLuaString(L, -2);
            RasterObject* robj = dynamic_cast<RasterObject*>(getLuaObject(L, -1, RasterObject::OBJECT_TYPE));
            rasters.push_back({rkey, robj});

            /* Pop value */
            lua_pop(L, 1);
        }

        /* Create Dispatch */
        return createLuaObject(L, new ArrowSampler(L, _parms, input_file, outq_name, rasters));
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
int ArrowSampler::luaSample(lua_State* L)
{
    try
    {
        /* Get Self */
        ArrowSampler* lua_obj = dynamic_cast<ArrowSampler*>(getLuaSelf(L, 1));
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
void ArrowSampler::init(void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ArrowSampler::deinit(void)
{
}

/*----------------------------------------------------------------------------
 * Sampler Constructor
 *----------------------------------------------------------------------------*/
ArrowSampler::Sampler::Sampler(const char* _rkey, RasterObject* _robj, ArrowSampler* _obj) :
    robj(_robj),
    obj(_obj)
{
    rkey = StringLib::duplicate(_rkey);
}


/*----------------------------------------------------------------------------
 * Sampler Destructor
 *----------------------------------------------------------------------------*/
ArrowSampler::Sampler::~Sampler(void)
{
    clearSamples();
    delete [] rkey;
    robj->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * clearSamples
 *----------------------------------------------------------------------------*/
void ArrowSampler::Sampler::clearSamples(void)
{
    for(ArrowSampler::sample_list_t* slist : samples)
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
void ArrowSampler::sample(void)
{
    if(alreadySampled) return;
    alreadySampled = true;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, traceId, "arrow_sampler", "{\"filename\":\"%s\"}", dataFile);
    EventLib::stashId(trace_id);

    /* Start sampling threads */
    for(sampler_t* sampler : samplers)
    {
        Thread* pid = new Thread(samplerThread, sampler);
        samplerPids.push_back(pid);
    }

    /* Wait for all sampling threads to finish */
    for(Thread* pid : samplerPids)
    {
        delete pid;
    }
    samplerPids.clear();

    try
    {
        impl->createOutpuFiles();

        /* Send Data File to User */
        ArrowCommon::send2User(dataFile, outputPath, trace_id, parms, outQ);
        ArrowCommon::removeFile(dataFile);

        /* Send Metadata File to User */
        if(ArrowCommon::fileExists(metadataFile))
        {
            ArrowCommon::send2User(metadataFile, outputMetadataPath, trace_id, parms, outQ);
            ArrowCommon::removeFile(metadataFile);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating output file: %s", e.what());
        stop_trace(INFO, trace_id);
        throw;
    }

    stop_trace(INFO, trace_id);
}

/*----------------------------------------------------------------------------
 * getParms
 *----------------------------------------------------------------------------*/
const ArrowParms* ArrowSampler::getParms(void)
{
    return parms;
}

/*----------------------------------------------------------------------------
 * getDataFile
 *----------------------------------------------------------------------------*/
const char* ArrowSampler::getDataFile(void)
{
    return dataFile;
}

/*----------------------------------------------------------------------------
 * getMetadataFile
 *----------------------------------------------------------------------------*/
const char* ArrowSampler::getMetadataFile(void)
{
    return metadataFile;
}

/*----------------------------------------------------------------------------
 * getSamplers
 *----------------------------------------------------------------------------*/
const std::vector<ArrowSampler::sampler_t*>& ArrowSampler::getSamplers(void)
{
    return samplers;
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowSampler::ArrowSampler(lua_State* L, ArrowParms* _parms, const char* input_file,
                           const char* outq_name, const std::vector<raster_info_t>& rasters):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    metadataFile(NULL),
    alreadySampled(false)
{
    /* Add Lua sample function */
    LuaEngine::setAttrFunc(L, "sample", luaSample);

    if (parms == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid ArrowParms object");

    if((parms->path == NULL) || (parms->path[0] == '\0'))
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid output file path");

    if ((input_file == NULL) || (input_file[0] == '\0'))
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid input file path");

    if ((outq_name == NULL) || (outq_name[0] == '\0'))
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid input queue name");

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

        /* Allocate Implementation */
        impl = new ArrowSamplerImpl(this);

        /* Get Paths */
        outputPath = ArrowCommon::getOutputPath(parms);
        outputMetadataPath = ArrowCommon::createMetadataFileName(outputPath);

        /* Create Unique Temporary Filenames */
        dataFile = ArrowCommon::getUniqueFileName();
        metadataFile = ArrowCommon::createMetadataFileName(dataFile);

        /* Initialize Queues */
        const int qdepth = 0x4000000;   // 64MB
        outQ = new Publisher(outq_name, Publisher::defaultFree, qdepth);

        /* Process Input File */
        impl->processInputFile(input_file, points);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ArrowSampler: %s", e.what());
        Delete();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ArrowSampler::~ArrowSampler(void)
{
    Delete();
}

/*----------------------------------------------------------------------------
 * Delete -
 *----------------------------------------------------------------------------*/
void ArrowSampler::Delete(void)
{
    parms->releaseLuaObject();

    for(Thread* pid : samplerPids)
        delete pid;

    for(sampler_t* sampler : samplers)
        delete sampler;

    for(point_info_t* pinfo : points)
        delete pinfo;

    delete [] dataFile;
    delete [] metadataFile;
    delete [] outputPath;
    delete [] outputMetadataPath;
    delete outQ;
    delete impl;
}

/*----------------------------------------------------------------------------
 * samplerThread
 *----------------------------------------------------------------------------*/
void* ArrowSampler::samplerThread(void* parm)
{
    sampler_t* sampler = static_cast<sampler_t*>(parm);
    RasterObject* robj = sampler->robj;

    for(point_info_t* pinfo : sampler->obj->points)
    {
        OGRPoint poi(pinfo->x, pinfo->y, 0.0);
        double   gps = robj->usePOItime() ? pinfo->gps : 0.0;

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