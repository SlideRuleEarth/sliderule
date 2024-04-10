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
#include "GeoLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03BathyReader::phRecType = "bathyrec.photons";
const RecordObject::fieldDef_t Atl03BathyReader::phRecDef[] = {
    {"time",            RecordObject::TIME8,    offsetof(photon_t, time_ns),        1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"index_ph",        RecordObject::INT32,    offsetof(photon_t, index_ph),       1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"geoid_corr_h",    RecordObject::FLOAT,    offsetof(photon_t, geoid_corr_h),   1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"latitude",        RecordObject::DOUBLE,   offsetof(photon_t, latitude),       1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",       RecordObject::DOUBLE,   offsetof(photon_t, longitude),      1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"x_ph",            RecordObject::DOUBLE,   offsetof(photon_t, x_ph),           1,  NULL, NATIVE_FLAGS},
    {"y_ph",            RecordObject::DOUBLE,   offsetof(photon_t, y_ph),           1,  NULL, NATIVE_FLAGS},
    {"x_atc",           RecordObject::DOUBLE,   offsetof(photon_t, x_atc),          1,  NULL, NATIVE_FLAGS},
    {"y_atc",           RecordObject::DOUBLE,   offsetof(photon_t, y_atc),          1,  NULL, NATIVE_FLAGS},
    {"sigma_along",     RecordObject::FLOAT,    offsetof(photon_t, sigma_along),    1,  NULL, NATIVE_FLAGS},
    {"sigma_across",    RecordObject::FLOAT,    offsetof(photon_t, sigma_across),   1,  NULL, NATIVE_FLAGS},
    {"ndwi",            RecordObject::FLOAT,    offsetof(photon_t, ndwi),           1,  NULL, NATIVE_FLAGS},
    {"yapc_score",      RecordObject::UINT8,    offsetof(photon_t, yapc_score),     1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"max_signal_conf", RecordObject::UINT8,    offsetof(photon_t, max_signal_conf),1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"quality_ph",      RecordObject::INT8,     offsetof(photon_t, quality_ph),     1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
};

const char* Atl03BathyReader::exRecType = "bathyrec";
const RecordObject::fieldDef_t Atl03BathyReader::exRecDef[] = {
    {"region",          RecordObject::UINT8,    offsetof(extent_t, region),                 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"track",           RecordObject::UINT8,    offsetof(extent_t, track),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"pair",            RecordObject::UINT8,    offsetof(extent_t, pair),                   1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"sc_orient",       RecordObject::UINT8,    offsetof(extent_t, spacecraft_orientation), 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"rgt",             RecordObject::UINT16,   offsetof(extent_t, reference_ground_track), 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"cycle",           RecordObject::UINT8,    offsetof(extent_t, cycle),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"utm_zone",        RecordObject::UINT8,    offsetof(extent_t, utm_zone),               1,  NULL, NATIVE_FLAGS},
    {"background_rate", RecordObject::DOUBLE,   offsetof(extent_t, background_rate),        1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"solar_elevation", RecordObject::FLOAT,    offsetof(extent_t, solar_elevation),        1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"wind_v",          RecordObject::FLOAT,    offsetof(extent_t, wind_v),                 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"pointing_angle",  RecordObject::FLOAT,    offsetof(extent_t, pointing_angle),         1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"extent_id",       RecordObject::UINT64,   offsetof(extent_t, extent_id),              1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"photons",         RecordObject::USER,     offsetof(extent_t, photons),                0,  phRecType, NATIVE_FLAGS | RecordObject::BATCH} // variable length
};

const char* Atl03BathyReader::OBJECT_TYPE = "Atl03BathyReader";
const char* Atl03BathyReader::LUA_META_NAME = "Atl03BathyReader";
const struct luaL_Reg Atl03BathyReader::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl03BathyReader::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    BathyParms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = dynamic_cast<Asset*>(getLuaObject(L, 1, Asset::OBJECT_TYPE));
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = dynamic_cast<BathyParms*>(getLuaObject(L, 4, BathyParms::OBJECT_TYPE));
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Atl03BathyReader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating Atl03BathyReader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03BathyReader::init (void)
{
    RECDEF(phRecType,       phRecDef,       sizeof(photon_t),       NULL);
    RECDEF(exRecType,       exRecDef,       sizeof(extent_t),       NULL /* "extent_id" */);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03BathyReader::Atl03BathyReader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, BathyParms* _parms, bool _send_terminator):
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

    /* Generate ATL09 Resource Name */
    resource09 = StringLib::duplicate(resource);
    resource09[4] = '9';
    // TODO: need to change the name to the first ATL03 granule for the orbit (region = 1)
    resource09[27] = '0';
    resource09[28] = '1';

    /* Create Publisher and File Pointer */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

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
                int gt_index = (2 * (track - 1)) + pair;
                if(parms->beams[gt_index] && (parms->track == Icesat2Parms::ALL_TRACKS || track == parms->track))
                {
                    info_t* info = new info_t;
                    info->builder = this;
                    info->track = track;
                    info->pair = pair;
                    info->beam = gt_index + 1;
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
Atl03BathyReader::~Atl03BathyReader (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete outQ;

    parms->releaseLuaObject();

    delete [] resource;
    delete [] resource09;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03BathyReader::Region::Region (info_t* info):
    segment_lat    (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str(), &info->builder->context),
    segment_lon    (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str(), &info->builder->context),
    segment_ph_cnt (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str(), &info->builder->context),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        segment_lat.join(info->builder->read_timeout_ms, true);
        segment_lon.join(info->builder->read_timeout_ms, true);
        segment_ph_cnt.join(info->builder->read_timeout_ms, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(info->builder->parms->raster != NULL)
        {
            rasterregion(info);
        }
        else if(info->builder->parms->points_in_poly > 0)
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
            throw RunTimeException(CRITICAL, RTE_EMPTY_SUBSET, "empty spatial region");
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
Atl03BathyReader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl03BathyReader::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl03BathyReader::Region::polyregion (info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        MathLib::coord_t segment_coord = {segment_lon[segment], segment_lat[segment]};
        MathLib::point_t segment_point = MathLib::coord2point(segment_coord, info->builder->parms->projection);

        /* Test Inclusion */
        if(MathLib::inpoly(info->builder->parms->projected_poly, info->builder->parms->points_in_poly, segment_point))
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
void Atl03BathyReader::Region::rasterregion (info_t* info)
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
            bool inclusion = info->builder->parms->raster->includes(segment_lon[segment], segment_lat[segment]);
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
Atl03BathyReader::Atl03Data::Atl03Data (info_t* info, const Region& region):
    sc_orient           (info->builder->asset, info->builder->resource,                                "/orbit_info/sc_orient",                &info->builder->context),
    velocity_sc         (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/velocity_sc").c_str(),     &info->builder->context, H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_delta_time  (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/delta_time").c_str(),      &info->builder->context, 0, region.first_segment, region.num_segments),
    segment_dist_x      (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/segment_dist_x").c_str(),  &info->builder->context, 0, region.first_segment, region.num_segments),
    solar_elevation     (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "geolocation/solar_elevation").c_str(), &info->builder->context, 0, region.first_segment, region.num_segments),
    dist_ph_along       (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/dist_ph_along").c_str(),       &info->builder->context, 0, region.first_photon,  region.num_photons),
    dist_ph_across      (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/dist_ph_across").c_str(),      &info->builder->context, 0, region.first_photon,  region.num_photons),
    h_ph                (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/h_ph").c_str(),                &info->builder->context, 0, region.first_photon,  region.num_photons),
    signal_conf_ph      (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/signal_conf_ph").c_str(),      &info->builder->context, info->builder->parms->surface_type, region.first_photon,  region.num_photons),
    quality_ph          (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/quality_ph").c_str(),          &info->builder->context, 0, region.first_photon,  region.num_photons),
    weight_ph           (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/weight_ph").c_str(),           &info->builder->context, 0, region.first_photon,  region.num_photons),
    lat_ph              (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/lat_ph").c_str(),              &info->builder->context, 0, region.first_photon,  region.num_photons),
    lon_ph              (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/lon_ph").c_str(),              &info->builder->context, 0, region.first_photon,  region.num_photons),
    delta_time          (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "heights/delta_time").c_str(),          &info->builder->context, 0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "bckgrd_atlas/delta_time").c_str(),     &info->builder->context),
    bckgrd_rate         (info->builder->asset, info->builder->resource, FString("%s/%s", info->prefix, "bckgrd_atlas/bckgrd_rate").c_str(),    &info->builder->context)
{
    sc_orient.join(info->builder->read_timeout_ms, true);
    velocity_sc.join(info->builder->read_timeout_ms, true);
    segment_delta_time.join(info->builder->read_timeout_ms, true);
    segment_dist_x.join(info->builder->read_timeout_ms, true);
    solar_elevation.join(info->builder->read_timeout_ms, true);
    dist_ph_along.join(info->builder->read_timeout_ms, true);
    dist_ph_across.join(info->builder->read_timeout_ms, true);
    h_ph.join(info->builder->read_timeout_ms, true);
    signal_conf_ph.join(info->builder->read_timeout_ms, true);
    quality_ph.join(info->builder->read_timeout_ms, true);
    weight_ph.join(info->builder->read_timeout_ms, true);
    lat_ph.join(info->builder->read_timeout_ms, true);
    lon_ph.join(info->builder->read_timeout_ms, true);
    delta_time.join(info->builder->read_timeout_ms, true);
    bckgrd_delta_time.join(info->builder->read_timeout_ms, true);
    bckgrd_rate.join(info->builder->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Atl03BathyReader::Atl03Data::~Atl03Data (void)
{
}

/*----------------------------------------------------------------------------
 * Atl09Class::Constructor
 *----------------------------------------------------------------------------*/
Atl03BathyReader::Atl09Class::Atl09Class (info_t* info):
    valid       (false),
    met_u10m    (info->builder->asset, info->builder->resource09, FString("profile_%d/low_rate/met_u10m", info->track).c_str(), &info->builder->context09),
    met_v10m    (info->builder->asset, info->builder->resource09, FString("profile_%d/low_rate/met_v10m", info->track).c_str(), &info->builder->context09)
{
    try
    {
        met_u10m.join(info->builder->read_timeout_ms, true);
        met_v10m.join(info->builder->read_timeout_ms, true);
        valid = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "ATL09 data unavailable for %s", info->builder->resource09);
    }
}

/*----------------------------------------------------------------------------
 * Atl09Class::Destructor
 *----------------------------------------------------------------------------*/
Atl03BathyReader::Atl09Class::~Atl09Class (void)
{
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl03BathyReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Atl03BathyReader* builder = info->builder;
    BathyParms* parms = builder->parms;

    /* Thread Variables */
    fileptr_t out_file = NULL;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, builder->traceId, "atl03_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", info->builder->asset->getName(), info->builder->resource, info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {        
        /* Subset to Region of Interest */
        Region region(info);

        /* Read ATL03/09 Datasets */
        Atl03Data atl03(info, region);
        Atl09Class atl09(info);

        /* Initialize Extent Counter */
        List<photon_t> extent_photons;     // list of individual photons in extent
        uint32_t extent_counter = 0;
        int32_t current_photon = 0; // index into the photon rate variables 
        int32_t current_segment = 0; // index into the segment rate variables
        int32_t photon_in_segment = 0; // the photon number in the current segment
        int32_t photon_in_extent = 0; // the photon index in the current extent
        int32_t bckgrd_in = 0;

        /* Get Set Level Parameters */
        GeoLib::utm_zone_t utm = GeoLib::calcUTMZone(region.segment_lat[0], region.segment_lon[0]);
        GeoLib::utm_transform_t utm_transform = GeoLib::getUTMTransform(utm);

        /* Traverse All Photons In Dataset */
        while(builder->active && (current_photon < atl03.dist_ph_along.size))
        {
            /* Go to Photon's Segment */
            photon_in_segment++;
            while((current_segment < region.segment_ph_cnt.size) &&
                  (photon_in_segment > region.segment_ph_cnt[current_segment]))
            {
                photon_in_segment = 1; // reset photons in segment
                current_segment++; // go to next segment
            }

            /* Check Current Segment */
            if(current_segment >= atl03.segment_dist_x.size)
            {
                mlog(ERROR, "Photons with no segments are detected in %s/%d (%d %ld %ld)!", info->builder->resource, info->track, current_segment, atl03.segment_dist_x.size, region.num_segments);
                break;
            }

            do
            {
                /* Check and Set Signal Confidence Level */
                int8_t atl03_cnf = atl03.signal_conf_ph[current_photon];
                if(atl03_cnf < Icesat2Parms::CNF_POSSIBLE_TEP || atl03_cnf > Icesat2Parms::CNF_SURFACE_HIGH)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 signal confidence: %d", atl03_cnf);
                }
                if(!parms->atl03_cnf[atl03_cnf + Icesat2Parms::SIGNAL_CONF_OFFSET])
                {
                    break;
                }

                /* Check and Set ATL03 Photon Quality Level */
                int8_t quality_ph = atl03.quality_ph[current_photon];
                if(quality_ph < Icesat2Parms::QUALITY_NOMINAL || quality_ph > Icesat2Parms::QUALITY_POSSIBLE_TEP)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 photon quality: %d", quality_ph);
                }
                if(!parms->quality_ph[quality_ph])
                {
                    break;
                }

                /* Check and Set YAPC Score */
                uint8_t yapc_score = atl03.weight_ph[current_photon];
                if(yapc_score < parms->yapc.score)
                {
                    break;
                }

                /* Check Region */
                if(region.inclusion_ptr)
                {
                    if(!region.inclusion_ptr[current_segment])
                    {
                        break;
                    }
                }

                /* Calculate UTM Coordinates */
                double latitude = atl03.lat_ph[current_photon];
                double longitude = atl03.lon_ph[current_photon];
                GeoLib::point_t coord;
                if(!GeoLib::calcUTMCoordinates(utm_transform, latitude, longitude, coord))
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "unable to convert %ld,%ld to UTM zone %d", latitude, longitude, utm.zone);
                }

                /* Add Photon to Extent */
                photon_t ph = {
                    .time_ns = Icesat2Parms::deltatime2timestamp(atl03.delta_time[current_photon]),
                    .index_ph = static_cast<int32_t>(region.first_photon) + current_photon,
                    .geoid_corr_h = 0.0, // TODO
                    .latitude = latitude,
                    .longitude = longitude,
                    .x_ph = coord.x,
                    .y_ph = coord.y,
                    .x_atc = atl03.segment_dist_x[current_segment] + atl03.dist_ph_along[current_photon],
                    .y_atc = atl03.dist_ph_across[current_photon],
                    .sigma_along = 0.0, // TODO
                    .sigma_across = 0.0, // TODO
                    .ndwi = 0.0, // TODO
                    .yapc_score = yapc_score,
                    .max_signal_conf = 0, // TODO
                    .quality_ph = (int8_t)quality_ph,
                };
                extent_photons.add(ph);
            } while(false);

            /* Go to Next Photon */
            current_photon++;
            photon_in_extent++;

            if((photon_in_extent >= parms->ph_in_extent) || 
               (current_photon >= atl03.dist_ph_along.size))
            {
                bool extent_valid = true;

                /* Check Photon Count */
                if(extent_photons.length() < parms->minimum_photon_count)
                {
                    extent_valid = false;
                }

                /* Check Along Track Spread */
                if(extent_photons.length() > 1)
                {
                    int32_t last = extent_photons.length() - 1;
                    double along_track_spread = extent_photons[last].x_atc - extent_photons[0].x_atc;
                    if(along_track_spread < parms->max_along_track_spread)
                    {
                        extent_valid = false;
                    }
                }

                /* Post Extent Record */
                if(extent_valid || parms->pass_invalid)
                {
                    /* Generate Extent ID */
                    uint64_t extent_id = Icesat2Parms::generateExtentId(builder->start_rgt, builder->start_cycle, builder->start_region, info->track, info->pair, extent_counter);

                    /* Calculate Extent Record Size */
                    int num_photons = extent_photons.length();
                    int extent_bytes = offsetof(extent_t, photons) + (sizeof(photon_t) * num_photons);

                    /* Allocate and Initialize Extent Record */
                    RecordObject record(exRecType, extent_bytes);
                    extent_t* extent                = (extent_t*)record.getRecordData();
                    extent->region                  = builder->start_region;
                    extent->track                   = info->track;
                    extent->pair                    = info->pair;
                    extent->spacecraft_orientation  = atl03.sc_orient[0];
                    extent->reference_ground_track  = builder->start_rgt;
                    extent->cycle                   = builder->start_cycle;
                    extent->utm_zone                = utm.zone;
                    extent->photon_count            = extent_photons.length();
                    extent->solar_elevation         = atl03.solar_elevation[current_segment];
                    extent->wind_v                  = 0.0; // TODO
                    extent->pointing_angle          = 0.0; // TODO
                    extent->background_rate         = calculateBackground(current_segment, bckgrd_in, atl03);
                    extent->extent_id               = extent_id;

                    /* Populate Photons */
                    for(int32_t p = 0; p < extent_photons.length(); p++)
                    {
                        extent->photons[p] = extent_photons[p];
                    }

                    /* Export Data */
                    if(parms->beam_file_prefix == NULL)
                    {
                        /* Post Record */
                        uint8_t* rec_buf = NULL;
                        int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
                        int post_status = MsgQ::STATE_TIMEOUT;
                        while(builder->active && (post_status = builder->outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);
                        if(post_status <= 0)
                        {
                            mlog(ERROR, "Atl03 bathy reader failed to post %s to stream %s: %d", record.getRecordType(), builder->outQ->getName(), post_status);
                        }
                    }
                    else
                    {
                        if(out_file == NULL)
                        {
                            /* Open JSON File */
                            FString json_filename("%s_beam_%d.json", parms->beam_file_prefix, info->beam);
                            fileptr_t json_file = fopen(json_filename.c_str(), "w");
                            if(json_file == NULL)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to create output json file %s: %s", json_filename.c_str(), strerror(errno));
                            }

/* 
 * Build JSON File
 *  block indentation to preserve tabs in destination file 
 */
FString json_contents(R"json({
    "track": "%d",
    "pair": %d,
    "beam": "gt%d%c",
    "sc_orient": "%s",
    "region": %d,
    "rgt": %d,
    "cycle": %d,
    "utm_zone": %d,
    "wind_v": %f,
    "pointing_angle": %f
})json",
    extent->track, 
    extent->pair, 
    extent->track, extent->pair == 0 ? 'l' : 'r', 
    extent->spacecraft_orientation == Icesat2Parms::SC_BACKWARD ? "backward" : "forward", 
    extent->region, 
    extent->reference_ground_track,
    extent->cycle,
    extent->utm_zone,
    extent->wind_v,
    extent->pointing_angle);

                            /* Write and Close JSON File */
                            fprintf(json_file, "%s", json_contents.c_str());
                            fclose(json_file);

                            /* Open Data File */
                            FString filename("%s_beam_%d.csv", parms->beam_file_prefix, info->beam);
                            out_file = fopen(filename.c_str(), "w");
                            if(out_file == NULL)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to create output daata file %s: %s", filename.c_str(), strerror(errno));
                            }

                            /* Write Header */
                            fprintf(out_file, "index_ph,time,geoid_corr_h,latitude,longitude,x_ph,y_ph,x_atc,y_atc,sigma_along,sigma_across,ndwi,yapc_score,max_signal_conf,quality_ph,background_rate,solar_elevation\n");
                        }

                        /* Write Data */
                        for(unsigned i = 0; i < extent->photon_count; i++)
                        {
                            fprintf(out_file, "%d,", extent->photons[i].index_ph);
                            fprintf(out_file, "%ld,", extent->photons[i].time_ns);
                            fprintf(out_file, "%f,", extent->photons[i].geoid_corr_h);
                            fprintf(out_file, "%lf,", extent->photons[i].latitude);
                            fprintf(out_file, "%lf,", extent->photons[i].longitude);
                            fprintf(out_file, "%lf,", extent->photons[i].x_ph);
                            fprintf(out_file, "%lf,", extent->photons[i].y_ph);
                            fprintf(out_file, "%lf,", extent->photons[i].x_atc);
                            fprintf(out_file, "%lf,", extent->photons[i].y_atc);
                            fprintf(out_file, "%f,", extent->photons[i].sigma_along);
                            fprintf(out_file, "%f,", extent->photons[i].sigma_across);
                            fprintf(out_file, "%f,", extent->photons[i].ndwi);
                            fprintf(out_file, "%d,", extent->photons[i].yapc_score);
                            fprintf(out_file, "%d,", extent->photons[i].max_signal_conf);
                            fprintf(out_file, "%d,", extent->photons[i].quality_ph);
                            fprintf(out_file, "%lf,", extent->background_rate);
                            fprintf(out_file, "%f", extent->solar_elevation);
                            fprintf(out_file, "\n");
                        }
                    }
                }

                /* Update Extent Counters */
                extent_counter++;
                photon_in_extent = 0;
                extent_photons.clear();
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), builder->outQ, &builder->active, "Failure on resource %s track %d: %s", info->builder->resource, info->track, e.what());
    }

    /* Close Output File (if open) */
    if(out_file)
    {
        fclose(out_file);
    }

    /* Handle Global Reader Updates */
    builder->threadMut.lock();
    {
        /* Count Completion */
        builder->numComplete++;
        if(builder->numComplete == builder->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", info->builder->resource);

            /* Indicate End of Data */
            if(builder->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(builder->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = builder->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s", status, info->builder->resource);
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s ... trying again", info->builder->resource);
                    }
                }
            }
            builder->signalComplete();
        }
    }
    builder->threadMut.unlock();

    /* Clean Up Info */
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * calculateBackground
 *----------------------------------------------------------------------------*/
double Atl03BathyReader::calculateBackground (int32_t current_segment, int32_t& bckgrd_in, const Atl03Data& atl03)
{
    double background_rate = atl03.bckgrd_rate[atl03.bckgrd_rate.size - 1];
    while(bckgrd_in < atl03.bckgrd_rate.size)
    {
        double curr_bckgrd_time = atl03.bckgrd_delta_time[bckgrd_in];
        double segment_time = atl03.segment_delta_time[current_segment];
        if(curr_bckgrd_time >= segment_time)
        {
            /* Interpolate Background Rate */
            if(bckgrd_in > 0)
            {
                double prev_bckgrd_time = atl03.bckgrd_delta_time[bckgrd_in - 1];
                double prev_bckgrd_rate = atl03.bckgrd_rate[bckgrd_in - 1];
                double curr_bckgrd_rate = atl03.bckgrd_rate[bckgrd_in];

                double bckgrd_run = curr_bckgrd_time - prev_bckgrd_time;
                double bckgrd_rise = curr_bckgrd_rate - prev_bckgrd_rate;
                double segment_to_bckgrd_delta = segment_time - prev_bckgrd_time;

                background_rate = ((bckgrd_rise / bckgrd_run) * segment_to_bckgrd_delta) + prev_bckgrd_rate;
            }
            else
            {
                /* Use First Background Rate (no interpolation) */
                background_rate = atl03.bckgrd_rate[0];
            }
            break;
        }

        /* Go To Next Background Rate */
        bckgrd_in++;
    }
    return background_rate;
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
void Atl03BathyReader::parseResource (const char* _resource, uint16_t& rgt, uint8_t& cycle, uint8_t& region)
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
        rgt = (uint16_t)val;
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
        cycle = (uint8_t)val;
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
        region = (uint8_t)val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Region from resource %s: %s", _resource, region_str);
    }
}
