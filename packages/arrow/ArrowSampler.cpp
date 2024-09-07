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
    const char* input_file = NULL;
    const char* outq_name =  NULL;
    std::vector<raster_info_t> user_rasters;

    /* Get Parameters */
    try
    {
        _parms      = dynamic_cast<ArrowParms*>(getLuaObject(L, 1, ArrowParms::OBJECT_TYPE));
        input_file  = getLuaString(L, 2);
        outq_name   = getLuaString(L, 3);

        /* Check if the parameter is a table */
        luaL_checktype(L, 4, LUA_TTABLE);

        /* first key for iteration */
        lua_pushnil(L);

        while(lua_next(L, 4) != 0)
        {
            const char*   rkey = getLuaString(L, -2);
            RasterObject* robj = dynamic_cast<RasterObject*>(getLuaObject(L, -1, RasterObject::OBJECT_TYPE));

            if(!rkey) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid raster key");
            if(!robj) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid raster object");

            user_rasters.push_back({rkey, robj});

            /* Pop value */
            lua_pop(L, 1);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());

        /* Release Lua Parameters Objects */
        if(_parms) _parms->releaseLuaObject();
        for(raster_info_t& raster : user_rasters)
        {
            raster.robj->releaseLuaObject();
        }
        return returnLuaStatus(L, false);
    }

    /* Create Dispatch */
    try
    {
        return createLuaObject(L, new ArrowSampler(L, _parms, input_file, outq_name, user_rasters));
    }
    catch(const RunTimeException& e)
    {
        /* Constructor releases all Lua objects */
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
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
ArrowSampler::BatchSampler::BatchSampler(const char* _rkey, RasterObject* _robj, ArrowSampler* _obj) :
    robj(_robj),
    obj(_obj)
{
    rkey = StringLib::duplicate(_rkey);
}


/*----------------------------------------------------------------------------
 * Sampler Destructor
 *----------------------------------------------------------------------------*/
ArrowSampler::BatchSampler::~BatchSampler(void)
{
    clearSamples();
    delete [] rkey;
    robj->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * clearSamples
 *----------------------------------------------------------------------------*/
void ArrowSampler::BatchSampler::clearSamples(void)
{
    for(sample_list_t* slist : samples)
    {
        delete slist;
    }
    samples.clear();
}

/*----------------------------------------------------------------------------
 * mainThread
 *----------------------------------------------------------------------------*/
void* ArrowSampler::mainThread(void* parm)
{
    ArrowSampler* s = reinterpret_cast<ArrowSampler*>(parm);

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, s->traceId, "arrow_sampler", "{\"filename\":\"%s\"}", s->dataFile);
    EventLib::stashId(trace_id);

    /* Get samples for all user RasterObjects */
    for(batch_sampler_t* sampler : s->batchSamplers)
    {
        if(s->active)
        {
            const uint32_t error_status = sampler->robj->getSamples(sampler->obj->points, sampler->samples);
            mlog(INFO, "getSamples for %s returned %u", sampler->rkey, error_status);

            /* batchSampling can take minutes, check active again */
            if(s->active)
                s->impl->processSamples(sampler);
        }

        /* Release since not needed anymore */
        sampler->clearSamples();
        sampler->file_ids.clear();
    }

    try
    {
        if(s->active)
        {
            s->impl->createOutpuFiles();

            /* Send Data File to User */
            ArrowCommon::send2User(s->dataFile, s->outputPath, trace_id, s->parms, s->outQ);

            /* Send Metadata File to User */
            if(ArrowCommon::fileExists(s->metadataFile))
            {
                ArrowCommon::send2User(s->metadataFile, s->outputMetadataPath, trace_id, s->parms, s->outQ);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating output file: %s", e.what());
        stop_trace(INFO, trace_id);
        throw;
    }

    /* Signal Completion */
    s->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    return NULL;
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
 * getBatchSamplers
 *----------------------------------------------------------------------------*/
const std::vector<ArrowSampler::batch_sampler_t*>& ArrowSampler::getBatchSamplers(void)
{
    return batchSamplers;
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowSampler::ArrowSampler(lua_State* L, ArrowParms* _parms, const char* input_file,
                           const char* outq_name, const std::vector<raster_info_t>& user_rasters):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    active(false),
    mainPid(NULL),
    parms(_parms),
    outQ(NULL),
    impl(NULL),
    dataFile(NULL),
    metadataFile(NULL),
    outputPath(NULL),
    outputMetadataPath(NULL)
{
    /* Validate Parameters */
    assert(parms);
    assert(input_file);
    assert(outq_name);

    try
    {
        /* Copy user raster objects, create batch samplers */
        for(std::size_t i = 0; i < user_rasters.size(); i++)
        {
            const raster_info_t& raster = user_rasters[i];
            const char* rkey = raster.rkey;
            RasterObject* robj = raster.robj;
            batch_sampler_t* sampler = new batch_sampler_t(rkey, robj, this);
            batchSamplers.push_back(sampler);
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

        /* Start Main Thread */
        active = true;
        mainPid = new Thread(mainThread, this);
    }
    catch(const RunTimeException& e)
    {
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
    active = false;
    delete mainPid;

    parms->releaseLuaObject();

    for(batch_sampler_t* sampler : batchSamplers)
        delete sampler;

    delete [] dataFile;
    delete [] metadataFile;
    delete [] outputPath;
    delete [] outputMetadataPath;
    delete outQ;
    delete impl;
}