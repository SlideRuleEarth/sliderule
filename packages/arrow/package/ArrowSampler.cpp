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
 * luaCreate - :arrowsampler(request_parms, input_file_path, output_qname, {["mosaic"]: dem1, ["strips"]: dem2})
 *----------------------------------------------------------------------------*/
int ArrowSampler::luaCreate(lua_State* L)
{
    RequestFields* rqst_parms = NULL;
    const char* input_file = NULL;
    const char* outq_name =  NULL;
    std::vector<raster_info_t> user_rasters;

    /* Get Parameters */
    try
    {
        rqst_parms  = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        input_file  = getLuaString(L, 2);
        outq_name   = getLuaString(L, 3);

        /* check if output path is empty */
        if(rqst_parms->output.path.value.empty())
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "output path must be set");
        }

        /* check if the parameter is a table */
        luaL_checktype(L, 4, LUA_TTABLE);

        /* first key for iteration */
        lua_pushnil(L);

        while(lua_next(L, 4) != 0)
        {
            const char*   rkey = getLuaString(L, -2);
            RasterObject* robj = dynamic_cast<RasterObject*>(getLuaObject(L, -1, RasterObject::OBJECT_TYPE));

            if(!rkey) throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid raster key");
            if(!robj) throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid raster object");

            user_rasters.push_back({rkey, robj});

            /* Pop value */
            lua_pop(L, 1);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());

        /* Release Lua Parameters Objects */
        if(rqst_parms) rqst_parms->releaseLuaObject();
        for(raster_info_t& raster : user_rasters)
        {
            raster.robj->releaseLuaObject();
        }
        return returnLuaStatus(L, false);
    }

    /* Create Dispatch */
    try
    {
        return createLuaObject(L, new ArrowSampler(L, rqst_parms, input_file, outq_name, user_rasters));
    }
    catch(const RunTimeException& e)
    {
        /* Constructor releases all Lua objects */
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
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
    delete [] rkey;
    robj->releaseLuaObject();
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
            const double t = TimeLib::latchtime();
            sampler->robj->getSamples(sampler->obj->points, sampler->samples);
            mlog(INFO, "getSamples time: %.3lf", TimeLib::latchtime() - t);

            /* batchSampling can take minutes, check active again */
            if(s->active)
                s->impl->processSamples(sampler);
        }

        /* Release since not needed anymore */
        sampler->samples.clear();
    }

    try
    {
        if(s->active)
        {
            s->impl->createOutpuFiles();

            /* Send Data File to User */
            OutputLib::send2User(s->dataFile, s->parms.path.value.c_str(), trace_id, &s->parms, s->outQ);

            /* Send Metadata File to User */
            if(OutputLib::fileExists(s->metadataFile))
            {
                OutputLib::send2User(s->metadataFile, s->outputMetadataPath, trace_id, &s->parms, s->outQ);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating output file, PARQUET_ARROW reported: %s", e.what());
        stop_trace(INFO, trace_id);
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
const OutputFields* ArrowSampler::getParms(void)
{
    return &parms;
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
ArrowSampler::ArrowSampler(lua_State* L, RequestFields* rqst_parms, const char* input_file,
                           const char* outq_name, const std::vector<raster_info_t>& user_rasters):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    active(false),
    mainPid(NULL),
    rqstParms(rqst_parms),
    parms(rqstParms->output),
    outQ(NULL),
    impl(NULL),
    dataFile(NULL),
    metadataFile(NULL),
    outputMetadataPath(NULL)
{
    /* Validate Parameters */
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
        outputMetadataPath = OutputLib::createMetadataFileName(parms.path.value.c_str());

        /* Create Unique Temporary Filenames */
        dataFile = OutputLib::getUniqueFileName();
        metadataFile = OutputLib::createMetadataFileName(dataFile);

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

    for(batch_sampler_t* sampler : batchSamplers)
    {
        sampler->robj->stopSampling();
        delete sampler;
    }

    delete [] dataFile;
    delete [] metadataFile;
    delete [] outputMetadataPath;
    delete outQ;
    delete impl;

    rqstParms->releaseLuaObject();
}