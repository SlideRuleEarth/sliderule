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
#include "gedi.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_STAT_SEGMENTS_READ          "read"
#define LUA_STAT_FOOTPRINTS_FILTERED    "filtered"
#define LUA_STAT_FOOTPRINTS_SENT        "sent"
#define LUA_STAT_FOOTPRINTS_DROPPED     "dropped"
#define LUA_STAT_FOOTPRINTS_RETRIED     "retried"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi04aReader::fpRecType = "gedil4a.footprint";
const RecordObject::fieldDef_t Gedi04aReader::fpRecDef[] = {
    {"delta_time",      RecordObject::DOUBLE,   offsetof(footprint_t, delta_time),      1,  NULL, NATIVE_FLAGS},
    {"latitude",        RecordObject::DOUBLE,   offsetof(footprint_t, latitude),        1,  NULL, NATIVE_FLAGS},
    {"longitude",       RecordObject::DOUBLE,   offsetof(footprint_t, longitude),       1,  NULL, NATIVE_FLAGS},
    {"agbd",            RecordObject::DOUBLE,   offsetof(footprint_t, agbd),            1,  NULL, NATIVE_FLAGS},
    {"elevation",       RecordObject::DOUBLE,   offsetof(footprint_t, elevation),       1,  NULL, NATIVE_FLAGS},
    {"solar_elevation", RecordObject::DOUBLE,   offsetof(footprint_t, solar_elevation), 1,  NULL, NATIVE_FLAGS},
    {"beam",            RecordObject::UINT8,    offsetof(footprint_t, beam),            1,  NULL, NATIVE_FLAGS},
    {"flags",           RecordObject::UINT8,    offsetof(footprint_t, flags),           1,  NULL, NATIVE_FLAGS}
};

const char* Gedi04aReader::batchRecType = "gedil4a";
const RecordObject::fieldDef_t Gedi04aReader::batchRecDef[] = {
    {"footprint",       RecordObject::USER,     offsetof(gedil4a_t, footprint),         0,  fpRecType, NATIVE_FLAGS}
};

const char* Gedi04aReader::OBJECT_TYPE = "Gedi04aReader";
const char* Gedi04aReader::LuaMetaName = "Gedi04aReader";
const struct luaL_Reg Gedi04aReader::LuaMetaTable[] = {
    {"parms",       luaParms},
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * GEDIT L4A READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Gedi04aReader::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    GediParms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = (GediParms*)getLuaObject(L, 4, GediParms::OBJECT_TYPE);
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new Gedi04aReader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating Gedi04aReader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Gedi04aReader::init (void)
{
    RECDEF(fpRecType,       fpRecDef,       sizeof(footprint),                  NULL);
    RECDEF(batchRecType,    batchRecDef,    offsetof(gedil4a_t, footprint[1]),  NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Gedi04aReader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    read_timeout_ms(_parms->read_timeout * 1000)
{
    assert(_asset);
    assert(_resource);
    assert(outq_name);
    assert(_parms);

    /* Save Info */
    asset = _asset;
    resource = StringLib::duplicate(_resource);
    parms = _parms;

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.footprints_read       = 0;
    stats.footprints_filtered   = 0;
    stats.footprints_sent       = 0;
    stats.footprints_dropped    = 0;
    stats.footprints_retried    = 0;

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    LocalLib::set(readerPid, 0, sizeof(readerPid));

    /* PRocess Resource */
    try
    {
        /* Read Beam Data */
        if(parms->beam == GediParms::ALL_BEAMS)
        {
            threadCount = GediParms::NUM_BEAMS;
            for(int b = 0; b < GediParms::NUM_BEAMS; b++)
            {
                info_t* info = new info_t;
                info->reader = this;
                info->beam = GediParms::BEAM_NUMBER[b];
                readerPid[b] = new Thread(subsettingThread, info);
            }
        }
        else if(parms->beam == GediParms::BEAM0000 ||
                parms->beam == GediParms::BEAM0001 ||
                parms->beam == GediParms::BEAM0010 ||
                parms->beam == GediParms::BEAM0011 ||
                parms->beam == GediParms::BEAM0101 ||
                parms->beam == GediParms::BEAM0110 ||
                parms->beam == GediParms::BEAM1000 ||
                parms->beam == GediParms::BEAM1011)
        {
            threadCount = 1;
            info_t* info = new info_t;
            info->reader = this;
            info->beam = parms->beam;
            subsettingThread(info);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid beam specified <%d>, must be 0, 1, 2, 3, 5, 6, 8, 11, or -1 for all", parms->beam);
        }
    }
    catch(const RunTimeException& e)
    {
        /* Log Error */
        mlog(e.level(), "Failed to process resource %s: %s", resource, e.what());

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
Gedi04aReader::~Gedi04aReader (void)
{
    active = false;

    for(int b = 0; b < GediParms::NUM_BEAMS; b++)
    {
        if(readerPid[b]) delete readerPid[b];
    }

    delete outQ;

    parms->releaseLuaObject();

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Region::Region (info_t* info):
    lat_lowestmode  (info->reader->asset, info->reader->resource, SafeString("%s/lat_lowestmode", GediParms::beam2group(info->beam)).getString(), &info->reader->context),
    lon_lowestmode  (info->reader->asset, info->reader->resource, SafeString("%s/lon_lowestmode", GediParms::beam2group(info->beam)).getString(), &info->reader->context),
    inclusion_mask  (NULL),
    inclusion_ptr   (NULL)
{
    /* Join Reads */
    lat_lowestmode.join(info->reader->read_timeout_ms, true);
    lon_lowestmode.join(info->reader->read_timeout_ms, true);

    /* Initialize Region */
    first_footprint = 0;
    num_footprints = -1;

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
    if(num_footprints <= 0)
    {
        cleanup();
        throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
    }

    /* Trim Geospatial Datasets Read from HDF5 File */
    lat_lowestmode.trim(first_footprint);
    lon_lowestmode.trim(first_footprint);
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Gedi04aReader::Region::cleanup (void)
{
    if(inclusion_mask) delete [] inclusion_mask;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Gedi04aReader::Region::polyregion (info_t* info)
{
    int points_in_polygon = info->reader->parms->polygon.length();

    /* Determine Best Projection To Use */
    MathLib::proj_t projection = MathLib::PLATE_CARREE;
    if(lat_lowestmode[0] > 70.0) projection = MathLib::NORTH_POLAR;
    else if(lat_lowestmode[0] < -70.0) projection = MathLib::SOUTH_POLAR;

    /* Project Polygon */
    List<MathLib::coord_t>::Iterator poly_iterator(info->reader->parms->polygon);
    MathLib::point_t* projected_poly = new MathLib::point_t [points_in_polygon];
    for(int i = 0; i < points_in_polygon; i++)
    {
        projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
    }

    /* Find First and Last Footprints in Polygon */
    bool first_footprint_found = false;
    bool last_footprint_found = false;
    int footprint = 0;
    while(footprint < lat_lowestmode.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        MathLib::coord_t footprint_coord = {lon_lowestmode[footprint], lat_lowestmode[footprint]};
        MathLib::point_t footprint_point = MathLib::coord2point(footprint_coord, projection);

        /* Test Inclusion */
        if(MathLib::inpoly(projected_poly, points_in_polygon, footprint_point))
        {
            inclusion = true;
        }

        /* Find First Footprint */
        if(!first_footprint_found && inclusion)
        {
            /* Set First Segment */
            first_footprint_found = true;
            first_footprint = footprint;
        }
        else if(!last_footprint_found && !inclusion)
        {
            /* Set Last Segment */
            last_footprint_found = true;
            break; // full extent found!
        }

        /* Bump Footprint */
        footprint++;
    }

    /* Set Number of Segments */
    if(first_footprint_found)
    {
        num_footprints = footprint - first_footprint;
    }

    /* Delete Projected Polygon */
    delete [] projected_poly;
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void Gedi04aReader::Region::rasterregion (info_t* info)
{
    /* Allocate Inclusion Mask */
    if(lat_lowestmode.size <= 0) return;
    inclusion_mask = new bool [lat_lowestmode.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    bool first_footprint_found = false;
    long last_footprint = 0;
    int footprint = 0;
    while(footprint < lat_lowestmode.size)
    {
        /* Check Inclusion */
        bool inclusion = info->reader->parms->raster->includes(lon_lowestmode[footprint], lat_lowestmode[footprint]);
        inclusion_mask[footprint] = inclusion;

        /* If Coordinate Is In Raster */
        if(inclusion)
        {
            if(!first_footprint_found)
            {
                first_footprint_found = true;
                first_footprint = footprint;
                last_footprint = footprint;
            }
            else
            {
                last_footprint = footprint;
            }
        }

        /* Bump Footprint */
        footprint++;
    }

    /* Set Number of Footprints */
    if(first_footprint_found)
    {
        num_footprints = last_footprint - first_footprint + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_footprint];
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Constructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Atl03Data::Atl03Data (info_t* info, Region& region):
    velocity_sc         (info->reader->asset, info->reader->resource, info->track, "geolocation/velocity_sc",     &info->reader->context, H5Coro::ALL_COLS, region.first_footprint, region.num_segments),
    segment_delta_time  (info->reader->asset, info->reader->resource, info->track, "geolocation/delta_time",      &info->reader->context, 0, region.first_footprint, region.num_segments),
    segment_id          (info->reader->asset, info->reader->resource, info->track, "geolocation/segment_id",      &info->reader->context, 0, region.first_footprint, region.num_segments),
    segment_dist_x      (info->reader->asset, info->reader->resource, info->track, "geolocation/segment_dist_x",  &info->reader->context, 0, region.first_footprint, region.num_segments),
    dist_ph_along       (info->reader->asset, info->reader->resource, info->track, "heights/dist_ph_along",       &info->reader->context, 0, region.first_photon,  region.num_photons),
    h_ph                (info->reader->asset, info->reader->resource, info->track, "heights/h_ph",                &info->reader->context, 0, region.first_photon,  region.num_photons),
    signal_conf_ph      (info->reader->asset, info->reader->resource, info->track, "heights/signal_conf_ph",      &info->reader->context, info->reader->parms->surface_type, region.first_photon,  region.num_photons),
    quality_ph          (info->reader->asset, info->reader->resource, info->track, "heights/quality_ph",          &info->reader->context, 0, region.first_photon,  region.num_photons),
    lat_ph              (info->reader->asset, info->reader->resource, info->track, "heights/lat_ph",              &info->reader->context, 0, region.first_photon,  region.num_photons),
    lon_ph              (info->reader->asset, info->reader->resource, info->track, "heights/lon_ph",              &info->reader->context, 0, region.first_photon,  region.num_photons),
    delta_time          (info->reader->asset, info->reader->resource, info->track, "heights/delta_time",          &info->reader->context, 0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (info->reader->asset, info->reader->resource, info->track, "bckgrd_atlas/delta_time",     &info->reader->context),
    bckgrd_rate         (info->reader->asset, info->reader->resource, info->track, "bckgrd_atlas/bckgrd_rate",    &info->reader->context),
    anc_geo_data        (GediParms::EXPECTED_NUM_FIELDS),
    anc_ph_data         (GediParms::EXPECTED_NUM_FIELDS)
{
    GediParms::string_list_t* geo_fields = info->reader->parms->atl03_geo_fields;
    GediParms::string_list_t* photon_fields = info->reader->parms->atl03_ph_fields;

    /* Read Ancillary Geolocation Fields */
    if(geo_fields)
    {
        for(int i = 0; i < geo_fields->length(); i++)
        {
            const char* field_name = (*geo_fields)[i].getString();
            const char* group_name = "geolocation";
            if( (field_name[0] == 't' && field_name[1] == 'i' && field_name[2] == 'd') ||
                (field_name[0] == 'g' && field_name[1] == 'e' && field_name[2] == 'o') ||
                (field_name[0] == 'd' && field_name[1] == 'e' && field_name[2] == 'm') ||
                (field_name[0] == 'd' && field_name[1] == 'a' && field_name[2] == 'c') )
            {
                group_name = "geophys_corr";
            }
            SafeString dataset_name("%s/%s", group_name, field_name);
            GTDArray* array = new GTDArray(info->reader->asset, info->reader->resource, info->track, dataset_name.getString(), &info->reader->context, 0, region.first_footprint, region.num_segments);
            anc_geo_data.add(field_name, array);
        }
    }

    /* Read Ancillary Geocorrection Fields */
    if(photon_fields)
    {
        for(int i = 0; i < photon_fields->length(); i++)
        {
            const char* field_name = (*photon_fields)[i].getString();
            SafeString dataset_name("heights/%s", field_name);
            GTDArray* array = new GTDArray(info->reader->asset, info->reader->resource, info->track, dataset_name.getString(), &info->reader->context, 0, region.first_photon,  region.num_photons);
            anc_ph_data.add(field_name, array);
        }
    }

    /* Join Hardcoded Reads */
    velocity_sc.join(info->reader->read_timeout_ms, true);
    segment_delta_time.join(info->reader->read_timeout_ms, true);
    segment_id.join(info->reader->read_timeout_ms, true);
    segment_dist_x.join(info->reader->read_timeout_ms, true);
    dist_ph_along.join(info->reader->read_timeout_ms, true);
    h_ph.join(info->reader->read_timeout_ms, true);
    signal_conf_ph.join(info->reader->read_timeout_ms, true);
    quality_ph.join(info->reader->read_timeout_ms, true);
    lat_ph.join(info->reader->read_timeout_ms, true);
    lon_ph.join(info->reader->read_timeout_ms, true);
    delta_time.join(info->reader->read_timeout_ms, true);
    bckgrd_delta_time.join(info->reader->read_timeout_ms, true);
    bckgrd_rate.join(info->reader->read_timeout_ms, true);

    /* Join Ancillary Geolocation Reads */
    if(geo_fields)
    {
        GTDArray* array = NULL;
        const char* dataset_name = anc_geo_data.first(&array);
        while(dataset_name != NULL)
        {
            array->join(info->reader->read_timeout_ms, true);
            dataset_name = anc_geo_data.next(&array);
        }
    }

    /* Join Ancillary Geocorrection Reads */
    if(photon_fields)
    {
        GTDArray* array = NULL;
        const char* dataset_name = anc_ph_data.first(&array);
        while(dataset_name != NULL)
        {
            array->join(info->reader->read_timeout_ms, true);
            dataset_name = anc_ph_data.next(&array);
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Atl03Data::~Atl03Data (void)
{
}

/*----------------------------------------------------------------------------
 * Atl08Class::Constructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Atl08Class::Atl08Class (info_t* info):
    enabled             (info->reader->parms->stages[GediParms::STAGE_ATL08]),
    phoreal             (info->reader->parms->stages[GediParms::STAGE_PHOREAL]),
    gt                  {NULL, NULL},
    relief              {NULL, NULL},
    atl08_segment_id    (enabled ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/ph_segment_id",   &info->reader->context08),
    atl08_pc_indx       (enabled ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/classed_pc_indx", &info->reader->context08),
    atl08_pc_flag       (enabled ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/classed_pc_flag", &info->reader->context08),
    atl08_ph_h          (phoreal ? info->reader->asset : NULL, info->reader->resource08, info->track, "signal_photons/ph_h",            &info->reader->context08)
{
}

/*----------------------------------------------------------------------------
 * Atl08Class::Destructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::Atl08Class::~Atl08Class (void)
{
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        if(gt[t]) delete [] gt[t];
        if(relief[t]) delete [] relief[t];
    }
}

/*----------------------------------------------------------------------------
 * Atl08Class::classify
 *----------------------------------------------------------------------------*/
void Gedi04aReader::Atl08Class::classify (info_t* info, Region& region, Atl03Data& atl03)
{
    /* Do Nothing If Not Enabled */
    if(!info->reader->parms->stages[GediParms::STAGE_ATL08])
    {
        return;
    }

    /* Wait for Reads to Complete */
    atl08_segment_id.join(info->reader->read_timeout_ms, true);
    atl08_pc_indx.join(info->reader->read_timeout_ms, true);
    atl08_pc_flag.join(info->reader->read_timeout_ms, true);
    if(phoreal) atl08_ph_h.join(info->reader->read_timeout_ms, true);

    /* Rename Segment Photon Counts (to easily identify with ATL03) */
    GTArray<int32_t>* atl03_segment_ph_cnt = &region.segment_ph_cnt;

    /* Classify Photons */
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        /* Allocate ATL08 Classification Array */
        int num_photons = atl03.dist_ph_along[t].size;
        gt[t] = new uint8_t [num_photons];

        /* Allocate ATL08 Relief Array */
        if(phoreal) relief[t] = new float [num_photons];

        /* Populate ATL08 Classifications */
        int32_t atl03_photon = 0;
        int32_t atl08_photon = 0;
        for(int atl03_segment_index = 0; atl03_segment_index < atl03.segment_id[t].size; atl03_segment_index++)
        {
            int32_t atl03_segment = atl03.segment_id[t][atl03_segment_index];
            int32_t atl03_segment_count = atl03_segment_ph_cnt->gt[t][atl03_segment_index];
            for(int atl03_count = 1; atl03_count <= atl03_segment_count; atl03_count++)
            {
                /* Go To Segment */
                while( (atl08_photon < atl08_segment_id[t].size) && // atl08 photon is valid
                       (atl08_segment_id[t][atl08_photon] < atl03_segment) )
                {
                    atl08_photon++;
                }

                while( (atl08_photon < atl08_segment_id[t].size) && // atl08 photon is valid
                       (atl08_segment_id[t][atl08_photon] == atl03_segment) &&
                       (atl08_pc_indx[t][atl08_photon] < atl03_count))
                {
                    atl08_photon++;
                }

                /* Check Match */
                if( (atl08_photon < atl08_segment_id[t].size) &&
                    (atl08_segment_id[t][atl08_photon] == atl03_segment) &&
                    (atl08_pc_indx[t][atl08_photon] == atl03_count) )
                {
                    /* Assign Classification */
                    gt[t][atl03_photon] = (uint8_t)atl08_pc_flag[t][atl08_photon];

                    /* Populate ATL08 Relief */
                    if(phoreal) relief[t][atl03_photon] = atl08_ph_h[t][atl08_photon];

                    /* Go To Next ATL08 Photon */
                    atl08_photon++;
                }
                else
                {
                    /* Unclassified */
                    gt[t][atl03_photon] = GediParms::ATL08_UNCLASSIFIED;
                }

                /* Go To Next ATL03 Photon */
                atl03_photon++;
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl08Class::operator[]
 *----------------------------------------------------------------------------*/
uint8_t* Gedi04aReader::Atl08Class::operator[] (int t)
{
    return gt[t];
}

/*----------------------------------------------------------------------------
 * YapcScore::Constructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::YapcScore::YapcScore (info_t* info, Region& region, Atl03Data& atl03):
    gt {NULL, NULL}
{
    /* Do Nothing If Not Enabled */
    if(!info->reader->parms->stages[GediParms::STAGE_YAPC])
    {
        return;
    }

    /* Run YAPC */
    if(info->reader->parms->yapc.version == 3)
    {
        yapcV3(info, region, atl03);
    }
    else if(info->reader->parms->yapc.version == 2 || info->reader->parms->yapc.version == 1)
    {
        yapcV2(info, region, atl03);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid YAPC version specified: %d", info->reader->parms->yapc.version);
    }
}

/*----------------------------------------------------------------------------
 * YapcScore::Destructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::YapcScore::~YapcScore (void)
{
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        if(gt[t]) delete [] gt[t];
    }
}

/*----------------------------------------------------------------------------
 * yapcV2
 *----------------------------------------------------------------------------*/
void Gedi04aReader::YapcScore::yapcV2 (info_t* info, Region& region, Atl03Data& atl03)
{
    /* YAPC Hard-Coded Parameters */
    const double MAXIMUM_HSPREAD = 15000.0; // meters
    const double HSPREAD_BINSIZE = 1.0; // meters
    const int MAX_KNN = 25;
    double nearest_neighbors[MAX_KNN];

    /* Shortcut to Settings */
    GediParms::yapc_t* settings = &info->reader->parms->yapc;

    /* Score Photons
     *
     *   CANNOT THROW BELOW THIS POINT
     */
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        /* Allocate ATL08 Classification Array */
        int32_t num_photons = atl03.dist_ph_along[t].size;
        gt[t] = new uint8_t [num_photons];
        LocalLib::set(gt[t], 0, num_photons);

        /* Initialize Indices */
        int32_t ph_b0 = 0; // buffer start
        int32_t ph_b1 = 0; // buffer end
        int32_t ph_c0 = 0; // center start
        int32_t ph_c1 = 0; // center end

        /* Loop Through Each ATL03 Segment */
        int32_t num_segments = atl03.segment_id[t].size;
        for(int segment_index = 0; segment_index < num_segments; segment_index++)
        {
            /* Determine Indices */
            ph_b0 += segment_index > 1 ? region.segment_ph_cnt[t][segment_index - 2] : 0; // Center - 2
            ph_c0 += segment_index > 0 ? region.segment_ph_cnt[t][segment_index - 1] : 0; // Center - 1
            ph_c1 += region.segment_ph_cnt[t][segment_index]; // Center
            ph_b1 += segment_index < (num_segments - 1) ? region.segment_ph_cnt[t][segment_index + 1] : 0; // Center + 1

            /* Calculate N and KNN */
            int32_t N = region.segment_ph_cnt[t][segment_index];
            int knn = (settings->knn != 0) ? settings->knn : MAX(1, (sqrt((double)N) + 0.5) / 2);
            knn = MIN(knn, MAX_KNN); // truncate if too large

            /* Check Valid Extent (note check against knn)*/
            if((N <= knn) || (N < info->reader->parms->minimum_photon_count)) continue;

            /* Calculate Distance and Height Spread */
            double min_h = atl03.h_ph[t][0];
            double max_h = min_h;
            double min_x = atl03.dist_ph_along[t][0];
            double max_x = min_x;
            for(int n = 1; n < N; n++)
            {
                double h = atl03.h_ph[t][n];
                double x = atl03.dist_ph_along[t][n];
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
                unsigned int bin = (unsigned int)((atl03.h_ph[t][n] - min_h) / HSPREAD_BINSIZE);
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
                    double delta_x = abs(atl03.dist_ph_along[t][x] - atl03.dist_ph_along[t][y]);
                    if(delta_x > half_win_x) continue;

                    /*  Calculate Weighted Distance */
                    double delta_h = abs(atl03.h_ph[t][x] - atl03.h_ph[t][y]);
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
void Gedi04aReader::YapcScore::yapcV3 (info_t* info, Region& region, Atl03Data& atl03)
{
    /* YAPC Parameters */
    GediParms::yapc_t* settings = &info->reader->parms->yapc;
    const double hWX = settings->win_x / 2; // meters
    const double hWZ = settings->win_h / 2; // meters

    /* Score Photons
     *
     *   CANNOT THROW BELOW THIS POINT
     */
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        int32_t num_segments = atl03.segment_id[t].size;
        int32_t num_photons = atl03.dist_ph_along[t].size;

        /* Allocate Photon Arrays */
        gt[t] = new uint8_t [num_photons]; // class member freed in deconstructor
        double* ph_dist = new double[num_photons]; // local array freed below

        /* Populate Distance Array */
        int32_t ph_index = 0;
        for(int segment_index = 0; segment_index < num_segments; segment_index++)
        {
            for(int32_t ph_in_seg_index = 0; ph_in_seg_index < region.segment_ph_cnt[t][segment_index]; ph_in_seg_index++)
            {
                ph_dist[ph_index] = atl03.segment_dist_x[t][segment_index] + atl03.dist_ph_along[t][ph_index];
                ph_index++;
            }
        }

        /* Traverse Each Segment */
        ph_index = 0;
        for(int segment_index = 0; segment_index < num_segments; segment_index++)
        {
            /* Initialize Segment Parameters */
            int32_t N = region.segment_ph_cnt[t][segment_index];
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
                        double proximity = abs(atl03.h_ph[t][ph_index] - atl03.h_ph[t][neighbor_index]);
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
                        double proximity = abs(atl03.h_ph[t][ph_index] - atl03.h_ph[t][neighbor_index]);
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
 * YapcScore::operator[]
 *----------------------------------------------------------------------------*/
uint8_t* Gedi04aReader::YapcScore::operator[] (int t)
{
    return gt[t];
}

/*----------------------------------------------------------------------------
 * TrackState::Constructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::TrackState::TrackState (Atl03Data& atl03)
{
    gt[GediParms::RPT_L].ph_in              = 0;
    gt[GediParms::RPT_L].seg_in             = 0;
    gt[GediParms::RPT_L].seg_ph             = 0;
    gt[GediParms::RPT_L].start_segment      = 0;
    gt[GediParms::RPT_L].start_distance     = atl03.segment_dist_x[GediParms::RPT_L][0];
    gt[GediParms::RPT_L].seg_distance       = 0.0;
    gt[GediParms::RPT_L].start_seg_portion  = 0.0;
    gt[GediParms::RPT_L].track_complete     = false;
    gt[GediParms::RPT_L].bckgrd_in          = 0;
    gt[GediParms::RPT_L].photon_indices     = NULL;
    gt[GediParms::RPT_L].extent_segment     = 0;
    gt[GediParms::RPT_L].extent_valid       = true;

    gt[GediParms::RPT_R].ph_in             = 0;
    gt[GediParms::RPT_R].seg_in            = 0;
    gt[GediParms::RPT_R].seg_ph            = 0;
    gt[GediParms::RPT_R].start_segment     = 0;
    gt[GediParms::RPT_R].start_distance    = atl03.segment_dist_x[GediParms::RPT_R][0];
    gt[GediParms::RPT_R].seg_distance      = 0.0;
    gt[GediParms::RPT_R].start_seg_portion = 0.0;
    gt[GediParms::RPT_R].track_complete    = false;
    gt[GediParms::RPT_R].bckgrd_in         = 0;
    gt[GediParms::RPT_R].photon_indices    = NULL;
    gt[GediParms::RPT_R].extent_segment    = 0;
    gt[GediParms::RPT_R].extent_valid      = true;
}

/*----------------------------------------------------------------------------
 * TrackState::Destructor
 *----------------------------------------------------------------------------*/
Gedi04aReader::TrackState::~TrackState (void)
{
    if(gt[GediParms::RPT_L].photon_indices) delete gt[GediParms::RPT_L].photon_indices;
    if(gt[GediParms::RPT_R].photon_indices) delete gt[GediParms::RPT_R].photon_indices;
}

/*----------------------------------------------------------------------------
 * TrackState::operator[]
 *----------------------------------------------------------------------------*/
Gedi04aReader::TrackState::track_state_t& Gedi04aReader::TrackState::operator[](int t)
{
    return gt[t];
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi04aReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Gedi04aReader* reader = info->reader;
    GediParms* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};
    uint32_t extent_counter = 0;

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
        Atl03Data atl03(info, region);

        /* Perform YAPC Scoring (if requested) */
        YapcScore yapc(info, region, atl03);

        /* Perform ATL08 Classification (if requested) */
        atl08.classify(info, region, atl03);

        /* Initialize Track State */
        TrackState state(atl03);

        /* Increment Read Statistics */
        local_stats.footprints_read = (region.segment_ph_cnt[GediParms::RPT_L].size + region.segment_ph_cnt[GediParms::RPT_R].size);

        /* Calculate Length of Extent in Meters (used for distance) */
        state.extent_length = parms->extent_length;
        if(parms->dist_in_seg) state.extent_length *= ATL03_SEGMENT_LENGTH;

        /* Traverse All Photons In Dataset */
        while( reader->active && (!state[GediParms::RPT_L].track_complete || !state[GediParms::RPT_R].track_complete) )
        {
            /* Select Photons for Extent from each Track */
            for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
            {
                /* Skip Completed Tracks */
                if(state[t].track_complete)
                {
                    state[t].extent_valid = false;
                    continue;
                }

                /* Setup Variables for Extent */
                int32_t current_photon = state[t].ph_in;
                int32_t current_segment = state[t].seg_in;
                int32_t current_count = state[t].seg_ph; // number of photons in current segment already accounted for
                bool extent_complete = false;
                bool step_complete = false;

                /* Set Extent State */
                state[t].start_seg_portion = atl03.dist_ph_along[t][current_photon] / ATL03_SEGMENT_LENGTH;
                state[t].extent_segment = state[t].seg_in;
                state[t].extent_valid = true;
                state[t].extent_photons.clear();

                /* Ancillary Photon Fields */
                if(parms->atl03_ph_fields)
                {
                    if(state[t].photon_indices) state[t].photon_indices->clear();
                    else                        state[t].photon_indices = new List<int32_t>;
                }

                /* Traverse Photons Until Desired Along Track Distance Reached */
                while(!extent_complete || !step_complete)
                {
                    /* Go to Photon's Segment */
                    current_count++;
                    while((current_segment < region.segment_ph_cnt[t].size) &&
                          (current_count > region.segment_ph_cnt[t][current_segment]))
                    {
                        current_count = 1; // reset photons in segment
                        current_segment++; // go to next segment
                    }

                    /* Check Current Segment */
                    if(current_segment >= atl03.segment_dist_x[t].size)
                    {
                        mlog(ERROR, "Photons with no segments are detected is %s/%d     %d %ld %ld!", info->reader->resource, info->track, current_segment, atl03.segment_dist_x[t].size, region.num_segments[t]);
                        state[t].track_complete = true;
                        break;
                    }

                    /* Update Along Track Distance and Progress */
                    double delta_distance = atl03.segment_dist_x[t][current_segment] - state[t].start_distance;
                    double along_track_distance = delta_distance + atl03.dist_ph_along[t][current_photon];
                    int32_t along_track_segments = current_segment - state[t].extent_segment;

                    /* Set Next Extent's First Photon */
                    if((!step_complete) &&
                       ((!parms->dist_in_seg && along_track_distance >= parms->extent_step) ||
                        (parms->dist_in_seg && along_track_segments >= (int32_t)parms->extent_step)))
                    {
                        state[t].ph_in = current_photon;
                        state[t].seg_in = current_segment;
                        state[t].seg_ph = current_count - 1;
                        step_complete = true;
                    }

                    /* Check if Photon within Extent's Length */
                    if((!parms->dist_in_seg && along_track_distance < parms->extent_length) ||
                       (parms->dist_in_seg && along_track_segments < parms->extent_length))
                    {
                        do
                        {
                            /* Check and Set Signal Confidence Level */
                            int8_t atl03_cnf = atl03.signal_conf_ph[t][current_photon];
                            if(atl03_cnf < GediParms::CNF_POSSIBLE_TEP || atl03_cnf > GediParms::CNF_SURFACE_HIGH)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 signal confidence: %d", atl03_cnf);
                            }
                            else if(!parms->atl03_cnf[atl03_cnf + GediParms::SIGNAL_CONF_OFFSET])
                            {
                                break;
                            }

                            /* Check and Set ATL03 Photon Quality Level */
                            int8_t quality_ph = atl03.quality_ph[t][current_photon];
                            if(quality_ph < GediParms::QUALITY_NOMINAL || quality_ph > GediParms::QUALITY_POSSIBLE_TEP)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 photon quality: %d", quality_ph);
                            }
                            else if(!parms->quality_ph[quality_ph])
                            {
                                break;
                            }

                            /* Check and Set ATL08 Classification */
                            GediParms::atl08_classification_t atl08_class = GediParms::ATL08_UNCLASSIFIED;
                            if(atl08[t])
                            {
                                atl08_class = (GediParms::atl08_classification_t)atl08[t][current_photon];
                                if(atl08_class < 0 || atl08_class >= GediParms::NUM_ATL08_CLASSES)
                                {
                                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl08 classification: %d", atl08_class);
                                }
                                else if(!parms->atl08_class[atl08_class])
                                {
                                    break;
                                }
                            }

                            /* Check and Set Relief */
                            float relief = 0.0;
                            if(atl08.phoreal)
                            {
                                if(!parms->phoreal.use_abs_h)
                                {
                                    relief = atl08.relief[t][current_photon];
                                }
                                else
                                {
                                    relief = atl03.h_ph[t][current_photon];
                                }
                            }

                            /* Check and Set YAPC Score */
                            uint8_t yapc_score = 0;
                            if(yapc[t])
                            {
                                yapc_score = yapc[t][current_photon];
                                if(yapc_score < parms->yapc.score)
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
                                .delta_time = atl03.delta_time[t][current_photon],
                                .latitude = atl03.lat_ph[t][current_photon],
                                .longitude = atl03.lon_ph[t][current_photon],
                                .distance = along_track_distance - (state.extent_length / 2.0),
                                .height = atl03.h_ph[t][current_photon],
                                .relief = relief,
                                .atl08_class = (uint8_t)atl08_class,
                                .atl03_cnf = (int8_t)atl03_cnf,
                                .quality_ph = (int8_t)quality_ph,
                                .yapc_score = yapc_score
                            };
                            state[t].extent_photons.add(ph);

                            /* Index Photon for Ancillary Fields */
                            if(state[t].photon_indices)
                            {
                                state[t].photon_indices->add(current_photon);
                            }
                        } while(false);
                    }
                    else
                    {
                        extent_complete = true;
                    }

                    /* Go to Next Photon */
                    current_photon++;

                    /* Check Current Photon */
                    if(current_photon >= atl03.dist_ph_along[t].size)
                    {
                        state[t].track_complete = true;
                        break;
                    }
                }

                /* Save Off Segment Distance to Include in Extent Record */
                state[t].seg_distance = state[t].start_distance + (state.extent_length / 2.0);

                /* Add Step to Start Distance */
                if(!parms->dist_in_seg)
                {
                    state[t].start_distance += parms->extent_step; // step start distance

                    /* Apply Segment Distance Correction and Update Start Segment */
                    while( ((state[t].start_segment + 1) < atl03.segment_dist_x[t].size) &&
                            (state[t].start_distance >= atl03.segment_dist_x[t][state[t].start_segment + 1]) )
                    {
                        state[t].start_distance += atl03.segment_dist_x[t][state[t].start_segment + 1] - atl03.segment_dist_x[t][state[t].start_segment];
                        state[t].start_distance -= ATL03_SEGMENT_LENGTH;
                        state[t].start_segment++;
                    }
                }
                else // distance in segments
                {
                    int32_t next_segment = state[t].extent_segment + (int32_t)parms->extent_step;
                    if(next_segment < atl03.segment_dist_x[t].size)
                    {
                        state[t].start_distance = atl03.segment_dist_x[t][next_segment]; // set start distance to next extent's segment distance
                    }
                }

                /* Check Photon Count */
                if(state[t].extent_photons.length() < parms->minimum_photon_count)
                {
                    state[t].extent_valid = false;
                }

                /* Check Along Track Spread */
                if(state[t].extent_photons.length() > 1)
                {
                    int32_t last = state[t].extent_photons.length() - 1;
                    double along_track_spread = state[t].extent_photons[last].distance - state[t].extent_photons[0].distance;
                    if(along_track_spread < parms->along_track_spread)
                    {
                        state[t].extent_valid = false;
                    }
                }
            }

            /* Create Extent Record */
            if(state[GediParms::RPT_L].extent_valid || state[GediParms::RPT_R].extent_valid || parms->pass_invalid)
            {
                /* Generate Extent ID */
                uint64_t extent_id = ((uint64_t)reader->start_rgt << 52) |
                                     ((uint64_t)reader->start_cycle << 36) |
                                     ((uint64_t)reader->start_region << 32) |
                                     ((uint64_t)info->track << 30) |
                                     (((uint64_t)extent_counter & 0xFFFFFFF) << 2) |
                                     GediParms::EXTENT_ID_PHOTONS;

                /* Build and Send Extent Record */
                if(!reader->flatten)
                {
                    reader->sendExtentRecord(extent_id, info->track, state, atl03, &local_stats);
                }
                else
                {
                    reader->sendFlatRecord (extent_id, info->track, state, atl03, &local_stats);
                }


                /* Send Ancillary Records */
                reader->sendAncillaryGeoRecords(extent_id, parms->atl03_geo_fields, &atl03.anc_geo_data, state, &local_stats);
                reader->sendAncillaryPhRecords(extent_id, parms->atl03_ph_fields, &atl03.anc_ph_data, state, &local_stats);
            }
            else // neither pair in extent valid
            {
                local_stats.footprints_filtered++;
            }

            /* Bump Extent Counter */
            extent_counter++;
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
        reader->stats.footprints_read += local_stats.footprints_read;
        reader->stats.footprints_filtered += local_stats.footprints_filtered;
        reader->stats.footprints_sent += local_stats.footprints_sent;
        reader->stats.footprints_dropped += local_stats.footprints_dropped;
        reader->stats.footprints_retried += local_stats.footprints_retried;

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
 * calculateBackground
 *----------------------------------------------------------------------------*/
double Gedi04aReader::calculateBackground (int t, TrackState& state, Atl03Data& atl03)
{
    double background_rate = atl03.bckgrd_rate[t][atl03.bckgrd_rate[t].size - 1];
    while(state[t].bckgrd_in < atl03.bckgrd_rate[t].size)
    {
        double curr_bckgrd_time = atl03.bckgrd_delta_time[t][state[t].bckgrd_in];
        double segment_time = atl03.segment_delta_time[t][state[t].extent_segment];
        if(curr_bckgrd_time >= segment_time)
        {
            /* Interpolate Background Rate */
            if(state[t].bckgrd_in > 0)
            {
                double prev_bckgrd_time = atl03.bckgrd_delta_time[t][state[t].bckgrd_in - 1];
                double prev_bckgrd_rate = atl03.bckgrd_rate[t][state[t].bckgrd_in - 1];
                double curr_bckgrd_rate = atl03.bckgrd_rate[t][state[t].bckgrd_in];

                double bckgrd_run = curr_bckgrd_time - prev_bckgrd_time;
                double bckgrd_rise = curr_bckgrd_rate - prev_bckgrd_rate;
                double segment_to_bckgrd_delta = segment_time - prev_bckgrd_time;

                background_rate = ((bckgrd_rise / bckgrd_run) * segment_to_bckgrd_delta) + prev_bckgrd_rate;
            }
            else
            {
                /* Use First Background Rate (no interpolation) */
                background_rate = atl03.bckgrd_rate[t][0];
            }
            break;
        }
        else
        {
            /* Go To Next Background Rate */
            state[t].bckgrd_in++;
        }
    }
    return background_rate;
}

/*----------------------------------------------------------------------------
 * calculateSegmentId
 *----------------------------------------------------------------------------*/
uint32_t Gedi04aReader::calculateSegmentId (int t, TrackState& state, Atl03Data& atl03)
{
    /* Calculate Segment ID (attempt to arrive at closest ATL06 segment ID represented by extent) */
    double atl06_segment_id = (double)atl03.segment_id[t][state[t].extent_segment]; // start with first segment in extent
    if(!parms->dist_in_seg)
    {
        atl06_segment_id += state[t].start_seg_portion; // add portion of first segment that first photon is included
        atl06_segment_id += (int)((parms->extent_length / ATL03_SEGMENT_LENGTH) / 2.0); // add half the length of the extent
    }
    else // dist_in_seg is true
    {
        atl06_segment_id += (int)(parms->extent_length / 2.0);
    }

    /* Round Up */
    return (uint32_t)(atl06_segment_id + 0.5);
}

/*----------------------------------------------------------------------------
 * sendExtentRecord
 *----------------------------------------------------------------------------*/
bool Gedi04aReader::sendExtentRecord (uint64_t extent_id, uint8_t track, TrackState& state, Atl03Data& atl03, stats_t* local_stats)
{
    /* Calculate Extent Record Size */
    int num_photons = state[GediParms::RPT_L].extent_photons.length() + state[GediParms::RPT_R].extent_photons.length();
    int extent_bytes = offsetof(extent_t, photons) + (sizeof(photon_t) * num_photons);

    /* Allocate and Initialize Extent Record */
    RecordObject record(exRecType, extent_bytes);
    extent_t* extent = (extent_t*)record.getRecordData();
    extent->extent_id = extent_id;
    extent->reference_pair_track = track;
    extent->spacecraft_orientation = (*sc_orient)[0];
    extent->reference_ground_track_start = start_rgt;
    extent->cycle_start = start_cycle;

    /* Populate Extent */
    uint32_t ph_out = 0;
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        /* Calculate Spacecraft Velocity */
        int32_t sc_v_offset = state[t].extent_segment * 3;
        double sc_v1 = atl03.velocity_sc[t][sc_v_offset + 0];
        double sc_v2 = atl03.velocity_sc[t][sc_v_offset + 1];
        double sc_v3 = atl03.velocity_sc[t][sc_v_offset + 2];
        double spacecraft_velocity = sqrt((sc_v1*sc_v1) + (sc_v2*sc_v2) + (sc_v3*sc_v3));

        /* Calculate Segment ID (attempt to arrive at closest ATL06 segment ID represented by extent) */
        double atl06_segment_id = (double)atl03.segment_id[t][state[t].extent_segment]; // start with first segment in extent
        if(!parms->dist_in_seg)
        {
            atl06_segment_id += state[t].start_seg_portion; // add portion of first segment that first photon is included
            atl06_segment_id += (int)((parms->extent_length / ATL03_SEGMENT_LENGTH) / 2.0); // add half the length of the extent
        }
        else // dist_in_seg is true
        {
            atl06_segment_id += (int)(parms->extent_length / 2.0);
        }

        /* Populate Attributes */
        extent->valid[t]                = state[t].extent_valid;
        extent->segment_id[t]           = calculateSegmentId(t, state, atl03);
        extent->segment_distance[t]     = state[t].seg_distance;
        extent->extent_length[t]        = state.extent_length;
        extent->spacecraft_velocity[t]  = spacecraft_velocity;
        extent->background_rate[t]      = calculateBackground(t, state, atl03);
        extent->photon_count[t]         = state[t].extent_photons.length();

        /* Populate Photons */
        if(num_photons > 0)
        {
            for(int32_t p = 0; p < state[t].extent_photons.length(); p++)
            {
                extent->photons[ph_out++] = state[t].extent_photons[p];
            }
        }
    }

    /* Set Photon Pointer Fields */
    extent->photon_offset[GediParms::RPT_L] = offsetof(extent_t, photons); // pointers are set to offset from start of record data
    extent->photon_offset[GediParms::RPT_R] = offsetof(extent_t, photons) + (sizeof(photon_t) * extent->photon_count[GediParms::RPT_L]);

    /* Post Segment Record */
    return postRecord(&record, local_stats);
}

/*----------------------------------------------------------------------------
 * sendFlatRecord
 *----------------------------------------------------------------------------*/
bool Gedi04aReader::sendFlatRecord (uint64_t extent_id, uint8_t track, TrackState& state, Atl03Data& atl03, stats_t* local_stats)
{
    /* Calculate Extent Record Size */
    int num_photons = state[GediParms::RPT_L].extent_photons.length() + state[GediParms::RPT_R].extent_photons.length();
    int extent_bytes = sizeof(flat_photon_t) * num_photons;

    /* Allocate and Initialize Extent Record */
    RecordObject record(exFlatRecType, extent_bytes);
    flat_photon_t* extent = (flat_photon_t*)record.getRecordData();

    /* Populate Extent */
    uint32_t ph_out = 0;
    for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
    {
        uint8_t spot = GediParms::getSpotNumber((GediParms::sc_orient_t)(*sc_orient)[0], (GediParms::track_t)track, t);
        uint32_t segment_id = calculateSegmentId(t, state, atl03);

        /* Populate Photons */
        if(num_photons > 0)
        {
            for(int32_t p = 0; p < state[t].extent_photons.length(); p++)
            {
                flat_photon_t* photon = &extent[ph_out++];
                photon->extent_id = extent_id;
                photon->track = track;
                photon->spot = spot;
                photon->pair = t;
                photon->rgt = start_rgt;
                photon->cycle = start_cycle;
                photon->segment_id = segment_id;
                photon->photon = state[t].extent_photons[p];
            }
        }
    }

    /* Post Segment Record */
    return postRecord(&record, local_stats);
}

/*----------------------------------------------------------------------------
 * sendAncillaryGeoRecords
 *----------------------------------------------------------------------------*/
bool Gedi04aReader::sendAncillaryGeoRecords (uint64_t extent_id, GediParms::string_list_t* field_list, MgDictionary<GTDArray*>* field_dict, TrackState& state, stats_t* local_stats)
{
    if(field_list)
    {
        for(int i = 0; i < field_list->length(); i++)
        {
            /* Get Data Array */
            GTDArray* array = field_dict->get((*field_list)[i].getString());

            /* Create Ancillary Record */
            int record_size = offsetof(anc_extent_t, data) + array->gt[GediParms::RPT_L].elementSize() + array->gt[GediParms::RPT_R].elementSize();
            RecordObject record(exAncRecType, record_size);
            anc_extent_t* data = (anc_extent_t*)record.getRecordData();

            /* Populate Ancillary Record */
            data->extent_id = extent_id;
            data->field_index = i;
            data->data_type = array->gt[GediParms::RPT_L].elementType();

            /* Populate Ancillary Data */
            uint32_t num_elements[GediParms::NUM_PAIR_TRACKS] = {1, 1};
            int32_t start_element[GediParms::NUM_PAIR_TRACKS] = {state[GediParms::RPT_L].extent_segment, state[GediParms::RPT_R].extent_segment};
            array->serialize(&data->data[0], start_element, num_elements);

            /* Post Ancillary Record */
            postRecord(&record, local_stats);
        }
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * sendAncillaryPhRecords
 *----------------------------------------------------------------------------*/
bool Gedi04aReader::sendAncillaryPhRecords (uint64_t extent_id, GediParms::string_list_t* field_list, MgDictionary<GTDArray*>* field_dict, TrackState& state, stats_t* local_stats)
{
    if(field_list)
    {
        for(int i = 0; i < field_list->length(); i++)
        {
            /* Get Data Array */
            GTDArray* array = field_dict->get((*field_list)[i].getString());

            /* Create Ancillary Record */
            int record_size =   offsetof(anc_photon_t, data) +
                                (array->gt[GediParms::RPT_L].elementSize() * state[GediParms::RPT_L].photon_indices->length()) +
                                (array->gt[GediParms::RPT_R].elementSize() * state[GediParms::RPT_R].photon_indices->length());
            RecordObject record(phAncRecType, record_size);
            anc_photon_t* data = (anc_photon_t*)record.getRecordData();

            /* Populate Ancillary Record */
            data->extent_id = extent_id;
            data->field_index = i;
            data->data_type = array->gt[GediParms::RPT_L].elementType();
            data->num_elements[GediParms::RPT_L] = state[GediParms::RPT_L].photon_indices->length();
            data->num_elements[GediParms::RPT_R] = state[GediParms::RPT_R].photon_indices->length();

            /* Populate Ancillary Data */
            uint64_t bytes_written = 0;
            for(int t = 0; t < GediParms::NUM_PAIR_TRACKS; t++)
            {
                for(int p = 0; p < state[t].photon_indices->length(); p++)
                {
                    bytes_written += array->gt[t].serialize(&data->data[bytes_written], state[t].photon_indices->get(p), 1);
                }
            }

            /* Post Ancillary Record */
            postRecord(&record, local_stats);
        }
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * postRecord
 *----------------------------------------------------------------------------*/
bool Gedi04aReader::postRecord (RecordObject* record, stats_t* local_stats)
{
    uint8_t* rec_buf = NULL;
    int rec_bytes = record->serialize(&rec_buf, RecordObject::REFERENCE);
    int post_status = MsgQ::STATE_TIMEOUT;
    while(active && (post_status = outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT)
    {
        local_stats->footprints_retried++;
    }

    /* Update Statistics */
    if(post_status > 0)
    {
        local_stats->footprints_sent++;
        return true;
    }
    else
    {
        mlog(ERROR, "Atl03 reader failed to post %s to stream %s: %d", record->getRecordType(), outQ->getName(), post_status);
        local_stats->footprints_dropped++;
        return false;
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
void Gedi04aReader::parseResource (const char* resource, int32_t& rgt, int32_t& cycle, int32_t& region)
{
    if(StringLib::size(resource) < 29)
    {
        rgt = 0;
        cycle = 0;
        region = 0;
        return; // early exit on error
    }

    long val;
    char rgt_str[5];
    rgt_str[0] = resource[21];
    rgt_str[1] = resource[22];
    rgt_str[2] = resource[23];
    rgt_str[3] = resource[24];
    rgt_str[4] = '\0';
    if(StringLib::str2long(rgt_str, &val, 10))
    {
        rgt = val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse RGT from resource %s: %s", resource, rgt_str);
    }

    char cycle_str[3];
    cycle_str[0] = resource[25];
    cycle_str[1] = resource[26];
    cycle_str[2] = '\0';
    if(StringLib::str2long(cycle_str, &val, 10))
    {
        cycle = val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Cycle from resource %s: %s", resource, cycle_str);
    }

    char region_str[3];
    region_str[0] = resource[27];
    region_str[1] = resource[28];
    region_str[2] = '\0';
    if(StringLib::str2long(region_str, &val, 10))
    {
        region = val;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse Region from resource %s: %s", resource, region_str);
    }
}

/*----------------------------------------------------------------------------
 * luaParms - :parms() --> {<key>=<value>, ...} containing parameters
 *----------------------------------------------------------------------------*/
int Gedi04aReader::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Gedi04aReader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Gedi04aReader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Create Parameter Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, GediParms::SURFACE_TYPE,         lua_obj->parms->surface_type);
        LuaEngine::setAttrNum(L, GediParms::ALONG_TRACK_SPREAD,   lua_obj->parms->along_track_spread);
        LuaEngine::setAttrInt(L, GediParms::MIN_PHOTON_COUNT,     lua_obj->parms->minimum_photon_count);
        LuaEngine::setAttrNum(L, GediParms::EXTENT_LENGTH,        lua_obj->parms->extent_length);
        LuaEngine::setAttrNum(L, GediParms::EXTENT_STEP,          lua_obj->parms->extent_step);
        lua_pushstring(L, GediParms::ATL03_CNF);
        lua_newtable(L);
        for(int i = GediParms::CNF_POSSIBLE_TEP; i <= GediParms::CNF_SURFACE_HIGH; i++)
        {
            lua_pushboolean(L, lua_obj->parms->atl03_cnf[i + GediParms::SIGNAL_CONF_OFFSET]);
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
int Gedi04aReader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Gedi04aReader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Gedi04aReader*)getLuaSelf(L, 1);
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
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ,        lua_obj->stats.footprints_read);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_FILTERED,     lua_obj->stats.footprints_filtered);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_SENT,         lua_obj->stats.footprints_sent);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_DROPPED,      lua_obj->stats.footprints_dropped);
        LuaEngine::setAttrInt(L, LUA_STAT_FOOTPRINTS_RETRIED,      lua_obj->stats.footprints_retried);

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
