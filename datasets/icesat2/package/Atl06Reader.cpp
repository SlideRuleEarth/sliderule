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
#include "Atl06Reader.h"
#include "Icesat2Fields.h"
#include "AncillaryFields.h"
#include "FieldList.h"

using std::numeric_limits;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl06Reader::elRecType = "atl06srec.elevation";
const RecordObject::fieldDef_t Atl06Reader::elRecDef[] = {
    {"extent_id",               RecordObject::UINT64,   offsetof(elevation_t, extent_id),               1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"rgt",                     RecordObject::UINT16,   offsetof(elevation_t, rgt),                     1,  NULL, NATIVE_FLAGS},
    {"cycle",                   RecordObject::UINT16,   offsetof(elevation_t, cycle),                   1,  NULL, NATIVE_FLAGS},
    {"spot",                    RecordObject::UINT8,    offsetof(elevation_t, spot),                    1,  NULL, NATIVE_FLAGS},
    {"gt",                      RecordObject::UINT8,    offsetof(elevation_t, gt),                      1,  NULL, NATIVE_FLAGS},
// land_ice_segments
    {"time",                    RecordObject::TIME8,    offsetof(elevation_t, time_ns),                 1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"h_li",                    RecordObject::FLOAT,    offsetof(elevation_t, h_li),                    1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"h_li_sigma",              RecordObject::FLOAT,    offsetof(elevation_t, h_li_sigma),              1,  NULL, NATIVE_FLAGS},
    {"latitude",                RecordObject::DOUBLE,   offsetof(elevation_t, latitude),                1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",               RecordObject::DOUBLE,   offsetof(elevation_t, longitude),               1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"atl06_quality_summary",   RecordObject::INT8,     offsetof(elevation_t, atl06_quality_summary),   1,  NULL, NATIVE_FLAGS},
    {"segment_id",              RecordObject::UINT32,   offsetof(elevation_t, segment_id),              1,  NULL, NATIVE_FLAGS},
    {"sigma_geo_h",             RecordObject::FLOAT,    offsetof(elevation_t, sigma_geo_h),             1,  NULL, NATIVE_FLAGS},
// ground track
    {"x_atc",                   RecordObject::DOUBLE,   offsetof(elevation_t, x_atc),                   1,  NULL, NATIVE_FLAGS},
    {"y_atc",                   RecordObject::FLOAT,    offsetof(elevation_t, y_atc),                   1,  NULL, NATIVE_FLAGS},
    {"seg_azimuth",             RecordObject::FLOAT,    offsetof(elevation_t, seg_azimuth),             1,  NULL, NATIVE_FLAGS},
// fit_statistics
    {"dh_fit_dx",               RecordObject::FLOAT,    offsetof(elevation_t, dh_fit_dx),               1,  NULL, NATIVE_FLAGS},
    {"h_robust_sprd",           RecordObject::FLOAT,    offsetof(elevation_t, h_robust_sprd),           1,  NULL, NATIVE_FLAGS},
    {"n_fit_photons",           RecordObject::INT32,    offsetof(elevation_t, n_fit_photons),           1,  NULL, NATIVE_FLAGS},
    {"w_surface_window_final",  RecordObject::FLOAT,    offsetof(elevation_t, w_surface_window_final),  1,  NULL, NATIVE_FLAGS},
// geophysical
    {"bsnow_conf",              RecordObject::INT8,     offsetof(elevation_t, bsnow_conf),              1,  NULL, NATIVE_FLAGS},
    {"bsnow_h",                 RecordObject::FLOAT,    offsetof(elevation_t, bsnow_h),                 1,  NULL, NATIVE_FLAGS},
    {"r_eff",                   RecordObject::FLOAT,    offsetof(elevation_t, r_eff),                   1,  NULL, NATIVE_FLAGS},
    {"tide_ocean",              RecordObject::FLOAT,    offsetof(elevation_t, tide_ocean),              1,  NULL, NATIVE_FLAGS},
};

const char* Atl06Reader::atRecType = "atl06srec";
const RecordObject::fieldDef_t Atl06Reader::atRecDef[] = {
    {"elevation",               RecordObject::USER,     offsetof(atl06_t, elevation),               0,  elRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

const char* Atl06Reader::OBJECT_TYPE = "Atl06Reader";
const char* Atl06Reader::LUA_META_NAME = "Atl06Reader";
const struct luaL_Reg Atl06Reader::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL06 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl06Reader::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 3, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Atl06Reader(L, outq_name, _parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", Atl06Reader::LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl06Reader::init (void)
{
    RECDEF(elRecType,       elRecDef,       sizeof(elevation_t),            NULL /* "extent_id" */);
    RECDEF(atRecType,       atRecDef,       offsetof(atl06_t, elevation),   NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Atl06Reader (lua_State* L, const char* outq_name, Icesat2Fields* _parms, bool _send_terminator):
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
Atl06Reader::~Atl06Reader (void)
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
Atl06Reader::Region::Region (const info_t* info):
    latitude        (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/latitude").c_str()),
    longitude       (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/longitude").c_str()),
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
Atl06Reader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl06Reader::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl06Reader::Region::polyregion (const info_t* info)
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
void Atl06Reader::Region::rasterregion (const info_t* info)
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
 * Atl06Data::Constructor
 *----------------------------------------------------------------------------*/
Atl06Reader::Atl06Data::Atl06Data (const info_t* info, const Region& region):
    sc_orient               (info->reader->context,                                "/orbit_info/sc_orient"),
    delta_time              (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/delta_time").c_str(),                           0, region.first_segment, region.num_segments),
    h_li                    (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/h_li").c_str(),                                 0, region.first_segment, region.num_segments),
    h_li_sigma              (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/h_li_sigma").c_str(),                           0, region.first_segment, region.num_segments),
    atl06_quality_summary   (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/atl06_quality_summary").c_str(),                0, region.first_segment, region.num_segments),
    segment_id              (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/segment_id").c_str(),                           0, region.first_segment, region.num_segments),
    sigma_geo_h             (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/sigma_geo_h").c_str(),                          0, region.first_segment, region.num_segments),
    x_atc                   (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/ground_track/x_atc").c_str(),                   0, region.first_segment, region.num_segments),
    y_atc                   (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/ground_track/y_atc").c_str(),                   0, region.first_segment, region.num_segments),
    seg_azimuth             (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/ground_track/seg_azimuth").c_str(),             0, region.first_segment, region.num_segments),
    dh_fit_dx               (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/dh_fit_dx").c_str(),             0, region.first_segment, region.num_segments),
    h_robust_sprd           (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/h_robust_sprd").c_str(),         0, region.first_segment, region.num_segments),
    n_fit_photons           (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/n_fit_photons").c_str(),         0, region.first_segment, region.num_segments),
    w_surface_window_final  (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/fit_statistics/w_surface_window_final").c_str(),0, region.first_segment, region.num_segments),
    bsnow_conf              (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/bsnow_conf").c_str(),               0, region.first_segment, region.num_segments),
    bsnow_h                 (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/bsnow_h").c_str(),                  0, region.first_segment, region.num_segments),
    r_eff                   (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/r_eff").c_str(),                    0, region.first_segment, region.num_segments),
    tide_ocean              (info->reader->context, FString("%s/%s", info->prefix, "land_ice_segments/geophysical/tide_ocean").c_str(),               0, region.first_segment, region.num_segments)
{
    const FieldList<string>& anc_fields = info->reader->parms->atl06Fields;

    /* Read Ancillary Fields */
    if(anc_fields.length() > 0)
    {
        for(int i = 0; i < anc_fields.length(); i++)
        {
            const string& field_name = anc_fields[i];
            const FString dataset_name("%s/land_ice_segments/%s", info->prefix, field_name.c_str());
            H5DArray* array = new H5DArray(info->reader->context, dataset_name.c_str(), 0, region.first_segment, region.num_segments);
            const bool status = anc_data.add(field_name.c_str(), array);
            if(!status) delete array;
            assert(status); // the dictionary add should never fail
        }
    }

    /* Join Hardcoded Reads */
    sc_orient.join(info->reader->read_timeout_ms, true);
    delta_time.join(info->reader->read_timeout_ms, true);
    h_li.join(info->reader->read_timeout_ms, true);
    h_li_sigma.join(info->reader->read_timeout_ms, true);
    atl06_quality_summary.join(info->reader->read_timeout_ms, true);
    segment_id.join(info->reader->read_timeout_ms, true);
    sigma_geo_h.join(info->reader->read_timeout_ms, true);
    x_atc.join(info->reader->read_timeout_ms, true);
    y_atc.join(info->reader->read_timeout_ms, true);
    seg_azimuth.join(info->reader->read_timeout_ms, true);
    dh_fit_dx.join(info->reader->read_timeout_ms, true);
    h_robust_sprd.join(info->reader->read_timeout_ms, true);
    n_fit_photons.join(info->reader->read_timeout_ms, true);
    w_surface_window_final.join(info->reader->read_timeout_ms, true);
    bsnow_conf.join(info->reader->read_timeout_ms, true);
    bsnow_h.join(info->reader->read_timeout_ms, true);
    r_eff.join(info->reader->read_timeout_ms, true);
    tide_ocean.join(info->reader->read_timeout_ms, true);

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
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl06Reader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Atl06Reader* reader = info->reader;
    const Icesat2Fields* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};
    vector<RecordObject*> rec_vec;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "atl06_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", parms->asset.getName(), parms->getResource(), info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(info);

        /* Read ATL06 Datasets */
        const Atl06Data atl06(info, region);

        /* Increment Read Statistics */
        local_stats.segments_read = region.num_segments;

        /* Initialize Loop Variables */
        RecordObject* batch_record = NULL;
        atl06_t* atl06_data = NULL;
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
                batch_record = new RecordObject(atRecType, sizeof(atl06_t));
                atl06_data = reinterpret_cast<atl06_t*>(batch_record->getRecordData());
                rec_vec.push_back(batch_record);
            }

            /* Populate Elevation */
            elevation_t* entry = &atl06_data->elevation[batch_index++];
            entry->extent_id                = Icesat2Fields::generateExtentId(parms->rgt.value, parms->cycle.value, parms->region.value, info->track, info->pair, extent_counter) | Icesat2Fields::EXTENT_ID_ELEVATION;
            entry->time_ns                  = Icesat2Fields::deltatime2timestamp(atl06.delta_time[segment]);
            entry->segment_id               = atl06.segment_id[segment];
            entry->rgt                      = parms->rgt.value;
            entry->cycle                    = parms->cycle.value;
            entry->spot                     = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl06.sc_orient[0], (Icesat2Fields::track_t)info->track, info->pair);
            entry->gt                       = Icesat2Fields::getGroundTrack((Icesat2Fields::sc_orient_t)atl06.sc_orient[0], (Icesat2Fields::track_t)info->track, info->pair);
            entry->atl06_quality_summary    = atl06.atl06_quality_summary[segment];
            entry->bsnow_conf               = atl06.bsnow_conf[segment];
            entry->n_fit_photons            = atl06.n_fit_photons[segment]          != numeric_limits<int32_t>::max() ? atl06.n_fit_photons[segment]            : 0;
            entry->latitude                 = region.latitude[segment];
            entry->longitude                = region.longitude[segment];
            entry->x_atc                    = atl06.x_atc[segment]                  != numeric_limits<double>::max()  ? atl06.x_atc[segment]                    : numeric_limits<double>::quiet_NaN();
            entry->y_atc                    = atl06.y_atc[segment]                  != numeric_limits<float>::max()   ? atl06.y_atc[segment]                    : numeric_limits<float>::quiet_NaN();
            entry->h_li                     = atl06.h_li[segment]                   != numeric_limits<float>::max()   ? atl06.h_li[segment]                     : numeric_limits<float>::quiet_NaN();
            entry->h_li_sigma               = atl06.h_li_sigma[segment]             != numeric_limits<float>::max()   ? atl06.h_li_sigma[segment]               : numeric_limits<float>::quiet_NaN();
            entry->sigma_geo_h              = atl06.sigma_geo_h[segment]            != numeric_limits<float>::max()   ? atl06.sigma_geo_h[segment]              : numeric_limits<float>::quiet_NaN();
            entry->seg_azimuth              = atl06.seg_azimuth[segment]            != numeric_limits<float>::max()   ? atl06.seg_azimuth[segment]              : numeric_limits<float>::quiet_NaN();
            entry->dh_fit_dx                = atl06.dh_fit_dx[segment]              != numeric_limits<float>::max()   ? atl06.dh_fit_dx[segment]                : numeric_limits<float>::quiet_NaN();
            entry->h_robust_sprd            = atl06.h_robust_sprd[segment]          != numeric_limits<float>::max()   ? atl06.h_robust_sprd[segment]            : numeric_limits<float>::quiet_NaN();
            entry->w_surface_window_final   = atl06.w_surface_window_final[segment] != numeric_limits<float>::max()   ? atl06.w_surface_window_final[segment]   : numeric_limits<float>::quiet_NaN();
            entry->bsnow_h                  = atl06.bsnow_h[segment]                != numeric_limits<float>::max()   ? atl06.bsnow_h[segment]                  : numeric_limits<float>::quiet_NaN();
            entry->r_eff                    = atl06.r_eff[segment]                  != numeric_limits<float>::max()   ? atl06.r_eff[segment]                    : numeric_limits<float>::quiet_NaN();
            entry->tide_ocean               = atl06.tide_ocean[segment]             != numeric_limits<float>::max()   ? atl06.tide_ocean[segment]               : numeric_limits<float>::quiet_NaN();

            /* Populate Ancillary Data */
            if(parms->atl06Fields.length() > 0)
            {
                /* Populate Each Field in Array */
                vector<AncillaryFields::field_t> field_vec;
                for(int i = 0; i < parms->atl06Fields.length(); i++)
                {
                    const string& field_name = parms->atl06Fields[i];

                    AncillaryFields::field_t field;
                    field.anc_type = Icesat2Fields::ATL06_ANC_TYPE;
                    field.field_index = i;
                    field.data_type = atl06.anc_data[field_name.c_str()]->elementType();
                    atl06.anc_data[field_name.c_str()]->serialize(&field.value[0], segment, 1);

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
                const int recsize = batch_index * sizeof(elevation_t);
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
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s track %d: %s", parms->getResource(), info->track, e.what());
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
            mlog(INFO, "Completed processing resource %s", parms->getResource());

            /* Indicate End of Data */
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s", status, parms->getResource());
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s ... trying again", parms->getResource());
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
int Atl06Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl06Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<Atl06Reader*>(getLuaSelf(L, 1));
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
