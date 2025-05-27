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
#include <limits>

#include "OsApi.h"
#include "ContainerRecord.h"
#include "Atl13Reader.h"
#include "Icesat2Fields.h"
#include "AncillaryFields.h"

using std::numeric_limits;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl13Reader::wtRecType = "atl13srec.elevation";
const RecordObject::fieldDef_t Atl13Reader::wtRecDef[] = {
    {"extent_id",               RecordObject::UINT64,   offsetof(water_t, extent_id),               1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"rgt",                     RecordObject::UINT16,   offsetof(water_t, rgt),                     1,  NULL, NATIVE_FLAGS},
    {"cycle",                   RecordObject::UINT16,   offsetof(water_t, cycle),                   1,  NULL, NATIVE_FLAGS},
    {"spot",                    RecordObject::UINT8,    offsetof(water_t, spot),                    1,  NULL, NATIVE_FLAGS},
    {"gt",                      RecordObject::UINT8,    offsetof(water_t, gt),                      1,  NULL, NATIVE_FLAGS},
// beam
    {"time",                    RecordObject::TIME8,    offsetof(water_t, time_ns),                 1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"snow_ice",                RecordObject::INT8,     offsetof(water_t, snow_ice_atl09),          1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"cloud",                   RecordObject::INT8,     offsetof(water_t, cloud_flag_asr_atl09),    1,  NULL, NATIVE_FLAGS},
    {"latitude",                RecordObject::DOUBLE,   offsetof(water_t, latitude),                1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",               RecordObject::DOUBLE,   offsetof(water_t, longitude),               1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"ht_ortho",                RecordObject::FLOAT,    offsetof(water_t, ht_ortho),                1,  NULL, NATIVE_FLAGS},
    {"ht_water_surf",           RecordObject::FLOAT,    offsetof(water_t, ht_water_surf),           1,  NULL, NATIVE_FLAGS},
    {"segment_azimuth",         RecordObject::FLOAT,    offsetof(water_t, segment_azimuth),         1,  NULL, NATIVE_FLAGS},
    {"segment_quality",         RecordObject::INT32,    offsetof(water_t, segment_quality),         1,  NULL, NATIVE_FLAGS},
    {"segment_slope_trk_bdy",   RecordObject::FLOAT,    offsetof(water_t, segment_slope_trk_bdy),   1,  NULL, NATIVE_FLAGS},
    {"water_depth",             RecordObject::FLOAT,    offsetof(water_t, water_depth),             1,  NULL, NATIVE_FLAGS},
};

const char* Atl13Reader::atRecType = "atl13srec";
const RecordObject::fieldDef_t Atl13Reader::atRecDef[] = {
    {"water",                   RecordObject::USER,     offsetof(atl13_t, water),                   0,  wtRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

const char* Atl13Reader::OBJECT_TYPE = "Atl13Reader";
const char* Atl13Reader::LUA_META_NAME = "Atl13Reader";
const struct luaL_Reg Atl13Reader::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL13 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl13Reader::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 3, true, true);

        /* Check for Null Resource and Asset */
        if(_parms->resource.value.empty()) throw RunTimeException(CRITICAL, RTE_FAILURE, "Must supply a resource to process");
        else if(_parms->asset.asset == NULL) throw RunTimeException(CRITICAL, RTE_FAILURE, "Must supply a valid asset");

        /* Return Reader Object */
        return createLuaObject(L, new Atl13Reader(L, outq_name, _parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", Atl13Reader::LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl13Reader::init (void)
{
    RECDEF(wtRecType,       wtRecDef,       sizeof(water_t),        NULL /* "extent_id" */);
    RECDEF(atRecType,       atRecDef,       sizeof(atl13_t),        NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl13Reader::Atl13Reader (lua_State* L, const char* outq_name, Icesat2Fields* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->readTimeout.value * 1000),
    parms(_parms),
    context(NULL)
{
    assert(outq_name);
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.segments_read     = 0;
    stats.extents_filtered  = 0;
    stats.extents_sent      = 0;
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
        context = new H5Coro::Context(parms->asset.asset, parms->resource.value.c_str());

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
            throw RunTimeException(CRITICAL, RTE_FAILURE, "No reader threads were created, invalid track specified: %d\n", parms->track.value);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, outQ, &active, "Failure on resource %s: %s", parms->resource.value.c_str(), e.what());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, outQ, &active, "Failure on resource %s: %s", parms->resource.value.c_str(), e.what());

        /* Indicate End of Data */
        if(sendTerminator) outQ->postCopy("", 0, SYS_TIMEOUT);
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl13Reader::~Atl13Reader (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete outQ;

    delete context;

    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl13Reader::Region::Region (const info_t* info):
    latitude        (info->reader->context, FString("%s/%s", info->prefix, "segment_lat").c_str()),
    longitude       (info->reader->context, FString("%s/%s", info->prefix, "segment_lon").c_str()),
    inclusion_mask  {NULL},
    inclusion_ptr   {NULL}
{
    try
    {
        /* Join Reads */
        latitude.join(info->reader->read_timeout_ms, true);
        longitude.join(info->reader->read_timeout_ms, true);

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
            num_segments = latitude.size;
        }

        /* Check If Anything to Process */
        if(num_segments <= 0)
        {
            throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        latitude.trim(first_segment);
        longitude.trim(first_segment);
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
Atl13Reader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl13Reader::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl13Reader::Region::polyregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < latitude.size)
    {
        /* Test Inclusion */
        const bool inclusion = info->reader->parms->polyIncludes(longitude[segment], latitude[segment]);

        /* Check First Segment */
        if(!first_segment_found && inclusion)
        {
            /* If Coordinate Is In Polygon */
            first_segment_found = true;
            first_segment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            /* If Coordinate Is NOT In Polygon */
            break; // full extent found!
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
void Atl13Reader::Region::rasterregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;

    /* Check Size */
    if(latitude.size <= 0)
    {
        return;
    }

    /* Allocate Inclusion Mask */
    inclusion_mask = new bool [latitude.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    long last_segment = 0;
    int segment = 0;
    while(segment < latitude.size)
    {
        /* Check Inclusion */
        const bool inclusion = info->reader->parms->maskIncludes(longitude[segment], latitude[segment]);
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
 * Atl13Data::Constructor
 *----------------------------------------------------------------------------*/
Atl13Reader::Atl13Data::Atl13Data (const info_t* info, const Region& region):
    sc_orient               (info->reader->context,                                "/orbit_info/sc_orient"),
    delta_time              (info->reader->context, FString("%s/%s", info->prefix, "delta_time").c_str(),            0, region.first_segment, region.num_segments),
    segment_id_beg          (info->reader->context, FString("%s/%s", info->prefix, "segment_id_beg").c_str(),        0, region.first_segment, region.num_segments),
    snow_ice_atl09          (info->reader->context, FString("%s/%s", info->prefix, "snow_ice_atl09").c_str(),        0, region.first_segment, region.num_segments),
    cloud_flag_asr_atl09    (info->reader->context, FString("%s/%s", info->prefix, "cloud_flag_asr_atl09").c_str(),  0, region.first_segment, region.num_segments),
    ht_ortho                (info->reader->context, FString("%s/%s", info->prefix, "ht_ortho").c_str(),              0, region.first_segment, region.num_segments),
    ht_water_surf           (info->reader->context, FString("%s/%s", info->prefix, "ht_water_surf").c_str(),         0, region.first_segment, region.num_segments),
    segment_azimuth         (info->reader->context, FString("%s/%s", info->prefix, "segment_azimuth").c_str(),       0, region.first_segment, region.num_segments),
    segment_quality         (info->reader->context, FString("%s/%s", info->prefix, "segment_quality").c_str(),       H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_slope_trk_bdy   (info->reader->context, FString("%s/%s", info->prefix, "segment_slope_trk_bdy").c_str(), 0, region.first_segment, region.num_segments),
    water_depth             (info->reader->context, FString("%s/%s", info->prefix, "water_depth").c_str(),           0, region.first_segment, region.num_segments)
{
    const FieldList<string>& anc_fields = info->reader->parms->atl13Fields;

    /* Read Ancillary Fields */
    if(anc_fields.length() > 0)
    {
        for(int i = 0; i < anc_fields.length(); i++)
        {
            const string& field_name = anc_fields[i];
            const FString dataset_name("%s/%s", info->prefix, field_name.c_str());
            H5DArray* array = new H5DArray(info->reader->context, dataset_name.c_str(), 0, region.first_segment, region.num_segments);
            const bool status = anc_data.add(field_name.c_str(), array);
            if(!status) delete array;
            assert(status); // the dictionary add should never fail
        }
    }

    /* Join Hardcoded Reads */
    sc_orient.join(info->reader->read_timeout_ms, true);
    delta_time.join(info->reader->read_timeout_ms, true);
    segment_id_beg.join(info->reader->read_timeout_ms, true);
    snow_ice_atl09.join(info->reader->read_timeout_ms, true);
    cloud_flag_asr_atl09.join(info->reader->read_timeout_ms, true);
    ht_ortho.join(info->reader->read_timeout_ms, true);
    ht_water_surf.join(info->reader->read_timeout_ms, true);
    segment_azimuth.join(info->reader->read_timeout_ms, true);
    segment_quality.join(info->reader->read_timeout_ms, true);
    segment_slope_trk_bdy.join(info->reader->read_timeout_ms, true);
    water_depth.join(info->reader->read_timeout_ms, true);

    /* Join Ancillary  Reads */
    if(anc_fields.length() > 0)
    {
        H5DArray* array = NULL;
        const char* dataset_name = anc_data.first(&array);
        while(dataset_name != NULL)
        {
            array->join(info->reader->read_timeout_ms, true);
            dataset_name = anc_data.next(&array);
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Atl13Reader::Atl13Data::~Atl13Data (void) = default;

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl13Reader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Atl13Reader* reader = info->reader;
    const Icesat2Fields* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};
    vector<RecordObject*> rec_vec;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "atl13_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", parms->asset.asset->getName(), parms->resource.value.c_str(), info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(info);

        /* Read ATL06 Datasets */
        const Atl13Data atl13(info, region);

        /* Increment Read Statistics */
        local_stats.segments_read = region.num_segments;

        /* Initialize Loop Variables */
        RecordObject* batch_record = NULL;
        atl13_t* atl13_data = NULL;
        uint32_t extent_counter = 0;
        int batch_index = 0;

        /* Loop Through Each Segment */
        for(long segment = 0; reader->active && segment < region.num_segments; segment++)
        {
            /* Check for Inclusion Mask */
            if(region.inclusion_ptr)
            {
                if(!region.inclusion_ptr[segment])
                {
                    continue;
                }
            }

            /* Create Elevation Batch Record */
            if(!batch_record)
            {
                batch_record = new RecordObject(atRecType);
                atl13_data = reinterpret_cast<atl13_t*>(batch_record->getRecordData());
                rec_vec.push_back(batch_record);
            }

            /* Grab Nominal Segment Quality */
            const int32_t nomial_segment_quality = atl13.segment_quality[4 * segment];

            /* Populate Elevation */
            water_t* entry = &atl13_data->water[batch_index++];
            entry->extent_id                = Icesat2Fields::generateExtentId(reader->parms->granuleFields.rgt.value, reader->parms->granuleFields.cycle.value, reader->parms->granuleFields.region.value, info->track, info->pair, extent_counter) | Icesat2Fields::EXTENT_ID_ELEVATION;
            entry->time_ns                  = Icesat2Fields::deltatime2timestamp(atl13.delta_time[segment]);
            entry->segment_id               = atl13.segment_id_beg[segment];
            entry->rgt                      = reader->parms->granuleFields.rgt.value;
            entry->cycle                    = reader->parms->granuleFields.cycle.value;
            entry->spot                     = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl13.sc_orient[0], (Icesat2Fields::track_t)info->track, info->pair);
            entry->gt                       = Icesat2Fields::getGroundTrack((Icesat2Fields::sc_orient_t)atl13.sc_orient[0], (Icesat2Fields::track_t)info->track, info->pair);
            entry->latitude                 = region.latitude[segment];
            entry->longitude                = region.longitude[segment];
            entry->snow_ice_atl09           = atl13.snow_ice_atl09[segment];
            entry->cloud_flag_asr_atl09     = atl13.cloud_flag_asr_atl09[segment];
            entry->ht_ortho                 = atl13.ht_ortho[segment]               != numeric_limits<float>::max()   ? atl13.ht_ortho[segment]                 : numeric_limits<float>::quiet_NaN();
            entry->ht_water_surf            = atl13.ht_water_surf[segment]          != numeric_limits<float>::max()   ? atl13.ht_water_surf[segment]            : numeric_limits<float>::quiet_NaN();
            entry->segment_azimuth          = atl13.segment_azimuth[segment]        != numeric_limits<float>::max()   ? atl13.segment_azimuth[segment]          : numeric_limits<float>::quiet_NaN();
            entry->segment_quality          = nomial_segment_quality                != numeric_limits<int32_t>::max() ? atl13.segment_quality[segment]          : 0;
            entry->segment_slope_trk_bdy    = atl13.segment_slope_trk_bdy[segment]  != numeric_limits<float>::max()   ? atl13.segment_slope_trk_bdy[segment]    : numeric_limits<float>::quiet_NaN();
            entry->water_depth              = atl13.water_depth[segment]            != numeric_limits<float>::max()   ? atl13.water_depth[segment]              : numeric_limits<float>::quiet_NaN();

            /* Populate Ancillary Data */
            if(parms->atl13Fields.length() > 0)
            {
                /* Populate Each Field in Array */
                vector<AncillaryFields::field_t> field_vec;
                for(int i = 0; i < parms->atl13Fields.length(); i++)
                {
                    const string& field_name = parms->atl13Fields[i];

                    AncillaryFields::field_t field;
                    field.anc_type = Icesat2Fields::ATL06_ANC_TYPE;
                    field.field_index = i;
                    field.data_type = atl13.anc_data[field_name.c_str()]->elementType();
                    atl13.anc_data[field_name.c_str()]->serialize(&field.value[0], segment, 1);

                    field_vec.push_back(field);
                }

                /* Create Field Array Record */
                RecordObject* field_array_rec = AncillaryFields::createFieldArrayRecord(entry->extent_id, field_vec); // memory allocation
                rec_vec.push_back(field_array_rec);
            }

            /* Post Records */
            if((batch_index == BATCH_SIZE) || (segment == (region.num_segments - 1)))
            {
                int post_status = MsgQ::STATE_TIMEOUT;
                unsigned char* buffer = NULL;
                int bufsize = 0;

                /* Calculate Batch Record Size */
                const int recsize = batch_index * sizeof(water_t);
                batch_record->setUsedData(recsize);

                if(rec_vec.size() > 1) // elevation and ancillary field records
                {
                    /* Create and Serialize Container Record */
                    ContainerRecord container(rec_vec);
                    bufsize = container.serialize(&buffer, RecordObject::TAKE_OWNERSHIP);
                }
                else // only an elevation record
                {
                    /* Serialize Elevation Batch Record */
                    bufsize = batch_record->serialize(&buffer, RecordObject::TAKE_OWNERSHIP);
                }

                /* Post Record */
                while(reader->active && (post_status = reader->outQ->postRef(buffer, bufsize, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT)
                {
                    local_stats.extents_retried++;
                }

                /* Handle Success/Failure of Post */
                if(post_status > 0)
                {
                    local_stats.extents_sent += batch_index;
                }
                else
                {
                    delete [] buffer; // delete buffer we now own and never was posted
                    local_stats.extents_dropped += batch_index;
                }

                /* Reset Batch Record */
                batch_record = NULL;
                batch_index = 0;

                /* Free and Reset Records */
                for(RecordObject* rec: rec_vec)
                {
                    delete rec;
                }
                rec_vec.clear();
            }

            /* Bump Extent Counter */
            extent_counter++;
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s track %d: %s", parms->resource.value.c_str(), info->track, e.what());
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
            mlog(INFO, "Completed processing resource %s", parms->resource.value.c_str());

            /* Indicate End of Data */
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s", status, parms->resource.value.c_str());
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s ... trying again", parms->resource.value.c_str());
                    }
                }
            }
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up */
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Atl13Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl13Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<Atl13Reader*>(getLuaSelf(L, 1));
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
