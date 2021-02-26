/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "icesat2.h"
#include "core.h"

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
    try
    {
        /* Get URL */
        Asset*      _asset      = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        int         tblindex    = 2;
        const char* outq_name   = getLuaString(L, 3);
        int         num_threads = getLuaInteger(L, 4, true, DEFAULT_NUM_THREADS);

        /* Build Resource Table */
        List<const char*>* _resources = new List<const char*>();
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
            throw RunTimeException("parm #1 must be a table of resource names");
        }

        /* Return Indexer Object */
        return createLuaObject(L, new Atl03Indexer(L, _asset, _resources, outq_name, num_threads));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating Atl03Indexer: %s\n", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Indexer::init (void)
{
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(recType, NULL, sizeof(index_t), recDef, sizeof(recDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", recType, rc);
    }
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
        mlog(CRITICAL, "Invalid number of threads supplied: %d. Setting to default: %d.\n", num_threads, DEFAULT_NUM_THREADS);
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
    LocalLib::set(&indexRec, 0, sizeof(indexRec));

    /* Initialize Indexers */
    active = true;
    numComplete = 0;
    threadCount = num_threads;
    indexerPid = new Thread* [threadCount];
    LocalLib::set(indexerPid, 0, threadCount * sizeof(Thread*));
        
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
    for(int i = resourceEntry; i < resources->length(); i++)
    {
        delete [] resources->get(i);
    }
    delete resources;

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
    uint32_t trace_id = start_trace_ext(indexer->traceId, "atl03_indexer", "{\"tag\":\"%s\"}", indexer->getName());
    TraceLib::stashId (trace_id); // set thread specific trace id for H5Lib

    /* Build Prefix */
    char prefix[MAX_STR_SIZE];
    StringLib::format(prefix, MAX_STR_SIZE, "%s://%s/", indexer->asset->getFormat(), indexer->asset->getUrl());

    /* Initialize Context */
    H5Api::context_t* context = NULL;
    
    try
    {
        while(!complete)
        {
            char url[MAX_STR_SIZE];
            const char* resource_name = NULL;

            /* Get Next Resource in List */            
            indexer->resourceMut.lock();
            {
                if(indexer->resourceEntry < indexer->resources->length())
                {
                    resource_name = indexer->resources->get(indexer->resourceEntry);
                    StringLib::format(url, MAX_STR_SIZE, "%s%s", prefix, resource_name);
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
                context = new H5Api::context_t;

                /* Read Data from HDF5 File */
                H5Array<double>     sdp_gps_epoch       (url, "/ancillary_data/atlas_sdp_gps_epoch", context);
                H5Array<double>     start_delta_time    (url, "/ancillary_data/start_delta_time", context);
                H5Array<double>     end_delta_time      (url, "/ancillary_data/end_delta_time", context);
                H5Array<int8_t>     cycle               (url, "/orbit_info/cycle_number", context);
                H5Array<uint16_t>   rgt                 (url, "/orbit_info/rgt", context);
                H5Array<double>     gt3r_lat            (url, "/gt3r/geolocation/reference_photon_lat", context, 0, 0, 1);
                H5Array<double>     gt3r_lon            (url, "/gt3r/geolocation/reference_photon_lon", context, 0, 0, 1);
                H5Array<double>     gt1l_lat            (url, "/gt1l/geolocation/reference_photon_lat", context);
                H5Array<double>     gt1l_lon            (url, "/gt1l/geolocation/reference_photon_lon", context);

                /* Clean Up Context */
                delete context;
                context = NULL;

                /* Allocate Record */
                RecordObject* record = new RecordObject(recType);
                index_t* index = (index_t*)record->getRecordData();

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
                int rec_bytes = record->serialize(&rec_buf, RecordObject::REFERENCE);
                int post_status = MsgQ::STATE_ERROR;
                while(indexer->active && (post_status = indexer->outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) <= 0)
                {
                    mlog(DEBUG, "Atl03 indexer failed to post to stream %s: %d\n", indexer->outQ->getName(), post_status);
                }
            }
        }
    }
    catch(const std::exception& e)
    {
        mlog(CRITICAL, "Unable to process resources in %s: %s\n", indexer->getName(), e.what());
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
    stop_trace(trace_id);

    /* Return */
    return NULL;
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
        mlog(CRITICAL, "Error configuring %s: %s\n", LuaMetaName, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
