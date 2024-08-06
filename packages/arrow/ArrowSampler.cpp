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
    std::vector<raster_info_t> rasters;

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

            rasters.push_back({rkey, robj});

            /* Pop value */
            lua_pop(L, 1);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());

        /* Release Lua Parameters Objects */
        if(_parms) _parms->releaseLuaObject();
        for(raster_info_t& raster : rasters)
        {
            raster.robj->releaseLuaObject();
        }
        return returnLuaStatus(L, false);
    }

    /* Create Dispatch */
    try
    {
        return createLuaObject(L, new ArrowSampler(L, _parms, input_file, outq_name, rasters));
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
    delete [] rkey;
    robj->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Reader Constructor
 *----------------------------------------------------------------------------*/
ArrowSampler::Reader::Reader(RasterObject* _robj, ArrowSampler* _obj) :
    robj(_robj),
    obj(_obj),
    range({0, 0})
{
}

/*----------------------------------------------------------------------------
 * Reader Destructor
 *----------------------------------------------------------------------------*/
ArrowSampler::Reader::~Reader(void)
{
    for(ArrowSampler::sample_list_t* slist : samples)
    {
        delete slist;
    }

    delete robj;  /* This is locally created RasterObject, not lua created */
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

    /* Sample all user provided raster objects (data sets) */
    for(batch_sampler_t* sampler : s->batchSamplers)
    {
        batchSampling(sampler);
    }

    try
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
                           const char* outq_name, const std::vector<raster_info_t>& rasters):
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
        for(std::size_t i = 0; i < rasters.size(); i++)
        {
            const raster_info_t& raster = rasters[i];
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
 * getReadersRange
 *----------------------------------------------------------------------------*/
void ArrowSampler::getReadersRange(std::vector<reader_range_t>& ranges, uint32_t maxNumThreads)
{
    const uint32_t minPointsPerThread = 20;

    /* Determine how many reader threads to use and index range for each */
    if(points.size() <= minPointsPerThread)
    {
        ranges.emplace_back(reader_range_t{0, static_cast<uint32_t>(points.size())});
        return;
    }

    uint32_t numThreads = std::min(maxNumThreads, static_cast<uint32_t>(points.size()) / minPointsPerThread);

    /* Ensure at least two threads if points.size() > minPointsPerThread */
    if(numThreads == 1 && maxNumThreads > 1)
    {
        numThreads = 2;
    }

    const uint32_t pointsPerThread = points.size() / numThreads;
    uint32_t remainingPoints = points.size() % numThreads;

    uint32_t start = 0;
    for(uint32_t i = 0; i < numThreads; i++)
    {
        const uint32_t end = start + pointsPerThread + (remainingPoints > 0 ? 1 : 0);
        ranges.emplace_back(reader_range_t{start, end});

        start = end;
        if(remainingPoints > 0)
        {
            remainingPoints--;
        }
    }
}

/*----------------------------------------------------------------------------
 * batchSampling
 *----------------------------------------------------------------------------*/
void ArrowSampler::batchSampling(batch_sampler_t* sampler)
{
    /* Get maximum number of batch processing threads allowed */
    const uint32_t maxNumThreads = sampler->robj->getMaxBatchThreads();

    /* Get readers ranges */
    std::vector<reader_range_t> ranges;
    sampler->obj->getReadersRange(ranges, maxNumThreads);

    for(uint32_t i = 0; i < ranges.size(); i++)
    {
        const reader_range_t& r = ranges[i];
        print2term("%s: ragne-%u: %u to %u\n", sampler->rkey, i, r.start_indx, r.end_indx);
    }

    const uint32_t numThreads = ranges.size();

    /* Start reader threads */
    std::vector<Thread*> pids;
    std::vector<reader_t*> readers;
    for(uint32_t i = 0; i < numThreads; i++)
    {
        /* Create RasterObject for each reader.
         * This is a local object and will be deleted in the reader destructor.
         * RasterObject from the user is not used here.
         */
        RasterObject* _robj = RasterObject::cppCreate(sampler->robj);
        reader_t* reader = new reader_t(_robj, sampler->obj);
        reader->range = ranges[i];
        readers.push_back(reader);
        Thread* pid = new Thread(readerThread, reader);
        pids.push_back(pid);
    }

    /* Wait for all reader threads to finish */
    for(Thread* pid : pids)
    {
        delete pid;
    }

    /* Copy samples lists (pointers only) from each reader. */
    for(const reader_t* reader : readers)
    {
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
                const uint64_t id = sampler->robj->fileDictAdd(name);

                /* Update the sample file id */
                sample->fileId = id;
            }

            /* slist pointer is now in two samples vectors, one in the reader and one in the batch sampler
             * reader's destructor will delete it
             */
            sampler->samples.push_back(slist);
        }
    }


    /* Convert samples into new columns */
    if(sampler->obj->active &&
       sampler->obj->impl->processSamples(sampler))
    {
        /* Create raster file map <id, filename> */
        Dictionary<uint64_t>::Iterator iterator(sampler->robj->fileDictGet());
        for(int i = 0; i < iterator.length; i++)
        {
            const char* name = iterator[i].key;
            const uint64_t id = iterator[i].value;

            /* For some data sets, dictionary contains quality mask rasters in addition to data rasters.
             * Only add rasters with id present in the samples
            */
            if(sampler->file_ids.find(id) != sampler->file_ids.end())
            {
                sampler->filemap.emplace_back(id, name);
            }
        }

        /* Sort the map with increasing file id */
        std::sort(sampler->filemap.begin(), sampler->filemap.end(),
            [](const std::pair<uint64_t, std::string>& a, const std::pair<uint64_t, std::string>& b)
            { return a.first < b.first; });
    }

    /* Clean up readers, deletes cppcreated raster objects */
    for(const reader_t* reader : readers)
    {
        delete reader;
    }

    /* Release since not needed anymore */
    sampler->samples.clear();
    sampler->file_ids.clear();
}

/*----------------------------------------------------------------------------
 * readerThread
 *----------------------------------------------------------------------------*/
void* ArrowSampler::readerThread(void* parm)
{
    reader_t* reader = static_cast<reader_t*>(parm);
    RasterObject* robj = reader->robj;

    const uint32_t start_indx = reader->range.start_indx;
    const uint32_t end_indx   = reader->range.end_indx;

    for(uint32_t i = start_indx; i < end_indx; i++)
    {
        if(!reader->obj->active) break; // early exit if lua object is being destroyed

        point_info_t* pinfo = reader->obj->points[i];

        const MathLib::point_3d_t point = {pinfo->x, pinfo->y, 0.0};
        const double   gps = robj->usePOItime() ? pinfo->gps : 0.0;

        sample_list_t* slist = new sample_list_t;
        bool listvalid = true;
        const uint32_t err = robj->getSamples(point, gps, *slist, NULL);

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
        reader->samples.push_back(slist);
    }

    /* Exit Thread */
    return NULL;
}