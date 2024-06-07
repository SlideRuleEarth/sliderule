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

#include "core.h"
#include "h5.h"
#include "icesat2.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Viewer::segRecType = "atl03vrec.segments";
const RecordObject::fieldDef_t Atl03Viewer::segRecDef[] = {
    {"time",            RecordObject::TIME8,    offsetof(segment_t, time_ns),        1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"latitude",        RecordObject::DOUBLE,   offsetof(segment_t, latitude),       1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",       RecordObject::DOUBLE,   offsetof(segment_t, longitude),      1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"segment_dist_x",  RecordObject::FLOAT,    offsetof(segment_t, dist_x),         1,  NULL, NATIVE_FLAGS},
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
    {"extent_id",       RecordObject::UINT64,   offsetof(extent_t, extent_id),              1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
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
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl03Viewer::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    Icesat2Parms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = dynamic_cast<Asset*>(getLuaObject(L, 1, Asset::OBJECT_TYPE));
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = dynamic_cast<Icesat2Parms*>(getLuaObject(L, 4, Icesat2Parms::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Viewer Object */
        return createLuaObject(L, new Atl03Viewer(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
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
Atl03Viewer::Atl03Viewer (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, Icesat2Parms* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->read_timeout * 1000)
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
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
        /* Parse Globals (throws) */
        parseResource(resource, start_rgt, start_cycle, start_region);

        /* Create Readers */
        for(int track = 1; track <= Icesat2Parms::NUM_TRACKS; track++)
        {
            for(int pair = 0; pair < Icesat2Parms::NUM_PAIR_TRACKS; pair++)
            {
                const int gt_index = (2 * (track - 1)) + pair;
                if(parms->beams[gt_index] && (parms->track == Icesat2Parms::ALL_TRACKS || track == parms->track))
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
            throw RunTimeException(CRITICAL, RTE_ERROR, "No reader threads were created, invalid track specified: %d\n", parms->track);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, outQ, &active, "Failure on resource %s: %s", resource, e.what());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, outQ, &active, "Failure on resource %s: %s", resource, e.what());

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

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03Viewer::Region::Region (info_t* info):
    segment_lat    (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str(), &info->reader->context),
    segment_lon    (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str(), &info->reader->context),
    segment_ph_cnt (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str(), &info->reader->context),
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
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(info->reader->parms->raster.valid())
        {
            rasterregion(info);
        }
        else if(info->reader->parms->points_in_poly > 0)
        {
            polyregion(info);
        }
        else
        {
            return; // early exit since no subsetting required
        }

        /* Check If Anything to Process */
        if(num_photons <= 0)
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
void Atl03Viewer::Region::polyregion (info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        const MathLib::coord_t segment_coord = {segment_lon[segment], segment_lat[segment]};
        const MathLib::point_t segment_point = MathLib::coord2point(segment_coord, info->reader->parms->projection);

        /* Test Inclusion */
        if(MathLib::inpoly(info->reader->parms->projected_poly, info->reader->parms->points_in_poly, segment_point))
        {
            inclusion = true;
        }

        /* Check First Segment */
        if(!first_segment_found)
        {
            /* If Coordinate Is In Polygon */
            if(inclusion && segment_ph_cnt[segment] != 0)
            {
                /* Set First Segment */
                first_segment_found = true;
                first_segment = segment;

                /* Include Photons From First Segment */
                num_photons = segment_ph_cnt[segment];
            }
            else
            {
                /* Update Photon Index */
                first_photon += segment_ph_cnt[segment];
            }
        }
        else
        {
            /* If Coordinate Is NOT In Polygon */
            if(!inclusion && segment_ph_cnt[segment] != 0)
            {
                break; // full extent found!
            }

            /* Update Photon Index */
            num_photons += segment_ph_cnt[segment];
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
void Atl03Viewer::Region::rasterregion (info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;

    /* Check Size */
    if(segment_ph_cnt.size <= 0)
    {
        return;
    }

    /* Allocate Inclusion Mask */
    inclusion_mask = new bool [segment_ph_cnt.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    long curr_num_photons = 0;
    long last_segment = 0;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        if(segment_ph_cnt[segment] != 0)
        {
            /* Check Inclusion */
            const bool inclusion = info->reader->parms->raster.includes(segment_lon[segment], segment_lat[segment]);
            inclusion_mask[segment] = inclusion;

            /* Check For First Segment */
            if(!first_segment_found)
            {
                /* If Coordinate Is In Raster */
                if(inclusion)
                {
                    first_segment_found = true;

                    /* Set First Segment */
                    first_segment = segment;
                    last_segment = segment;

                    /* Include Photons From First Segment */
                    curr_num_photons = segment_ph_cnt[segment];
                    num_photons = curr_num_photons;
                }
                else
                {
                    /* Update Photon Index */
                    first_photon += segment_ph_cnt[segment];
                }
            }
            else
            {
                /* Update Photon Count and Segment */
                curr_num_photons += segment_ph_cnt[segment];

                /* If Coordinate Is In Raster */
                if(inclusion)
                {
                    /* Update Number of Photons to Current Count */
                    num_photons = curr_num_photons;

                    /* Update Number of Segments to Current Segment Count */
                    last_segment = segment;
                }
            }
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
Atl03Viewer::Atl03Data::Atl03Data (info_t* info, const Region& region):
    sc_orient           (info->reader->asset, info->reader->resource,                                "/orbit_info/sc_orient",                     &info->reader->context),
    segment_delta_time  (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/delta_time").c_str(),           &info->reader->context, 0, region.first_segment, region.num_segments),
    segment_id          (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/segment_id").c_str(),           &info->reader->context, 0, region.first_segment, region.num_segments),
    segment_dist_x      (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/segment_dist_x").c_str(),       &info->reader->context, 0, region.first_segment, region.num_segments),
    ref_segment_lat     (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str(), &info->reader->context, 0, region.first_segment, region.num_segments),
    ref_segment_lon     (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str(), &info->reader->context, 0, region.first_segment, region.num_segments),
    segment_ph_cnt      (info->reader->asset, info->reader->resource, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str(),       &info->reader->context, 0, region.first_segment, region.num_segments)
{
    /* Join Hardcoded Reads */
    sc_orient.join(info->reader->read_timeout_ms, true);
    segment_delta_time.join(info->reader->read_timeout_ms, true);
    segment_id.join(info->reader->read_timeout_ms, true);
    segment_dist_x.join(info->reader->read_timeout_ms, true);
    ref_segment_lat.join(info->reader->read_timeout_ms, true);
    ref_segment_lon.join(info->reader->read_timeout_ms, true);
    segment_ph_cnt.join(info->reader->read_timeout_ms, true);

    /* Sanity check for size of all returned value arrays */
    const long scnt = region.num_segments != -1 ? region.num_segments : segment_delta_time.size;
    assert(scnt == segment_delta_time.size);
    assert(scnt == ref_segment_lat.size);
    assert(scnt == ref_segment_lon.size);
    assert(scnt == segment_dist_x.size);
    assert(scnt == segment_id.size);
    assert(scnt == segment_ph_cnt.size);
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
    info_t* info = static_cast<info_t*>(parm);
    Atl03Viewer* reader = info->reader;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "atl03_viewsubsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", info->reader->asset->getName(), info->reader->resource, info->track);
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

        /* Initialize Extent Counter */
        uint32_t extent_counter = 0;

        /* Traverse All Segments in Dataset */
        for(uint32_t s = 0; s < local_stats.segments_read; s++)
        {
            /* Check for Termination */
            if(!reader->active) break;

            const segment_t segment = {
                .time_ns   = Icesat2Parms::deltatime2timestamp(atl03.segment_delta_time[s]),
                .latitude  = atl03.ref_segment_lat[s],
                .longitude = atl03.ref_segment_lon[s],
                .dist_x    = atl03.segment_dist_x[s],
                .id        = static_cast<uint32_t>(atl03.segment_id[s]),
                .ph_cnt    = static_cast<uint32_t>(region.segment_ph_cnt[s])
            };
            segments.add(segment);

            const bool last_segment = (s == local_stats.segments_read - 1);

            if(segments.length() % max_segments_per_extent == 0 || last_segment)
            {
                /* Generate Extent ID */

                /* Calculate Extent Record Size */
                const int batch_bytes = offsetof(extent_t, segments) + (sizeof(segment_t) * segments.length());

                /* Initialize Extent Record */
                RecordObject record (batchRecType, batch_bytes);
                extent_t* extent = reinterpret_cast<extent_t*>(record.getRecordData());
                extent->region = reader->start_region;
                extent->track = info->track;
                extent->pair = info->pair;
                extent->spot = Icesat2Parms::getSpotNumber(static_cast<Icesat2Parms::sc_orient_t>(atl03.sc_orient[0]),
                                                           static_cast<Icesat2Parms::track_t>(info->track), info->pair);
                extent->reference_ground_track = reader->start_rgt;
                extent->cycle = reader->start_cycle;
                extent->extent_id = Icesat2Parms::generateExtentId(reader->start_rgt, reader->start_cycle, reader->start_region, info->track, info->pair, extent_counter);

                /* Populate Segments */
                for(int32_t i = 0; i < segments.length(); i++)
                {
                    extent->segments[i] = segments[i];
                }

                reader->postRecord(record, local_stats);

                /* Reset Segment List */
                segments.clear();

                /* Bump Extent Counter */
                extent_counter++;
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s track %d.%d: %s", info->reader->resource, info->track, info->pair, e.what());
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
            mlog(INFO, "Completed processing resource %s track %d.%d (r: %u, s: %u)", info->reader->resource, info->track, info->pair, local_stats.segments_read, local_stats.extents_sent);

            /* Indicate End of Data */
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s track %d.%d", status, info->reader->resource, info->track, info->pair);
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s track %d.%d ... trying again", info->reader->resource, info->track, info->pair);
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
 * parseResource
 *
 *  ATL0x_YYYYMMDDHHMMSS_ttttccrr_vvv_ee
 *      YYYY    - year
 *      MM      - month
 *      DD      - day
 *      HH      - hour
 *      MM      - minute
 *      SS      - second
 *      tttt    - reference ground track
 *      cc      - cycle
 *      rr      - region
 *      vvv     - version
 *      ee      - revision
 *----------------------------------------------------------------------------*/
void Atl03Viewer::parseResource (const char* _resource, uint16_t& rgt, uint8_t& cycle, uint8_t& region)
{
    if(StringLib::size(_resource) < 29)
    {
        rgt = 0;
        cycle = 0;
        region = 0;
        return; // early exit on error
    }

    long val;
    char rgt_str[5];
    rgt_str[0] = _resource[21];
    rgt_str[1] = _resource[22];
    rgt_str[2] = _resource[23];
    rgt_str[3] = _resource[24];
    rgt_str[4] = '\0';
    if(StringLib::str2long(rgt_str, &val, 10))
    {
        rgt = static_cast<uint16_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse RGT from resource %s: %s", _resource, rgt_str);
    }

    char cycle_str[3];
    cycle_str[0] = _resource[25];
    cycle_str[1] = _resource[26];
    cycle_str[2] = '\0';
    if(StringLib::str2long(cycle_str, &val, 10))
    {
        cycle = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Cycle from resource %s: %s", _resource, cycle_str);
    }

    char region_str[3];
    region_str[0] = _resource[27];
    region_str[1] = _resource[28];
    region_str[2] = '\0';
    if(StringLib::str2long(region_str, &val, 10))
    {
        region = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Region from resource %s: %s", _resource, region_str);
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
