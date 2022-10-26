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
 * DEFINES
 ******************************************************************************/

#define LUA_STAT_SEGMENTS_READ          "read"
#define LUA_STAT_EXTENTS_FILTERED       "filtered"
#define LUA_STAT_EXTENTS_SENT           "sent"
#define LUA_STAT_EXTENTS_DROPPED        "dropped"
#define LUA_STAT_EXTENTS_RETRIED        "retried"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Reader::phRecType = "atl03rec.photons";
const RecordObject::fieldDef_t Atl03Reader::phRecDef[] = {
    {"delta_time",  RecordObject::DOUBLE,   offsetof(photon_t, delta_time),     1,  NULL, NATIVE_FLAGS},
    {"latitude",    RecordObject::DOUBLE,   offsetof(photon_t, latitude),       1,  NULL, NATIVE_FLAGS},
    {"longitude",   RecordObject::DOUBLE,   offsetof(photon_t, longitude),      1,  NULL, NATIVE_FLAGS},
    {"distance",    RecordObject::DOUBLE,   offsetof(photon_t, distance),       1,  NULL, NATIVE_FLAGS},
    {"height",      RecordObject::FLOAT,    offsetof(photon_t, height),         1,  NULL, NATIVE_FLAGS},
    {"atl08_class", RecordObject::UINT8,    offsetof(photon_t, atl08_class),    1,  NULL, NATIVE_FLAGS},
    {"atl03_cnf",   RecordObject::INT8,     offsetof(photon_t, atl03_cnf),      1,  NULL, NATIVE_FLAGS},
    {"quality_ph",  RecordObject::INT8,     offsetof(photon_t, quality_ph),     1,  NULL, NATIVE_FLAGS},
    {"yapc_score",  RecordObject::UINT8,    offsetof(photon_t, yapc_score),     1,  NULL, NATIVE_FLAGS}
};

const char* Atl03Reader::exRecType = "atl03rec";
const RecordObject::fieldDef_t Atl03Reader::exRecDef[] = {
    {"track",       RecordObject::UINT8,    offsetof(extent_t, reference_pair_track),           1,  NULL, NATIVE_FLAGS},
    {"sc_orient",   RecordObject::UINT8,    offsetof(extent_t, spacecraft_orientation),         1,  NULL, NATIVE_FLAGS},
    {"rgt",         RecordObject::UINT16,   offsetof(extent_t, reference_ground_track_start),   1,  NULL, NATIVE_FLAGS},
    {"cycle",       RecordObject::UINT16,   offsetof(extent_t, cycle_start),                    1,  NULL, NATIVE_FLAGS},
    {"segment_id",  RecordObject::UINT32,   offsetof(extent_t, segment_id[0]),                  2,  NULL, NATIVE_FLAGS},
    {"segment_dist",RecordObject::DOUBLE,   offsetof(extent_t, segment_distance[0]),            2,  NULL, NATIVE_FLAGS}, // distance from equator
    {"count",       RecordObject::UINT32,   offsetof(extent_t, photon_count[0]),                2,  NULL, NATIVE_FLAGS},
    {"photons",     RecordObject::USER,     offsetof(extent_t, photon_offset[0]),               2,  phRecType, NATIVE_FLAGS | RecordObject::POINTER},
    {"data",        RecordObject::USER,     sizeof(extent_t),                                   0,  phRecType, NATIVE_FLAGS} // variable length
};

const double Atl03Reader::ATL03_SEGMENT_LENGTH = 20.0; // meters

const char* Atl03Reader::OBJECT_TYPE = "Atl03Reader";
const char* Atl03Reader::LuaMetaName = "Atl03Reader";
const struct luaL_Reg Atl03Reader::LuaMetaTable[] = {
    {"parms",       luaParms},
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>)
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        Asset* asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        atl06_parms_t* parms = getLuaAtl06Parms(L, 4);
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Atl03Reader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Atl03Reader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Reader::init (void)
{
    RecordObject::recordDefErr_t ex_rc = RecordObject::defineRecord(exRecType, "track", sizeof(extent_t), exRecDef, sizeof(exRecDef) / sizeof(RecordObject::fieldDef_t));
    if(ex_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", exRecType, ex_rc);
    }

    RecordObject::recordDefErr_t ph_rc = RecordObject::defineRecord(phRecType, NULL, sizeof(photon_t), phRecDef, sizeof(phRecDef) / sizeof(RecordObject::fieldDef_t));
    if(ph_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", phRecType, ph_rc);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl03Reader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, atl06_parms_t* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;

    /* Generate ATL08 Resource Name */
    SafeString atl08_resource("%s", resource);
    atl08_resource.setChar('8', 4);
    resource08 = atl08_resource.getString(true);

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
    LocalLib::set(readerPid, 0, sizeof(readerPid));

    /* Initialize Global Information to Null */
    sc_orient       = NULL;
    start_rgt       = NULL;
    start_cycle     = NULL;
    atl08_rgt       = NULL;

    /* Read Global Resource Information */
    try
    {
        /* Read ATL03 Global Data */
        sc_orient       = new H5Array<int8_t> (asset, resource, "/orbit_info/sc_orient", &context);
        start_rgt       = new H5Array<int32_t>(asset, resource, "/ancillary_data/start_rgt", &context);
        start_cycle     = new H5Array<int32_t>(asset, resource, "/ancillary_data/start_cycle", &context);

        /* Read ATL08 File (if necessary) */
        if(parms->stages[STAGE_ATL08])
        {
            atl08_rgt = new H5Array<int32_t>(asset, resource08, "/ancillary_data/start_rgt", &context08);
        }

        /* Set Metrics */
        PluginMetrics::setRegion(parms);

        /* Join Global Data */
        sc_orient->join(H5_READ_TIMEOUT_MS, true);
        start_rgt->join(H5_READ_TIMEOUT_MS, true);
        start_cycle->join(H5_READ_TIMEOUT_MS, true);

        /* Wait for ATL08 File (if necessary) */
        if(parms->stages[STAGE_ATL08])
        {
            atl08_rgt->join(H5_READ_TIMEOUT_MS, true);
        }

        /* Read ATL03 Track Data */
        if(parms->track == ALL_TRACKS)
        {
            threadCount = NUM_TRACKS;

            /* Create Readers */
            for(int t = 0; t < NUM_TRACKS; t++)
            {
                info_t* info = new info_t;
                info->reader = this;
                info->track = t + 1;
                readerPid[t] = new Thread(subsettingThread, info);
            }
        }
        else if(parms->track >= 1 && parms->track <= 3)
        {
            /* Execute Reader */
            threadCount = 1;
            info_t* info = new info_t;
            info->reader = this;
            info->track = parms->track;
            subsettingThread(info);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid track specified <%d>, must be 1 to 3, or 0 for all", parms->track);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Log Error */
        mlog(e.level(), "Failed to read global information in resource %s: %s", resource, e.what());

        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) LuaEndpoint::generateExceptionStatus(RTE_TIMEOUT, e.level(), outQ, &active, "%s: (%s)", e.what(), resource);
        else LuaEndpoint::generateExceptionStatus(RTE_RESOURCE_DOES_NOT_EXIST, e.level(), outQ, &active, "%s: (%s)", e.what(), resource);

        /* Indicate End of Data */
        if(sendTerminator) outQ->postCopy("", 0);
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::~Atl03Reader (void)
{
    active = false;

    for(int t = 0; t < NUM_TRACKS; t++)
    {
        if(readerPid[t]) delete readerPid[t];
    }

    delete outQ;
    if(parms->raster) delete parms->raster;
    delete parms;
    delete [] resource;
    delete [] resource08;

    if(sc_orient)       delete sc_orient;
    if(start_rgt)       delete start_rgt;
    if(start_cycle)     delete start_cycle;
    if(atl08_rgt)       delete atl08_rgt;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Region::Region (info_t* info):
    segment_lat    (info->reader->asset, info->reader->resource, info->track, "geolocation/reference_photon_lat", &info->reader->context),
    segment_lon    (info->reader->asset, info->reader->resource, info->track, "geolocation/reference_photon_lon", &info->reader->context),
    segment_ph_cnt (info->reader->asset, info->reader->resource, info->track, "geolocation/segment_ph_cnt",       &info->reader->context),
    inclusion_mask {NULL, NULL},
    inclusion_ptr  {NULL, NULL}
{
    /* Join Reads */
    segment_lat.join(H5_READ_TIMEOUT_MS, true);
    segment_lon.join(H5_READ_TIMEOUT_MS, true);
    segment_ph_cnt.join(H5_READ_TIMEOUT_MS, true);

    /* Initialize Region */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        first_segment[t] = 0;
        num_segments[t] = -1;
        first_photon[t] = 0;
        num_photons[t] = -1;
    }

    /* Determine Spatial Extent */
    if(info->reader->parms->raster != NULL)
    {
        rasterregion(info);
    }
    else if(info->reader->parms->polygon.length() > 0)
    {
        polyregion(info);
    }
    else
    {
        return; // early exit since no subsetting required
    }

    /* Check If Anything to Process */
    if(num_photons[PRT_LEFT] <= 0 || num_photons[PRT_RIGHT] <= 0)
    {
        throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
    }

    /* Trim Geospatial Extent Datasets Read from HDF5 File */
    segment_lat.trim(first_segment);
    segment_lon.trim(first_segment);
    segment_ph_cnt.trim(first_segment);
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Region::~Region (void)
{
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        if(inclusion_mask[t]) delete [] inclusion_mask[t];
    }
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl03Reader::Region::polyregion (info_t* info)
{
    int points_in_polygon = info->reader->parms->polygon.length();

    /* Determine Best Projection To Use */
    MathLib::proj_t projection = MathLib::PLATE_CARREE;
    if(segment_lat.gt[PRT_LEFT][0] > 70.0) projection = MathLib::NORTH_POLAR;
    else if(segment_lat.gt[PRT_LEFT][0] < -70.0) projection = MathLib::SOUTH_POLAR;

    /* Project Polygon */
    List<MathLib::coord_t>::Iterator poly_iterator(info->reader->parms->polygon);
    MathLib::point_t* projected_poly = new MathLib::point_t [points_in_polygon];
    for(int i = 0; i < points_in_polygon; i++)
    {
        projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
    }

    /* Find First Segment In Polygon */
    bool first_segment_found[PAIR_TRACKS_PER_GROUND_TRACK] = {false, false};
    bool last_segment_found[PAIR_TRACKS_PER_GROUND_TRACK] = {false, false};
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        int segment = 0;
        while(segment < segment_ph_cnt.gt[t].size)
        {
            bool inclusion = false;

            /* Project Segment Coordinate */
            MathLib::coord_t segment_coord = {segment_lon.gt[t][segment], segment_lat.gt[t][segment]};
            MathLib::point_t segment_point = MathLib::coord2point(segment_coord, projection);

            /* Test Inclusion */
            if(MathLib::inpoly(projected_poly, points_in_polygon, segment_point))
            {
                inclusion = true;
            }

            /* Check First Segment */
            if(!first_segment_found[t])
            {
                /* If Coordinate Is In Polygon */
                if(inclusion && segment_ph_cnt.gt[t][segment] != 0)
                {
                    /* Set First Segment */
                    first_segment_found[t] = true;
                    first_segment[t] = segment;

                    /* Include Photons From First Segment */
                    num_photons[t] = segment_ph_cnt.gt[t][segment];
                }
                else
                {
                    /* Update Photon Index */
                    first_photon[t] += segment_ph_cnt.gt[t][segment];
                }
            }
            else if(!last_segment_found[t])
            {
                /* If Coordinate Is NOT In Polygon */
                if(!inclusion && segment_ph_cnt.gt[t][segment] != 0)
                {
                    /* Set Last Segment */
                    last_segment_found[t] = true;
                    break; // full extent found!
                }
                else
                {
                    /* Update Photon Index */
                    num_photons[t] += segment_ph_cnt.gt[t][segment];
                }
            }

            /* Bump Segment */
            segment++;
        }

        /* Set Number of Segments */
        if(first_segment_found[t])
        {
            num_segments[t] = segment - first_segment[t];
        }
    }

    /* Delete Projected Polygon */
    delete [] projected_poly;
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void Atl03Reader::Region::rasterregion (info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found[PAIR_TRACKS_PER_GROUND_TRACK] = {false, false};
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Check Size */
        if(segment_ph_cnt.gt[t].size <= 0)
        {
            continue;
        }

        /* Allocate Inclusion Mask */
        inclusion_mask[t] = new bool [segment_ph_cnt.gt[t].size];
        inclusion_ptr[t] = inclusion_mask[t];

        /* Loop Throuh Segments */
        long curr_num_photons = 0;
        long last_segment = 0;
        int segment = 0;
        while(segment < segment_ph_cnt.gt[t].size)
        {
            if(segment_ph_cnt.gt[t][segment] != 0)
            {
                /* Check Inclusion */
                bool inclusion = info->reader->parms->raster->subset(segment_lon.gt[t][segment], segment_lat.gt[t][segment]);
                inclusion_mask[t][segment] = inclusion;

                /* Check For First Segment */
                if(!first_segment_found[t])
                {
                    /* If Coordinate Is In Raster */
                    if(inclusion)
                    {
                        first_segment_found[t] = true;

                        /* Set First Segment */
                        first_segment[t] = segment;
                        last_segment = segment;

                        /* Include Photons From First Segment */
                        curr_num_photons = segment_ph_cnt.gt[t][segment];
                        num_photons[t] = curr_num_photons;
                    }
                    else
                    {
                        /* Update Photon Index */
                        first_photon[t] += segment_ph_cnt.gt[t][segment];
                    }
                }
                else
                {
                    /* Update Photon Count and Segment */
                    curr_num_photons += segment_ph_cnt.gt[t][segment];

                    /* If Coordinate Is In Raster */
                    if(inclusion)
                    {
                        /* Update Number of Photons to Current Count */
                        num_photons[t] = curr_num_photons;

                        /* Update Number of Segments to Current Segment Count */
                        last_segment = segment;
                    }
                }
            }

            /* Bump Segment */
            segment++;
        }

        /* Set Number of Segments */
        if(first_segment_found[t])
        {
            num_segments[t] = last_segment - first_segment[t] + 1;

            /* Trim Inclusion Mask */
            inclusion_ptr[t] = &inclusion_mask[t][first_segment[t]];
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl03Data::Atl03Data (info_t* info, Region* region):
    velocity_sc         (info->reader->asset, info->reader->resource, info->track, "geolocation/velocity_sc",     &info->reader->context, H5Coro::ALL_COLS, region->first_segment, region->num_segments),
    segment_delta_time  (info->reader->asset, info->reader->resource, info->track, "geolocation/delta_time",      &info->reader->context, 0, region->first_segment, region->num_segments),
    segment_id          (info->reader->asset, info->reader->resource, info->track, "geolocation/segment_id",      &info->reader->context, 0, region->first_segment, region->num_segments),
    segment_dist_x      (info->reader->asset, info->reader->resource, info->track, "geolocation/segment_dist_x",  &info->reader->context, 0, region->first_segment, region->num_segments),
    dist_ph_along       (info->reader->asset, info->reader->resource, info->track, "heights/dist_ph_along",       &info->reader->context, 0, region->first_photon,  region->num_photons),
    h_ph                (info->reader->asset, info->reader->resource, info->track, "heights/h_ph",                &info->reader->context, 0, region->first_photon,  region->num_photons),
    signal_conf_ph      (info->reader->asset, info->reader->resource, info->track, "heights/signal_conf_ph",      &info->reader->context, info->reader->parms->surface_type, region->first_photon,  region->num_photons),
    quality_ph          (info->reader->asset, info->reader->resource, info->track, "heights/quality_ph",          &info->reader->context, 0, region->first_photon,  region->num_photons),
    lat_ph              (info->reader->asset, info->reader->resource, info->track, "heights/lat_ph",              &info->reader->context, 0, region->first_photon,  region->num_photons),
    lon_ph              (info->reader->asset, info->reader->resource, info->track, "heights/lon_ph",              &info->reader->context, 0, region->first_photon,  region->num_photons),
    delta_time          (info->reader->asset, info->reader->resource, info->track, "heights/delta_time",          &info->reader->context, 0, region->first_photon,  region->num_photons),
    bckgrd_delta_time   (info->reader->asset, info->reader->resource, info->track, "bckgrd_atlas/delta_time",     &info->reader->context),
    bckgrd_rate         (info->reader->asset, info->reader->resource, info->track, "bckgrd_atlas/bckgrd_rate",    &info->reader->context)
{
    /* Join Reads */
    velocity_sc.join(H5_READ_TIMEOUT_MS, true);
    segment_delta_time.join(H5_READ_TIMEOUT_MS, true);
    segment_id.join(H5_READ_TIMEOUT_MS, true);
    segment_dist_x.join(H5_READ_TIMEOUT_MS, true);
    dist_ph_along.join(H5_READ_TIMEOUT_MS, true);
    h_ph.join(H5_READ_TIMEOUT_MS, true);
    signal_conf_ph.join(H5_READ_TIMEOUT_MS, true);
    quality_ph.join(H5_READ_TIMEOUT_MS, true);
    lat_ph.join(H5_READ_TIMEOUT_MS, true);
    lon_ph.join(H5_READ_TIMEOUT_MS, true);
    delta_time.join(H5_READ_TIMEOUT_MS, true);
    bckgrd_delta_time.join(H5_READ_TIMEOUT_MS, true);
    bckgrd_rate.join(H5_READ_TIMEOUT_MS, true);
}

/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl03Data::~Atl03Data (void)
{
}

/*----------------------------------------------------------------------------
 * Atl08Class::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl08Class::Atl08Class (info_t* info):
    enabled             (info->reader->parms->stages[STAGE_ATL08]),
    gt                  {NULL, NULL},
    atl08_segment_id    (enabled ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/ph_segment_id",   &info->reader->context08),
    atl08_pc_indx       (enabled ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/classed_pc_indx", &info->reader->context08),
    atl08_pc_flag       (enabled ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/classed_pc_flag", &info->reader->context08)
{
}

/*----------------------------------------------------------------------------
 * Atl08Class::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl08Class::~Atl08Class (void)
{
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        if(gt[t]) delete [] gt[t];
    }
}

/*----------------------------------------------------------------------------
 * Atl08Class::classify
 *----------------------------------------------------------------------------*/
void Atl03Reader::Atl08Class::classify (info_t* info, Region* region, Atl03Data* data)
{
    /* Do Nothing If Not Enabled */
    if(!info->reader->parms->stages[STAGE_ATL08])
    {
        return;
    }

    /* Wait for Reads to Complete */
    atl08_segment_id.join(H5_READ_TIMEOUT_MS, true);
    atl08_pc_indx.join(H5_READ_TIMEOUT_MS, true);
    atl08_pc_flag.join(H5_READ_TIMEOUT_MS, true);

    /* Rename Segment Photon Counts (to easily identify with ATL03) */
    GTArray<int32_t>* atl03_segment_ph_cnt = &region->segment_ph_cnt;

    /* Classify Photons */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Allocate ATL08 Classification Array */
        int num_photons = data->dist_ph_along.gt[t].size;
        gt[t] = new uint8_t [num_photons];

        /* Populate ATL08 Classifications */
        int32_t atl03_photon = 0;
        int32_t atl08_photon = 0;
        for(int atl03_segment_index = 0; atl03_segment_index < data->segment_id.gt[t].size; atl03_segment_index++)
        {
            int32_t atl03_segment = data->segment_id.gt[t][atl03_segment_index];
            int32_t atl03_segment_count = atl03_segment_ph_cnt->gt[t][atl03_segment_index];
            for(int atl03_count = 1; atl03_count <= atl03_segment_count; atl03_count++)
            {
                /* Go To Segment */
                while( (atl08_photon < atl08_segment_id.gt[t].size) && // atl08 photon is valid
                       (atl08_segment_id.gt[t][atl08_photon] < atl03_segment) )
                {
                    atl08_photon++;
                }

                while( (atl08_photon < atl08_segment_id.gt[t].size) && // atl08 photon is valid
                       (atl08_segment_id.gt[t][atl08_photon] == atl03_segment) &&
                       (atl08_pc_indx.gt[t][atl08_photon] < atl03_count))
                {
                    atl08_photon++;
                }

                /* Check Match */
                if( (atl08_photon < atl08_segment_id.gt[t].size) &&
                    (atl08_segment_id.gt[t][atl08_photon] == atl03_segment) &&
                    (atl08_pc_indx.gt[t][atl08_photon] == atl03_count) )
                {
                    /* Assign Classification */
                    gt[t][atl03_photon] = (uint8_t)atl08_pc_flag.gt[t][atl08_photon];

                    /* Go To Next ATL08 Photon */
                    atl08_photon++;
                }
                else
                {
                    /* Unclassified */
                    gt[t][atl03_photon] = ATL08_UNCLASSIFIED;
                }

                /* Go To Next ATL03 Photon */
                atl03_photon++;
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * YapcScore::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::YapcScore::YapcScore (info_t* info, Region* region, Atl03Data* data):
    gt {NULL, NULL}
{
    /* Do Nothing If Not Enabled */
    if(!info->reader->parms->stages[STAGE_YAPC])
    {
        return;
    }

    /* Run YAPC */
    if(info->reader->parms->yapc.version == 3)
    {
        yapcV3(info, region, data);
    }
    else if(info->reader->parms->yapc.version == 2 || info->reader->parms->yapc.version == 1)
    {
        yapcV2(info, region, data);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid YAPC version specified: %d", info->reader->parms->yapc.version);
    }
}

/*----------------------------------------------------------------------------
 * yapcV2
 *----------------------------------------------------------------------------*/
void Atl03Reader::YapcScore::yapcV2 (info_t* info, Region* region, Atl03Data* data)
{
    /* YAPC Hard-Coded Parameters */
    const double MAXIMUM_HSPREAD = 15000.0; // meters
    const double HSPREAD_BINSIZE = 1.0; // meters
    const int MAX_KNN = 25;
    double nearest_neighbors[MAX_KNN];

    /* Shortcut to Settings */
    yapc_t* settings = &info->reader->parms->yapc;

    /* Score Photons
     *
     *   CANNOT THROW BELOW THIS POINT
     */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Allocate ATL08 Classification Array */
        int32_t num_photons = data->dist_ph_along.gt[t].size;
        gt[t] = new uint8_t [num_photons];
        LocalLib::set(gt[t], 0, num_photons);

        /* Initialize Indices */
        int32_t ph_b0 = 0; // buffer start
        int32_t ph_b1 = 0; // buffer end
        int32_t ph_c0 = 0; // center start
        int32_t ph_c1 = 0; // center end

        /* Loop Through Each ATL03 Segment */
        int32_t num_segments = data->segment_id.gt[t].size;
        for(int segment_index = 0; segment_index < num_segments; segment_index++)
        {
            /* Determine Indices */
            ph_b0 += segment_index > 1 ? region->segment_ph_cnt.gt[t][segment_index - 2] : 0; // Center - 2
            ph_c0 += segment_index > 0 ? region->segment_ph_cnt.gt[t][segment_index - 1] : 0; // Center - 1
            ph_c1 += region->segment_ph_cnt.gt[t][segment_index]; // Center
            ph_b1 += segment_index < (num_segments - 1) ? region->segment_ph_cnt.gt[t][segment_index + 1] : 0; // Center + 1

            /* Calculate N and KNN */
            int32_t N = region->segment_ph_cnt.gt[t][segment_index];
            int knn = (settings->knn != 0) ? settings->knn : MAX(1, (sqrt((double)N) + 0.5) / 2);
            knn = MIN(knn, MAX_KNN); // truncate if too large

            /* Check Valid Extent (note check against knn)*/
            if((N <= knn) || (N < info->reader->parms->minimum_photon_count)) continue;

            /* Calculate Distance and Height Spread */
            double min_h = data->h_ph.gt[t][0];
            double max_h = min_h;
            double min_x = data->dist_ph_along.gt[t][0];
            double max_x = min_x;
            for(int n = 1; n < N; n++)
            {
                double h = data->h_ph.gt[t][n];
                double x = data->dist_ph_along.gt[t][n];
                if(h < min_h) min_h = h;
                if(h > max_h) max_h = h;
                if(x < min_x) min_x = x;
                if(x > max_x) max_x = x;
            }
            double hspread = max_h - min_h;
            double xspread = max_x - min_x;

            /* Check Window */
            if(hspread <= 0.0 || hspread > MAXIMUM_HSPREAD || xspread <= 0.0)
            {
                mlog(ERROR, "Unable to perform YAPC selection due to invalid photon spread: %lf, %lf\n", hspread, xspread);
                continue;
            }

            /* Bin Photons to Calculate Height Span*/
            int num_bins = (int)(hspread / HSPREAD_BINSIZE) + 1;
            int8_t* bins = new int8_t [num_bins];
            LocalLib::set(bins, 0, num_bins);
            for(int n = 0; n < N; n++)
            {
                unsigned int bin = (unsigned int)((data->h_ph.gt[t][n] - min_h) / HSPREAD_BINSIZE);
                bins[bin] = 1; // mark that photon present
            }

            /* Determine Number of Bins with Photons to Calculate Height Span
            * (and remove potential gaps in telemetry bands) */
            int nonzero_bins = 0;
            for(int b = 0; b < num_bins; b++) nonzero_bins += bins[b];
            delete [] bins;

            /* Calculate Height Span */
            double h_span = (nonzero_bins * HSPREAD_BINSIZE) / (double)N * (double)knn;

            /* Calculate Window Parameters */
            double half_win_x = settings->win_x / 2.0;
            double half_win_h = (settings->win_h != 0.0) ? settings->win_h / 2.0 : h_span / 2.0;

            /* Calculate YAPC Score for all Photons in Center Segment */
            for(int y = ph_c0; y < ph_c1; y++)
            {
                double smallest_nearest_neighbor = DBL_MAX;
                int smallest_nearest_neighbor_index = 0;
                int num_nearest_neighbors = 0;

                /* For All Neighbors */
                for(int x = ph_b0; x < ph_b1; x++)
                {
                    /* Check for Identity */
                    if(y == x) continue;

                    /* Check Window */
                    double delta_x = abs(data->dist_ph_along.gt[t][x] - data->dist_ph_along.gt[t][y]);
                    if(delta_x > half_win_x) continue;

                    /*  Calculate Weighted Distance */
                    double delta_h = abs(data->h_ph.gt[t][x] - data->h_ph.gt[t][y]);
                    double proximity = half_win_h - delta_h;

                    /* Add to Nearest Neighbor */
                    if(num_nearest_neighbors < knn)
                    {
                        /* Maintain Smallest Nearest Neighbor */
                        if(proximity < smallest_nearest_neighbor)
                        {
                            smallest_nearest_neighbor = proximity;
                            smallest_nearest_neighbor_index = num_nearest_neighbors;
                        }

                        /* Automatically Add Nearest Neighbor (filling up array) */
                        nearest_neighbors[num_nearest_neighbors] = proximity;
                        num_nearest_neighbors++;
                    }
                    else if(proximity > smallest_nearest_neighbor)
                    {
                        /* Add New Nearest Neighbor (replace current largest) */
                        nearest_neighbors[smallest_nearest_neighbor_index] = proximity;
                        smallest_nearest_neighbor = proximity; // temporarily set

                        /* Recalculate Largest Nearest Neighbor */
                        for(int k = 0; k < knn; k++)
                        {
                            if(nearest_neighbors[k] < smallest_nearest_neighbor)
                            {
                                smallest_nearest_neighbor = nearest_neighbors[k];
                                smallest_nearest_neighbor_index = k;
                            }
                        }
                    }
                }

                /* Fill In Rest of Nearest Neighbors (if not already full) */
                for(int k = num_nearest_neighbors; k < knn; k++)
                {
                    nearest_neighbors[k] = 0.0;
                }

                /* Calculate Inverse Sum of Distances from Nearest Neighbors */
                double nearest_neighbor_sum = 0.0;
                for(int k = 0; k < knn; k++)
                {
                    if(nearest_neighbors[k] > 0.0)
                    {
                        nearest_neighbor_sum += nearest_neighbors[k];
                    }
                }
                nearest_neighbor_sum /= (double)knn;

                /* Calculate YAPC Score of Photon */
                gt[t][y] = (uint8_t)((nearest_neighbor_sum / half_win_h) * 0xFF);
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * yapcV3
 *----------------------------------------------------------------------------*/
void Atl03Reader::YapcScore::yapcV3 (info_t* info, Region* region, Atl03Data* data)
{
    /* YAPC Parameters */
    yapc_t* settings = &info->reader->parms->yapc;
    const double hWX = settings->win_x / 2; // meters
    const double hWZ = settings->win_h / 2; // meters

    /* Score Photons
     *
     *   CANNOT THROW BELOW THIS POINT
     */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        int32_t num_segments = data->segment_id.gt[t].size;
        int32_t num_photons = data->dist_ph_along.gt[t].size;

        /* Allocate Photon Arrays */
        gt[t] = new uint8_t [num_photons]; // class member freed in deconstructor
        double* ph_dist = new double[num_photons]; // local array freed below

        /* Populate Distance Array */
        int32_t ph_index = 0;
        for(int segment_index = 0; segment_index < num_segments; segment_index++)
        {
            for(int32_t ph_in_seg_index = 0; ph_in_seg_index < region->segment_ph_cnt.gt[t][segment_index]; ph_in_seg_index++)
            {
                ph_dist[ph_index] = data->segment_dist_x.gt[t][segment_index] + data->dist_ph_along.gt[t][ph_index];
                ph_index++;
            }
        }

        /* Traverse Each Segment */
        ph_index = 0;
        for(int segment_index = 0; segment_index < num_segments; segment_index++)
        {
            /* Initialize Segment Parameters */
            int32_t N = region->segment_ph_cnt.gt[t][segment_index];
            double* ph_weights = new double[N]; // local array freed below
            int max_knn = settings->min_knn;
            int32_t start_ph_index = ph_index;

            /* Traverse Each Photon in Segment*/
            for(int32_t ph_in_seg_index = 0; ph_in_seg_index < N; ph_in_seg_index++)
            {
                List<double> proximities;

                /* Check Nearest Neighbors to Left */
                int32_t neighbor_index = ph_index - 1;
                while(neighbor_index >= 0)
                {
                    /* Check Inside Horizontal Window */
                    double x_dist = ph_dist[ph_index] - ph_dist[neighbor_index];
                    if(x_dist <= hWX)
                    {
                        /* Check Inside Vertical Window */
                        double proximity = abs(data->h_ph.gt[t][ph_index] - data->h_ph.gt[t][neighbor_index]);
                        if(proximity <= hWZ)
                        {
                            proximities.add(proximity);
                        }
                    }

                    /* Check for Stopping Condition: 1m Buffer Added to X Window */
                    if(x_dist >= (hWX + 1.0)) break;

                    /* Goto Next Neighor */
                    neighbor_index--;
                }

                /* Check Nearest Neighbors to Right */
                neighbor_index = ph_index + 1;
                while(neighbor_index < num_photons)
                {
                    /* Check Inside Horizontal Window */
                    double x_dist = ph_dist[neighbor_index] - ph_dist[ph_index];
                    if(x_dist <= hWX)
                    {
                        /* Check Inside Vertical Window */
                        double proximity = abs(data->h_ph.gt[t][ph_index] - data->h_ph.gt[t][neighbor_index]);
                        if(proximity <= hWZ) // inside of height window
                        {
                            proximities.add(proximity);
                        }
                    }

                    /* Check for Stopping Condition: 1m Buffer Added to X Window */
                    if(x_dist >= (hWX + 1.0)) break;

                    /* Goto Next Neighor */
                    neighbor_index++;
                }

                /* Sort Proximities */
                proximities.sort();

                /* Calculate knn */
                double n = sqrt(proximities.length());
                int knn = MAX(n, settings->min_knn);
                if(knn > max_knn) max_knn = knn;

                /* Calculate Sum of Weights*/
                int num_nearest_neighbors = MIN(knn, proximities.length());
                double weight_sum = 0.0;
                for(int i = 0; i < num_nearest_neighbors; i++)
                {
                    weight_sum += hWZ - proximities[i];
                }
                ph_weights[ph_in_seg_index] = weight_sum;

                /* Go To Next Photon */
                ph_index++;
            }

            /* Normalize Weights */
            for(int32_t ph_in_seg_index = 0; ph_in_seg_index < N; ph_in_seg_index++)
            {
                double Wt = ph_weights[ph_in_seg_index] / (hWZ * max_knn);
                gt[t][start_ph_index] = (uint8_t)(MIN(Wt * 255, 255));
                start_ph_index++;
            }

            /* Free Photon Weights Array */
            delete [] ph_weights;
        }

        /* Free Photon Distance Array */
        delete [] ph_dist;
    }
}

/*----------------------------------------------------------------------------
 * YapcScore::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::YapcScore::~YapcScore (void)
{
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        if(gt[t]) delete [] gt[t];
    }
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl03Reader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Atl03Reader* reader = info->reader;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "atl03_reader", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", info->reader->asset->getName(), info->reader->resource, info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Start Reading ATL08 Data */
        Atl08Class atl08(info);

        /* Subset to Region of Interest */
        Region region(info);

        /* Read ATL03 Datasets */
        Atl03Data atl03(info, &region);

        /* Perform YAPC Scoring (if requested) */
        YapcScore yapc(info, &region, &atl03);

        /* Perform ATL08 Classification (if requested) */
        atl08.classify(info, &region, &atl03);

        /* Initialize Dataset Scope Variables */
        int32_t ph_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // photon index
        int32_t seg_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // segment index
        int32_t seg_ph[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // current photon index in segment
        int32_t start_segment[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // used to set start_distance
        double  start_distance[PAIR_TRACKS_PER_GROUND_TRACK] = { atl03.segment_dist_x.gt[PRT_LEFT][0], atl03.segment_dist_x.gt[PRT_RIGHT][0] };
        double  seg_distance[PAIR_TRACKS_PER_GROUND_TRACK] = { 0.0, 0.0 };
        double  start_seg_portion[PAIR_TRACKS_PER_GROUND_TRACK] = { 0.0, 0.0 };
        bool    track_complete[PAIR_TRACKS_PER_GROUND_TRACK] = { false, false };
        int32_t bckgrd_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // bckgrd index

        /* Increment Read Statistics */
        local_stats.segments_read = (region.segment_ph_cnt.gt[PRT_LEFT].size + region.segment_ph_cnt.gt[PRT_RIGHT].size);

        /* Calculate Length of Extent in Meters (used for distance) */
        double extent_distance = reader->parms->extent_length;
        if(reader->parms->dist_in_seg) extent_distance *= ATL03_SEGMENT_LENGTH;

        /* Traverse All Photons In Dataset */
        while( reader->active && (!track_complete[PRT_LEFT] || !track_complete[PRT_RIGHT]) )
        {
            List<photon_t> extent_photons[PAIR_TRACKS_PER_GROUND_TRACK];
            int32_t extent_segment[PAIR_TRACKS_PER_GROUND_TRACK];
            bool extent_valid[PAIR_TRACKS_PER_GROUND_TRACK] = { true, true };

            /* Select Photons for Extent from each Track */
            for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
            {
                /* Skip Completed Tracks */
                if(track_complete[t])
                {
                    extent_valid[t] = false;
                    continue;
                }

                /* Setup Variables for Extent */
                int32_t current_photon = ph_in[t];
                int32_t current_segment = seg_in[t];
                int32_t current_count = seg_ph[t]; // number of photons in current segment already accounted for
                bool extent_complete = false;
                bool step_complete = false;

                /* Set Extent Segment */
                extent_segment[t] = seg_in[t];
                start_seg_portion[t] = atl03.dist_ph_along.gt[t][current_photon] / ATL03_SEGMENT_LENGTH;

                /* Traverse Photons Until Desired Along Track Distance Reached */
                while(!extent_complete || !step_complete)
                {
                    /* Go to Photon's Segment */
                    current_count++;
                    while((current_segment < region.segment_ph_cnt.gt[t].size) &&
                          (current_count > region.segment_ph_cnt.gt[t][current_segment]))
                    {
                        current_count = 1; // reset photons in segment
                        current_segment++; // go to next segment
                    }

                    /* Check Current Segment */
                    if(current_segment >= atl03.segment_dist_x.gt[t].size)
                    {
                        mlog(ERROR, "Photons with no segments are detected is %s/%d     %d %ld %ld!", info->reader->resource, info->track, current_segment, atl03.segment_dist_x.gt[t].size, region.num_segments[t]);
                        track_complete[t] = true;
                        break;
                    }

                    /* Update Along Track Distance and Progress */
                    double delta_distance = atl03.segment_dist_x.gt[t][current_segment] - start_distance[t];
                    double along_track_distance = delta_distance + atl03.dist_ph_along.gt[t][current_photon];
                    int32_t along_track_segments = current_segment - extent_segment[t];

                    /* Set Next Extent's First Photon */
                    if((!step_complete) &&
                       ((!reader->parms->dist_in_seg && along_track_distance >= reader->parms->extent_step) ||
                        (reader->parms->dist_in_seg && along_track_segments >= (int32_t)reader->parms->extent_step)))
                    {
                        ph_in[t] = current_photon;
                        seg_in[t] = current_segment;
                        seg_ph[t] = current_count - 1;
                        step_complete = true;
                    }

                    /* Check if Photon within Extent's Length */
                    if((!reader->parms->dist_in_seg && along_track_distance < reader->parms->extent_length) ||
                       (reader->parms->dist_in_seg && along_track_segments < reader->parms->extent_length))
                    {
                        do
                        {
                            /* Check Signal Confidence Level */
                            int8_t atl03_cnf = atl03.signal_conf_ph.gt[t][current_photon];
                            if(atl03_cnf < CNF_POSSIBLE_TEP || atl03_cnf > CNF_SURFACE_HIGH)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 signal confidence: %d", atl03_cnf);
                            }
                            else if(!reader->parms->atl03_cnf[atl03_cnf + SIGNAL_CONF_OFFSET])
                            {
                                break;
                            }

                            /* Check ATL03 Photon Quality Level */
                            int8_t quality_ph = atl03.quality_ph.gt[t][current_photon];
                            if(quality_ph < QUALITY_NOMINAL || quality_ph > QUALITY_POSSIBLE_TEP)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 photon quality: %d", quality_ph);
                            }
                            else if(!reader->parms->quality_ph[quality_ph])
                            {
                                break;
                            }

                            /* Check ATL08 Classification */
                            atl08_classification_t atl08_class = ATL08_UNCLASSIFIED;
                            if(atl08.gt[t])
                            {
                                atl08_class = (atl08_classification_t)atl08.gt[t][current_photon];
                                if(atl08_class < 0 || atl08_class >= NUM_ATL08_CLASSES)
                                {
                                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl08 classification: %d", atl08_class);
                                }
                                else if(!reader->parms->atl08_class[atl08_class])
                                {
                                    break;
                                }
                            }

                            /* Check YAPC Score */
                            uint8_t yapc_score = 0;
                            if(yapc.gt[t])
                            {
                                yapc_score = yapc.gt[t][current_photon];
                                if(yapc_score < reader->parms->yapc.score)
                                {
                                    break;
                                }
                            }

                            /* Check Region */
                            if(region.inclusion_ptr[t])
                            {
                                if(!region.inclusion_ptr[t][current_segment])
                                {
                                    break;
                                }
                            }

                            /* Add Photon to Extent */
                            photon_t ph = {
                                .delta_time = atl03.delta_time.gt[t][current_photon],
                                .latitude = atl03.lat_ph.gt[t][current_photon],
                                .longitude = atl03.lon_ph.gt[t][current_photon],
                                .distance = along_track_distance - (extent_distance / 2.0),
                                .height = atl03.h_ph.gt[t][current_photon],
                                .atl08_class = (uint8_t)atl08_class,
                                .atl03_cnf = (int8_t)atl03_cnf,
                                .quality_ph = (int8_t)quality_ph,
                                .yapc_score = yapc_score
                            };
                            extent_photons[t].add(ph);
                        } while(false);
                    }
                    else
                    {
                        extent_complete = true;
                    }

                    /* Go to Next Photon */
                    current_photon++;

                    /* Check Current Photon */
                    if(current_photon >= atl03.dist_ph_along.gt[t].size)
                    {
                        track_complete[t] = true;
                        break;
                    }
                }

                /* Save Off Segment Distance to Include in Extent Record */
                seg_distance[t] = start_distance[t] + (extent_distance / 2.0);

                /* Add Step to Start Distance */
                if(!reader->parms->dist_in_seg)
                {
                    start_distance[t] += reader->parms->extent_step; // step start distance

                    /* Apply Segment Distance Correction and Update Start Segment */
                    while( ((start_segment[t] + 1) < atl03.segment_dist_x.gt[t].size) &&
                            (start_distance[t] >= atl03.segment_dist_x.gt[t][start_segment[t] + 1]) )
                    {
                        start_distance[t] += atl03.segment_dist_x.gt[t][start_segment[t] + 1] - atl03.segment_dist_x.gt[t][start_segment[t]];
                        start_distance[t] -= ATL03_SEGMENT_LENGTH;
                        start_segment[t]++;
                    }
                }
                else // distance in segments
                {
                    int32_t next_segment = extent_segment[t] + (int32_t)reader->parms->extent_step;
                    if(next_segment < atl03.segment_dist_x.gt[t].size)
                    {
                        start_distance[t] = atl03.segment_dist_x.gt[t][next_segment]; // set start distance to next extent's segment distance
                    }
                }

                /* Check Photon Count */
                if(extent_photons[t].length() < reader->parms->minimum_photon_count)
                {
                    extent_valid[t] = false;
                }

                /* Check Along Track Spread */
                if(extent_photons[t].length() > 1)
                {
                    int32_t last = extent_photons[t].length() - 1;
                    double along_track_spread = extent_photons[t][last].distance - extent_photons[t][0].distance;
                    if(along_track_spread < reader->parms->along_track_spread)
                    {
                        extent_valid[t] = false;
                    }
                }
            }

            /* Create Extent Record */
            if(extent_valid[PRT_LEFT] || extent_valid[PRT_RIGHT] || reader->parms->pass_invalid)
            {
                /* Calculate Extent Record Size */
                int num_photons = extent_photons[PRT_LEFT].length() + extent_photons[PRT_RIGHT].length();
                int extent_bytes = sizeof(extent_t) + (sizeof(photon_t) * num_photons);

                /* Allocate and Initialize Extent Record */
                RecordObject record(exRecType, extent_bytes);
                extent_t* extent = (extent_t*)record.getRecordData();
                extent->reference_pair_track = info->track;
                extent->spacecraft_orientation = (*reader->sc_orient)[0];
                extent->reference_ground_track_start = (*reader->start_rgt)[0];
                extent->cycle_start = (*reader->start_cycle)[0];

                /* Populate Extent */
                uint32_t ph_out = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Find Background */
                    double background_rate = atl03.bckgrd_rate.gt[t][atl03.bckgrd_rate.gt[t].size - 1];
                    while(bckgrd_in[t] < atl03.bckgrd_rate.gt[t].size)
                    {
                        double curr_bckgrd_time = atl03.bckgrd_delta_time.gt[t][bckgrd_in[t]];
                        double segment_time = atl03.segment_delta_time.gt[t][extent_segment[t]];
                        if(curr_bckgrd_time >= segment_time)
                        {
                            /* Interpolate Background Rate */
                            if(bckgrd_in[t] > 0)
                            {
                                double prev_bckgrd_time = atl03.bckgrd_delta_time.gt[t][bckgrd_in[t] - 1];
                                double prev_bckgrd_rate = atl03.bckgrd_rate.gt[t][bckgrd_in[t] - 1];
                                double curr_bckgrd_rate = atl03.bckgrd_rate.gt[t][bckgrd_in[t]];

                                double bckgrd_run = curr_bckgrd_time - prev_bckgrd_time;
                                double bckgrd_rise = curr_bckgrd_rate - prev_bckgrd_rate;
                                double segment_to_bckgrd_delta = segment_time - prev_bckgrd_time;

                                background_rate = ((bckgrd_rise / bckgrd_run) * segment_to_bckgrd_delta) + prev_bckgrd_rate;
                            }
                            else
                            {
                                /* Use First Background Rate (no interpolation) */
                                background_rate = atl03.bckgrd_rate.gt[t][0];
                            }
                            break;
                        }
                        else
                        {
                            /* Go To Next Background Rate */
                            bckgrd_in[t]++;
                        }
                    }

                    /* Calculate Spacecraft Velocity */
                    int32_t sc_v_offset = extent_segment[t] * 3;
                    double sc_v1 = atl03.velocity_sc.gt[t][sc_v_offset + 0];
                    double sc_v2 = atl03.velocity_sc.gt[t][sc_v_offset + 1];
                    double sc_v3 = atl03.velocity_sc.gt[t][sc_v_offset + 2];
                    double spacecraft_velocity = sqrt((sc_v1*sc_v1) + (sc_v2*sc_v2) + (sc_v3*sc_v3));

                    /* Calculate Segment ID (attempt to arrive at closest ATL06 segment ID represented by extent) */
                    double atl06_segment_id = (double)atl03.segment_id.gt[t][extent_segment[t]];                // start with first segment in extent
                    if(!reader->parms->dist_in_seg)
                    {
                        atl06_segment_id += start_seg_portion[t];                                               // add portion of first segment that first photon is included
                        atl06_segment_id += (int)((reader->parms->extent_length / ATL03_SEGMENT_LENGTH) / 2.0); // add half the length of the extent
                    }
                    else // dist_in_seg is true
                    {
                        atl06_segment_id += (int)(reader->parms->extent_length / 2.0);
                    }

                    /* Populate Attributes */
                    extent->valid[t]                = extent_valid[t];
                    extent->segment_id[t]           = (uint32_t)(atl06_segment_id + 0.5);
                    extent->segment_distance[t]     = seg_distance[t];
                    extent->extent_length[t]        = extent_distance;
                    extent->spacecraft_velocity[t]  = spacecraft_velocity;
                    extent->background_rate[t]      = background_rate;
                    extent->photon_count[t]         = extent_photons[t].length();

                    /* Populate Photons */
                    if(num_photons > 0)
                    {
                        for(int32_t p = 0; p < extent_photons[t].length(); p++)
                        {
                            extent->photons[ph_out++] = extent_photons[t][p];
                        }
                    }
                }

                /* Set Photon Pointer Fields */
                extent->photon_offset[PRT_LEFT] = sizeof(extent_t); // pointers are set to offset from start of record data
                extent->photon_offset[PRT_RIGHT] = sizeof(extent_t) + (sizeof(photon_t) * extent->photon_count[PRT_LEFT]);

                /* Post Segment Record */
                uint8_t* rec_buf = NULL;
                int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
                int post_status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (post_status = reader->outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT)
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
                    mlog(ERROR, "Atl03 reader failed to post to stream %s: %d", reader->outQ->getName(), post_status);
                    local_stats.extents_dropped++;
                }
            }
            else // neither pair in extent valid
            {
                local_stats.extents_filtered++;
            }

        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failure during processing of resource %s track %d: %s", info->reader->resource, info->track, e.what());
        LuaEndpoint::generateExceptionStatus(e.code(), e.level(), reader->outQ, &reader->active, "%s: (%s)", e.what(), info->reader->resource);
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
            mlog(INFO, "Completed processing resource %s", info->reader->resource);

            /* Indicate End of Data */
            if(reader->sendTerminator) reader->outQ->postCopy("", 0);
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
 * luaParms - :parms() --> {<key>=<value>, ...} containing parameters
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Atl03Reader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Create Parameter Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_PARM_SURFACE_TYPE,         lua_obj->parms->surface_type);
        LuaEngine::setAttrNum(L, LUA_PARM_ALONG_TRACK_SPREAD,   lua_obj->parms->along_track_spread);
        LuaEngine::setAttrInt(L, LUA_PARM_MIN_PHOTON_COUNT,     lua_obj->parms->minimum_photon_count);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_LENGTH,        lua_obj->parms->extent_length);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_STEP,          lua_obj->parms->extent_step);
        lua_pushstring(L, LUA_PARM_ATL03_CNF);
        lua_newtable(L);
        for(int i = CNF_POSSIBLE_TEP; i <= CNF_SURFACE_HIGH; i++)
        {
            lua_pushboolean(L, lua_obj->parms->atl03_cnf[i + SIGNAL_CONF_OFFSET]);
            lua_rawseti(L, -2, i);
        }
        lua_settable(L, -3);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning parameters %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Atl03Reader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ,        lua_obj->stats.segments_read);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_FILTERED,     lua_obj->stats.extents_filtered);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_SENT,         lua_obj->stats.extents_sent);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_DROPPED,      lua_obj->stats.extents_dropped);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_RETRIED,      lua_obj->stats.extents_retried);

        /* Clear if Requested */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

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
