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

#include <math.h>
#include <float.h>
#include <stdarg.h>

#include "OsApi.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "Icesat2Fields.h"
#include "Atl03Viewer.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Viewer::segRecType = "atl03vrec.segments";
const RecordObject::fieldDef_t Atl03Viewer::segRecDef[] = {
    {"time",            RecordObject::TIME8,    offsetof(segment_t, time_ns),        1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"extent_id",       RecordObject::UINT64,   offsetof(segment_t, extent_id),      1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"latitude",        RecordObject::DOUBLE,   offsetof(segment_t, latitude),       1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",       RecordObject::DOUBLE,   offsetof(segment_t, longitude),      1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"segment_dist_x",  RecordObject::DOUBLE,   offsetof(segment_t, dist_x),         1,  NULL, NATIVE_FLAGS},
    {"segment_id",      RecordObject::UINT32,   offsetof(segment_t, id),             1,  NULL, NATIVE_FLAGS},
    {"segment_ph_cnt",  RecordObject::UINT32,   offsetof(segment_t, ph_cnt),         1,  NULL, NATIVE_FLAGS}
};

const char* Atl03Viewer::batchRecType = "atl03vrec";
const RecordObject::fieldDef_t Atl03Viewer::batchRecDef[] = {
    {"region",          RecordObject::UINT8,    offsetof(extent_t, region),                 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"track",           RecordObject::UINT8,    offsetof(extent_t, track),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"pair",            RecordObject::UINT8,    offsetof(extent_t, pair),                   1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"spot",            RecordObject::UINT8,    offsetof(extent_t, spot),                   1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"rgt",             RecordObject::UINT16,   offsetof(extent_t, reference_ground_track), 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"cycle",           RecordObject::UINT8,    offsetof(extent_t, cycle),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"segments",        RecordObject::USER,     offsetof(extent_t, segments),               0,  segRecType, NATIVE_FLAGS | RecordObject::BATCH} // variable length
};

const char* Atl03Viewer::OBJECT_TYPE = "Atl03Viewer";
const char* Atl03Viewer::LUA_META_NAME = "Atl03Viewer";
const struct luaL_Reg Atl03Viewer::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 VIEWER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl03Viewer::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 3, true, true);

        /* Check for Null Resource and Asset */
        if(_parms->resource.value.empty()) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply a resource to process");
        else if(_parms->asset.asset == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply a valid asset");

        /* Return Viewer Object */
        return createLuaObject(L, new Atl03Viewer(L, outq_name, _parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating Atl03Viewer: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Viewer::init (void)
{
    RECDEF(segRecType,      segRecDef,      sizeof(segment_t),      NULL);
    RECDEF(batchRecType,    batchRecDef,    sizeof(extent_t),       NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::Atl03Viewer (lua_State* L, const char* outq_name, Icesat2Fields* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->readTimeout.value * 1000),
    context(NULL)
{
    assert(outq_name);
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Save Info */
    parms = _parms;

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.segments_read     = 0;
    stats.extents_filtered  = 0;
    stats.extents_dropped   = 0;
    stats.extents_retried   = 0;

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    memset(readerPid, 0, sizeof(readerPid));

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Read Global Resource Information */
    try
    {
        /* Create H5Coro Context */
        context = new H5Coro::Context(parms->asset.asset, parms->getResource());

        /* Create Readers */
        for(int track = 1; track <= Icesat2Fields::NUM_TRACKS; track++)
        {
            for(int pair = 0; pair < Icesat2Fields::NUM_PAIR_TRACKS; pair++)
            {
                const int gt_index = (2 * (track - 1)) + pair;
                if(parms->beams.values[gt_index] && (parms->track == Icesat2Fields::ALL_TRACKS || track == parms->track))
                {
                    info_t* info = new info_t;
                    info->reader = this;
                    info->track = track;
                    info->pair = pair;
                    StringLib::format(info->prefix, 7, "/gt%d%c", info->track, info->pair == 0 ? 'l' : 'r');
                    readerPid[threadCount++] = new Thread(subsettingThread, info);
                }
            }
        }

        /* Check if Readers Created */
        if(threadCount == 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "No reader threads were created, invalid track specified: %d\n", parms->track.value);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, outQ, &active, "Failure on resource %s: %s", parms->getResource(), e.what());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, outQ, &active, "Failure on resource %s: %s", parms->getResource(), e.what());

        /* Indicate End of Data */
        if(sendTerminator) outQ->postCopy("", 0, SYS_TIMEOUT);
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::~Atl03Viewer (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete outQ;

    parms->releaseLuaObject();

    delete context;
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::Region::Region (const info_t* info):
    segment_lat    (info->reader->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str()),
    segment_lon    (info->reader->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str()),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        segment_lat.join(info->reader->read_timeout_ms, true);
        segment_lon.join(info->reader->read_timeout_ms, true);
        segment_ph_cnt.join(info->reader->read_timeout_ms, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(info->reader->parms->regionMask.valid())
        {
            rasterregion(info);
        }
        else if(info->reader->parms->pointsInPolygon.value > 0)
        {
            polyregion(info);
        }
        else
        {
            num_segments = segment_ph_cnt.size;
        }

        /* Check If Anything to Process */
        if(num_segments <= 0)
        {
            throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        segment_lat.trim(first_segment);
        segment_lon.trim(first_segment);
        segment_ph_cnt.trim(first_segment);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl03Viewer::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl03Viewer::Region::polyregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_lat.size)
    {
        /* Test Inclusion */
        const bool inclusion = info->reader->parms->polyIncludes(segment_lon[segment], segment_lat[segment]);

        /* Segments with zero photon count may contain invalid coordinates,
           making them unsuitable for inclusion in polygon tests. */

        /* Check First Segment */
        if(!first_segment_found)
        {
            /* If Coordinate Is In Polygon */
            if(inclusion && segment_ph_cnt[segment] > 0)
            {
                /* Set First Segment */
                first_segment_found = true;
                first_segment = segment;
            }
        }
        else
        {
            /* If Coordinate Is NOT In Polygon */
            if(!inclusion && segment_ph_cnt[segment] > 0)
            {
                break; // full extent found!
            }
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        num_segments = segment - first_segment;
    }
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void Atl03Viewer::Region::rasterregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;

    /* Check Size */
    if(segment_lat.size <= 0)
    {
        return;
    }

    /* Allocate Inclusion Mask */
    inclusion_mask = new bool [segment_lat.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    long last_segment = 0;
    int segment = 0;
    while(segment < segment_lat.size)
    {
        /* Check Inclusion */
        const bool inclusion = info->reader->parms->maskIncludes(segment_lon[segment], segment_lat[segment]);
        inclusion_mask[segment] = inclusion;

        /* Check For First Segment */
        if(!first_segment_found && inclusion)
        {
            /* If Coordinate Is In Raster */
            first_segment_found = true;
            first_segment = segment;
            last_segment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            /* Update Number of Segments to Current Segment Count */
            last_segment = segment;
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        num_segments = last_segment - first_segment + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_segment];
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Constructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::Atl03Data::Atl03Data (const info_t* info, const Region& region):
    sc_orient           (info->reader->context,                                "/orbit_info/sc_orient"),
    segment_delta_time  (info->reader->context, FString("%s/%s", info->prefix, "geolocation/delta_time").c_str(),       0, region.first_segment, region.num_segments),
    segment_id          (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_id").c_str(),       0, region.first_segment, region.num_segments),
    segment_dist_x      (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_dist_x").c_str(),   0, region.first_segment, region.num_segments)
{
    /* Join Hardcoded Reads */
    sc_orient.join(info->reader->read_timeout_ms, true);
    segment_delta_time.join(info->reader->read_timeout_ms, true);
    segment_id.join(info->reader->read_timeout_ms, true);
    segment_dist_x.join(info->reader->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::Atl03Data::~Atl03Data (void) = default;

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl03Viewer::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Atl03Viewer* reader = info->reader;
    Icesat2Fields* parms = info->reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "atl03_viewsubsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", parms->asset.getName(), parms->getResource(), info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(info);

        /* Read ATL03 Datasets */
        const Atl03Data atl03(info, region);

        /* Increment Read Statistics */
        local_stats.segments_read = region.num_segments;

        List<segment_t> segments;

        const uint32_t max_segments_per_extent = 256;

        /* Loop Through Each Segment */
        for(long s = 0; reader->active && s < region.num_segments; s++)
        {
            /* Skip segments with zero photon count */
            if(region.segment_ph_cnt[s] == 0) continue;

            const segment_t segment = {
                .time_ns   = Icesat2Fields::deltatime2timestamp(atl03.segment_delta_time[s]),
                .extent_id = Icesat2Fields::generateExtentId(parms->granuleFields.rgt.value, parms->granuleFields.cycle.value, parms->granuleFields.region.value, info->track, info->pair, s),
                .latitude  = region.segment_lat[s],
                .longitude = region.segment_lon[s],
                .dist_x    = atl03.segment_dist_x[s],
                .id        = static_cast<uint32_t>(atl03.segment_id[s]),
                .ph_cnt    = static_cast<uint32_t>(region.segment_ph_cnt[s])
            };
            segments.add(segment);

            const bool last_segment = (s == local_stats.segments_read - 1);

            if(segments.length() % max_segments_per_extent == 0 || last_segment)
            {
                /* Calculate Extent Record Size */
                const int batch_bytes = offsetof(extent_t, segments) + (sizeof(segment_t) * segments.length());

                /* Initialize Extent Record */
                RecordObject record (batchRecType, batch_bytes);
                extent_t* extent = reinterpret_cast<extent_t*>(record.getRecordData());
                extent->region = parms->granuleFields.region.value;
                extent->track = info->track;
                extent->pair = info->pair;
                extent->spot = Icesat2Fields::getSpotNumber(static_cast<Icesat2Fields::sc_orient_t>(atl03.sc_orient[0]),
                                                           static_cast<Icesat2Fields::track_t>(info->track), info->pair);
                extent->reference_ground_track = parms->granuleFields.rgt.value;
                extent->cycle = parms->granuleFields.cycle.value;

                /* Populate Segments */
                for(int32_t i = 0; i < segments.length(); i++)
                {
                    extent->segments[i] = segments.get(i);
                }

                reader->postRecord(record, local_stats);

                /* Reset Segment List */
                segments.clear();
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s track %d.%d: %s", parms->getResource(), info->track, info->pair, e.what());
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Update Statistics */
        reader->stats.segments_read += local_stats.segments_read;
        reader->stats.extents_filtered += local_stats.extents_filtered;
        reader->stats.extents_sent += local_stats.extents_sent;
        reader->stats.extents_dropped += local_stats.extents_dropped;
        reader->stats.extents_retried += local_stats.extents_retried;

        /* Count Completion */
        reader->numComplete++;
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s track %d.%d (r: %u, s: %u)", parms->getResource(), info->track, info->pair, local_stats.segments_read, local_stats.extents_sent);

            /* Indicate End of Data */
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s track %d.%d", status, parms->getResource(), info->track, info->pair);
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s track %d.%d ... trying again", parms->getResource(), info->track, info->pair);
                    }
                }
            }
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up Info */
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * postRecord
 *----------------------------------------------------------------------------*/
void Atl03Viewer::postRecord (RecordObject& record, stats_t& local_stats)
{
    uint8_t* rec_buf = NULL;
    const int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
    int post_status = MsgQ::STATE_TIMEOUT;
    while(active && (post_status = outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT)
    {
        local_stats.extents_retried++;
    }

    /* Update Statistics */
    if(post_status > 0)
    {
        local_stats.extents_sent++;
    }
    else
    {
        mlog(DEBUG, "Atl03 reader failed to post %s to stream %s: %d", record.getRecordType(), outQ->getName(), post_status);
        local_stats.extents_dropped++;
    }
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Atl03Viewer::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Viewer* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<Atl03Viewer*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        const bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "read",        lua_obj->stats.segments_read);
        LuaEngine::setAttrInt(L, "filtered",    lua_obj->stats.extents_filtered);
        LuaEngine::setAttrInt(L, "sent",        lua_obj->stats.extents_sent);
        LuaEngine::setAttrInt(L, "dropped",     lua_obj->stats.extents_dropped);
        LuaEngine::setAttrInt(L, "retried",     lua_obj->stats.extents_retried);
        /* Clear if Requested */
        if(with_clear) memset(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning stats %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
