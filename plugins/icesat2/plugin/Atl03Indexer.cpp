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
#include "icesat2.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Indexer::recType = "atl03rec.index";
const RecordObject::fieldDef_t Atl03Indexer::recDef[] = {
    {"name",    RecordObject::STRING,   offsetof(index_t, name),    Asset::RESOURCE_NAME_LENGTH,    NULL, NATIVE_FLAGS},
    {"t0",      RecordObject::DOUBLE,   offsetof(index_t, t0),      1,                              NULL, NATIVE_FLAGS},
    {"t1",      RecordObject::DOUBLE,   offsetof(index_t, t1),      1,                              NULL, NATIVE_FLAGS},
    {"lat0",    RecordObject::DOUBLE,   offsetof(index_t, lat0),    1,                              NULL, NATIVE_FLAGS},
    {"lon0",    RecordObject::DOUBLE,   offsetof(index_t, lon0),    1,                              NULL, NATIVE_FLAGS},
    {"lat1",    RecordObject::DOUBLE,   offsetof(index_t, lat1),    1,                              NULL, NATIVE_FLAGS},
    {"lon1",    RecordObject::DOUBLE,   offsetof(index_t, lon1),    1,                              NULL, NATIVE_FLAGS},
    {"cycle",   RecordObject::UINT32,   offsetof(index_t, cycle),   1,                              NULL, NATIVE_FLAGS},
    {"rgt",     RecordObject::UINT32,   offsetof(index_t, rgt),     1,                              NULL, NATIVE_FLAGS},
};
const char* Atl03Indexer::OBJECT_TYPE = "Atl03Indexer";
const char* Atl03Indexer::LuaMetaName = "Atl03Indexer";
const struct luaL_Reg Atl03Indexer::LuaMetaTable[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource table>, <outq_name>, [<num threads>])
 *----------------------------------------------------------------------------*/
int Atl03Indexer::luaCreate (lua_State* L)
{
    List<const char*>* _resources = NULL;
    Asset* _asset = NULL;
    try
    {
        /* Get URL */
                    _asset      = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        int         tblindex    = 2;
        const char* outq_name   = getLuaString(L, 3);
        int         num_threads = getLuaInteger(L, 4, true, DEFAULT_NUM_THREADS);

        /* Build Resource Table */
        _resources = new List<const char*>();
        if(lua_type(L, tblindex) == LUA_TTABLE)
        {
            int size = lua_rawlen(L, tblindex);
            for(int e = 0; e < size; e++)
            {
                lua_rawgeti(L, tblindex, e + 1);
                const char* name = StringLib::duplicate(getLuaString(L, -1));
                _resources->add(name);
                lua_pop(L, 1);
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "parm #1 must be a table of resource names");
        }

        /* Return Indexer Object */
        return createLuaObject(L, new Atl03Indexer(L, _asset, _resources, outq_name, num_threads));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Atl03Indexer: %s", e.what());
    }

    /* Clean Up Resources Not Used Since Failed to Create Indexer */
    if(_resources) freeResources(_resources);

    /* Release Asset Since Failed to Create Indexer */
    if(_asset) _asset->releaseLuaObject();

    /* Return Failure */
    return returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Indexer::init (void)
{
    RECDEF(recType, recDef, sizeof(index_t), NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Note:   object takes ownership of _resources list as well as pointers to urls
 *          (const char*) inside the list; responsible for freeing both
 *----------------------------------------------------------------------------*/
Atl03Indexer::Atl03Indexer (lua_State* L, Asset* _asset, List<const char*>* _resources, const char* outq_name, int num_threads):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);
    assert(_resources);

    /* Check Num Threads */
    if(num_threads < 0 or num_threads > MAX_NUM_THREADS)
    {
        mlog(CRITICAL, "Invalid number of threads supplied: %d. Setting to default: %d.", num_threads, DEFAULT_NUM_THREADS);
        num_threads = DEFAULT_NUM_THREADS;
    }

    /* Save Off Asset */
    asset = _asset;

    /* Create Publisher */
    outQ = new Publisher(outq_name);

    /* Initialize Resource List */
    resources = _resources;
    resourceEntry = 0;

    /* Clear Record */
    memset(&indexRec, 0, sizeof(indexRec));

    /* Initialize Indexers */
    active = true;
    numComplete = 0;
    threadCount = num_threads;
    indexerPid = new Thread* [threadCount];
    memset(indexerPid, 0, threadCount * sizeof(Thread*));

    /* Create Indexers */
    for(int t = 0; t < threadCount; t++)
    {
        indexerPid[t] = new Thread(indexerThread, this);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03Indexer::~Atl03Indexer (void)
{
    /* Clean Up Threads */
    active = false;
    for(int t = 0; t < threadCount; t++)
    {
        if(indexerPid[t]) delete indexerPid[t];
    }
    delete [] indexerPid;

    /* Clean Up Publisher */
    delete outQ;

    /* Clean Up Resource List */
    freeResources(resources);

    /* Release Asset */
    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * indexerThread
 *----------------------------------------------------------------------------*/
void* Atl03Indexer::indexerThread (void* parm)
{
    /* Get Thread Info */
    Atl03Indexer* indexer = (Atl03Indexer*)parm;
    bool complete = false;

    /* Start Trace */
    uint32_t trace_id = start_trace(CRITICAL, indexer->traceId, "atl03_indexer", "{\"tag\":\"%s\"}", indexer->getName());
    EventLib::stashId (trace_id); // set thread specific trace id for H5Lib

    /* Build Prefix */
    char prefix[MAX_STR_SIZE];
    StringLib::format(prefix, MAX_STR_SIZE, "%s://%s/", indexer->asset->getFormat(), indexer->asset->getPath());

    /* Initialize Context */
    H5Coro::context_t* context = NULL;

    try
    {
        while(!complete)
        {
            const char* resource_name = NULL;

            /* Get Next Resource in List */
            indexer->resourceMut.lock();
            {
                if(indexer->resourceEntry < indexer->resources->length())
                {
                    resource_name = indexer->resources->get(indexer->resourceEntry);
                    indexer->resourceEntry++;
                }
                else
                {
                    complete = true;
                }
            }
            indexer->resourceMut.unlock();

            /* Index Resource */
            if(resource_name)
            {
                /* Create Context */
                context = new H5Coro::context_t;

                /* Read Data from HDF5 File */
                H5Array<double>     sdp_gps_epoch       (indexer->asset, resource_name, "/ancillary_data/atlas_sdp_gps_epoch", context);
                H5Array<double>     start_delta_time    (indexer->asset, resource_name, "/ancillary_data/start_delta_time", context);
                H5Array<double>     end_delta_time      (indexer->asset, resource_name, "/ancillary_data/end_delta_time", context);
                H5Array<int8_t>     cycle               (indexer->asset, resource_name, "/orbit_info/cycle_number", context);
                H5Array<uint16_t>   rgt                 (indexer->asset, resource_name, "/orbit_info/rgt", context);
                H5Array<double>     gt3r_lat            (indexer->asset, resource_name, "/gt3r/geolocation/reference_photon_lat", context, 0, 0, 1);
                H5Array<double>     gt3r_lon            (indexer->asset, resource_name, "/gt3r/geolocation/reference_photon_lon", context, 0, 0, 1);
                H5Array<double>     gt1l_lat            (indexer->asset, resource_name, "/gt1l/geolocation/reference_photon_lat", context);
                H5Array<double>     gt1l_lon            (indexer->asset, resource_name, "/gt1l/geolocation/reference_photon_lon", context);

                /* Join Reads */
                sdp_gps_epoch.join(H5_READ_TIMEOUT_MS, true);
                start_delta_time.join(H5_READ_TIMEOUT_MS, true);
                end_delta_time.join(H5_READ_TIMEOUT_MS, true);
                cycle.join(H5_READ_TIMEOUT_MS, true);
                rgt.join(H5_READ_TIMEOUT_MS, true);
                gt3r_lat.join(H5_READ_TIMEOUT_MS, true);
                gt3r_lon.join(H5_READ_TIMEOUT_MS, true);
                gt1l_lat.join(H5_READ_TIMEOUT_MS, true);
                gt1l_lon.join(H5_READ_TIMEOUT_MS, true);

                /* Clean Up Context */
                delete context;
                context = NULL;

                /* Allocate Record */
                RecordObject record(recType);
                index_t* index = (index_t*)record.getRecordData();

                /* Copy In Fields */
                StringLib::copy(index->name, resource_name, Asset::RESOURCE_NAME_LENGTH);
                index->t0       = sdp_gps_epoch[0] + start_delta_time[0];
                index->t1       = sdp_gps_epoch[0] + end_delta_time[0];
                index->lat0     = gt3r_lat[0];
                index->lon0     = gt3r_lon[0];  // TODO: verify this vs gt1l_lon
                index->lat1     = gt1l_lat[gt1l_lat.size - 1];
                index->lon1     = gt1l_lon[gt1l_lat.size - 1]; // TODO: verify this vs gt3r_lon
                index->cycle    = cycle[0];
                index->rgt      = rgt[0];

                /* Post Segment Record */
                uint8_t* rec_buf = NULL;
                int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
                int post_status = MsgQ::STATE_ERROR;
                while(indexer->active && (post_status = indexer->outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) <= 0)
                {
                    mlog(DEBUG, "Atl03 indexer failed to post to stream %s: %d", indexer->outQ->getName(), post_status);
                }

                /* Free Record */
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Unable to process resources in %s: %s", indexer->getName(), e.what());
    }

    /* Free Context */
    if(context) delete context;

    /* Count Completion */
    indexer->threadMut.lock();
    {
        indexer->numComplete++;
        if(indexer->numComplete == indexer->threadCount)
        {
            /* Indicate End of Data */
            indexer->outQ->postCopy("", 0);
            indexer->signalComplete();
        }
    }
    indexer->threadMut.unlock();

    /* Stop Trace */
    stop_trace(CRITICAL, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * freeResources
 *----------------------------------------------------------------------------*/
void Atl03Indexer::freeResources (List<const char*>* _resources)
{
    for(int i = 0; i < _resources->length(); i++)
    {
        delete [] _resources->get(i);
    }
    delete _resources;
}

/*----------------------------------------------------------------------------
 * luaStats
 *----------------------------------------------------------------------------*/
int Atl03Indexer::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        Atl03Indexer* lua_obj = (Atl03Indexer*)getLuaSelf(L, 1);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "processed",   lua_obj->resourceEntry);
        LuaEngine::setAttrInt(L, "threads",     lua_obj->threadCount);
        LuaEngine::setAttrInt(L, "completed",   lua_obj->numComplete);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error configuring %s: %s", LuaMetaName, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
