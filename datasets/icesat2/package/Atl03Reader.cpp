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
#include "ContainerRecord.h"
#include "LuaObject.h"
#include "AncillaryFields.h"
#include "Atl03Reader.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Reader::phRecType = "atl03rec.photons";
const RecordObject::fieldDef_t Atl03Reader::phRecDef[] = {
    {"time",            RecordObject::TIME8,    offsetof(photon_t, time_ns),        1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"latitude",        RecordObject::DOUBLE,   offsetof(photon_t, latitude),       1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",       RecordObject::DOUBLE,   offsetof(photon_t, longitude),      1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"x_atc",           RecordObject::FLOAT,    offsetof(photon_t, x_atc),          1,  NULL, NATIVE_FLAGS},
    {"y_atc",           RecordObject::FLOAT,    offsetof(photon_t, y_atc),          1,  NULL, NATIVE_FLAGS},
    {"height",          RecordObject::FLOAT,    offsetof(photon_t, height),         1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"relief",          RecordObject::FLOAT,    offsetof(photon_t, relief),         1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"landcover",       RecordObject::UINT8,    offsetof(photon_t, landcover),      1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"snowcover",       RecordObject::UINT8,    offsetof(photon_t, snowcover),      1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"atl08_class",     RecordObject::UINT8,    offsetof(photon_t, atl08_class),    1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"atl03_cnf",       RecordObject::INT8,     offsetof(photon_t, atl03_cnf),      1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"quality_ph",      RecordObject::INT8,     offsetof(photon_t, quality_ph),     1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"yapc_score",      RecordObject::UINT8,    offsetof(photon_t, yapc_score),     1,  NULL, NATIVE_FLAGS | RecordObject::AUX}
};

const char* Atl03Reader::exRecType = "atl03rec";
const RecordObject::fieldDef_t Atl03Reader::exRecDef[] = {
    {"region",          RecordObject::UINT8,    offsetof(extent_t, region),                 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"track",           RecordObject::UINT8,    offsetof(extent_t, track),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"pair",            RecordObject::UINT8,    offsetof(extent_t, pair),                   1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"sc_orient",       RecordObject::UINT8,    offsetof(extent_t, spacecraft_orientation), 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"rgt",             RecordObject::UINT16,   offsetof(extent_t, reference_ground_track), 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"cycle",           RecordObject::UINT8,    offsetof(extent_t, cycle),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"segment_id",      RecordObject::UINT32,   offsetof(extent_t, segment_id),             1,  NULL, NATIVE_FLAGS},
    {"segment_dist",    RecordObject::DOUBLE,   offsetof(extent_t, segment_distance),       1,  NULL, NATIVE_FLAGS}, // distance from equator
    {"background_rate", RecordObject::DOUBLE,   offsetof(extent_t, background_rate),        1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"solar_elevation", RecordObject::FLOAT,    offsetof(extent_t, solar_elevation),        1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"extent_id",       RecordObject::UINT64,   offsetof(extent_t, extent_id),              1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"photons",         RecordObject::USER,     offsetof(extent_t, photons),                0,  phRecType, NATIVE_FLAGS | RecordObject::BATCH} // variable length
};

const double Atl03Reader::ATL03_SEGMENT_LENGTH = 20.0; // meters

const char* Atl03Reader::OBJECT_TYPE = "Atl03Reader";
const char* Atl03Reader::LUA_META_NAME = "Atl03Reader";
const struct luaL_Reg Atl03Reader::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaCreate (lua_State* L)
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

        /* Return Reader Object */
        return createLuaObject(L, new Atl03Reader(L, outq_name, _parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating Atl03Reader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Reader::init (void)
{
    RECDEF(phRecType,       phRecDef,       sizeof(photon_t),       NULL);
    RECDEF(exRecType,       exRecDef,       sizeof(extent_t),       NULL /* "extent_id" */);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl03Reader (lua_State* L, const char* outq_name, Icesat2Fields* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    read_timeout_ms(_parms->readTimeout.value * 1000),
    parms(_parms),
    context(NULL),
    context08(NULL)
{
    assert(outq_name);
    assert(_parms);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Set Signal Confidence Index */
    if(parms->surfaceType == Icesat2Fields::SRT_DYNAMIC)
    {
        signalConfColIndex = H5Coro::ALL_COLS;
    }
    else
    {
        signalConfColIndex = static_cast<int>(parms->surfaceType.value);
    }

    /* Generate ATL08 Resource Name */
    char* resource08 = StringLib::duplicate(parms->getResource());
    resource08[4] = '8';

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
        /* Create H5Coro Contexts */
        context = new H5Coro::Context(parms->asset.asset, parms->getResource());
        context08 = new H5Coro::Context(parms->asset.asset, resource08);

        /* Create Readers */
        threadMut.lock();
        {
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
        }
        threadMut.unlock();

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

    /* Clean Up */
    delete [] resource08;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::~Atl03Reader (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete outQ;

    delete context;
    delete context08;

    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Region::Region (const info_t* info):
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
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

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
            num_photons  = 0;
            for(int i = 0; i < num_segments; i++)
            {
                num_photons += segment_ph_cnt[i];
            }
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
Atl03Reader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl03Reader::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl03Reader::Region::polyregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        /* Test Inclusion */
        const bool inclusion = info->reader->parms->polyIncludes(segment_lon[segment], segment_lat[segment]);

        /* Segments with zero photon count may contain invalid coordinates,
           making them unsuitable for inclusion in polygon tests. */

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
void Atl03Reader::Region::rasterregion (const info_t* info)
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
            const bool inclusion = info->reader->parms->maskIncludes(segment_lon[segment], segment_lat[segment]);
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
Atl03Reader::Atl03Data::Atl03Data (const info_t* info, const Region& region):
    read_yapc           (info->reader->parms->stages[Icesat2Fields::STAGE_YAPC] && (info->reader->parms->yapc.version == 0) && (info->reader->parms->version.value >= 6)),
    sc_orient           (info->reader->context,                                "/orbit_info/sc_orient"),
    velocity_sc         (info->reader->context, FString("%s/%s", info->prefix, "geolocation/velocity_sc").c_str(),      H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_delta_time  (info->reader->context, FString("%s/%s", info->prefix, "geolocation/delta_time").c_str(),       0, region.first_segment, region.num_segments),
    segment_id          (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_id").c_str(),       0, region.first_segment, region.num_segments),
    segment_dist_x      (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_dist_x").c_str(),   0, region.first_segment, region.num_segments),
    solar_elevation     (info->reader->context, FString("%s/%s", info->prefix, "geolocation/solar_elevation").c_str(),  0, region.first_segment, region.num_segments),
    dist_ph_along       (info->reader->context, FString("%s/%s", info->prefix, "heights/dist_ph_along").c_str(),        0, region.first_photon,  region.num_photons),
    dist_ph_across      (info->reader->context, FString("%s/%s", info->prefix, "heights/dist_ph_across").c_str(),       0, region.first_photon,  region.num_photons),
    h_ph                (info->reader->context, FString("%s/%s", info->prefix, "heights/h_ph").c_str(),                 0, region.first_photon,  region.num_photons),
    signal_conf_ph      (info->reader->context, FString("%s/%s", info->prefix, "heights/signal_conf_ph").c_str(),       info->reader->signalConfColIndex, region.first_photon,  region.num_photons),
    quality_ph          (info->reader->context, FString("%s/%s", info->prefix, "heights/quality_ph").c_str(),           0, region.first_photon,  region.num_photons),
    weight_ph           (read_yapc ? info->reader->context : NULL, FString("%s/%s", info->prefix, "heights/weight_ph").c_str(), 0, region.first_photon,  region.num_photons),
    lat_ph              (info->reader->context, FString("%s/%s", info->prefix, "heights/lat_ph").c_str(),               0, region.first_photon,  region.num_photons),
    lon_ph              (info->reader->context, FString("%s/%s", info->prefix, "heights/lon_ph").c_str(),               0, region.first_photon,  region.num_photons),
    delta_time          (info->reader->context, FString("%s/%s", info->prefix, "heights/delta_time").c_str(),           0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (info->reader->context, FString("%s/%s", info->prefix, "bckgrd_atlas/delta_time").c_str()),
    bckgrd_rate         (info->reader->context, FString("%s/%s", info->prefix, "bckgrd_atlas/bckgrd_rate").c_str()),
    anc_geo_data        (NULL),
    anc_ph_data         (NULL)
{
    const FieldList<string>& geo_fields = info->reader->parms->atl03GeoFields;
    const FieldList<string>& photon_fields = info->reader->parms->atl03PhFields;

    try
    {
        /* Read Ancillary Geolocation Fields */
        if(geo_fields.length() > 0)
        {
            anc_geo_data = new H5DArrayDictionary(Icesat2Fields::EXPECTED_NUM_FIELDS);
            for(int i = 0; i < geo_fields.length(); i++)
            {
                const string& field_name = geo_fields[i];
                const char* group_name = "geolocation";
                if( (field_name[0] == 't' && field_name[1] == 'i' && field_name[2] == 'd') ||
                    (field_name[0] == 'g' && field_name[1] == 'e' && field_name[2] == 'o') ||
                    (field_name[0] == 'd' && field_name[1] == 'e' && field_name[2] == 'm') ||
                    (field_name[0] == 'd' && field_name[1] == 'a' && field_name[2] == 'c') )
                {
                    group_name = "geophys_corr";
                }
                const FString dataset_name("%s/%s", group_name, field_name.c_str());
                H5DArray* array = new H5DArray(info->reader->context, FString("%s/%s", info->prefix, dataset_name.c_str()).c_str(), 0, region.first_segment, region.num_segments);
                const bool status = anc_geo_data->add(field_name.c_str(), array);
                if(!status) delete array;
                assert(status); // the dictionary add should never fail
            }
        }

        /* Read Ancillary Photon Fields */
        if(photon_fields.length() > 0)
        {
            anc_ph_data = new H5DArrayDictionary(Icesat2Fields::EXPECTED_NUM_FIELDS);
            for(int i = 0; i < photon_fields.length(); i++)
            {
                const string& field_name = photon_fields[i];
                const FString dataset_name("heights/%s", field_name.c_str());
                H5DArray* array = new H5DArray(info->reader->context, FString("%s/%s", info->prefix, dataset_name.c_str()).c_str(), 0, region.first_photon,  region.num_photons);
                const bool status = anc_ph_data->add(field_name.c_str(), array);
                if(!status) delete array;
                assert(status); // the dictionary add should never fail
            }
        }

        /* Join Hardcoded Reads */
        sc_orient.join(info->reader->read_timeout_ms, true);
        velocity_sc.join(info->reader->read_timeout_ms, true);
        segment_delta_time.join(info->reader->read_timeout_ms, true);
        segment_id.join(info->reader->read_timeout_ms, true);
        segment_dist_x.join(info->reader->read_timeout_ms, true);
        solar_elevation.join(info->reader->read_timeout_ms, true);
        dist_ph_along.join(info->reader->read_timeout_ms, true);
        dist_ph_across.join(info->reader->read_timeout_ms, true);
        h_ph.join(info->reader->read_timeout_ms, true);
        signal_conf_ph.join(info->reader->read_timeout_ms, true);
        quality_ph.join(info->reader->read_timeout_ms, true);
        if(read_yapc) weight_ph.join(info->reader->read_timeout_ms, true);
        lat_ph.join(info->reader->read_timeout_ms, true);
        lon_ph.join(info->reader->read_timeout_ms, true);
        delta_time.join(info->reader->read_timeout_ms, true);
        bckgrd_delta_time.join(info->reader->read_timeout_ms, true);
        bckgrd_rate.join(info->reader->read_timeout_ms, true);

        /* Join Ancillary Geolocation Reads */
        if(anc_geo_data)
        {
            H5DArray* array = NULL;
            const char* dataset_name = anc_geo_data->first(&array);
            while(dataset_name != NULL)
            {
                array->join(info->reader->read_timeout_ms, true);
                dataset_name = anc_geo_data->next(&array);
            }
        }

        /* Join Ancillary Photon Reads */
        if(anc_ph_data)
        {
            H5DArray* array = NULL;
            const char* dataset_name = anc_ph_data->first(&array);
            while(dataset_name != NULL)
            {
                array->join(info->reader->read_timeout_ms, true);
                dataset_name = anc_ph_data->next(&array);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to read ATL03 data: %s", e.what());
        delete anc_geo_data;
        delete anc_ph_data;
        throw;
    }
}


/*----------------------------------------------------------------------------
 * Atl03Data::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl03Data::~Atl03Data (void)
{
    delete anc_geo_data;
    delete anc_ph_data;
}

/*----------------------------------------------------------------------------
 * Atl08Class::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl08Class::Atl08Class (const info_t* info):
    enabled             (info->reader->parms->stages[Icesat2Fields::STAGE_ATL08]),
    phoreal             (info->reader->parms->stages[Icesat2Fields::STAGE_PHOREAL]),
    ancillary           (info->reader->parms->atl08Fields.length() > 0),
    classification      {NULL},
    relief              {NULL},
    landcover           {NULL},
    snowcover           {NULL},
    atl08_segment_id    (enabled ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/ph_segment_id").c_str()),
    atl08_pc_indx       (enabled ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/classed_pc_indx").c_str()),
    atl08_pc_flag       (enabled ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/classed_pc_flag").c_str()),
    atl08_ph_h          (phoreal ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/ph_h").c_str()),
    segment_id_beg      ((phoreal || ancillary) ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "land_segments/segment_id_beg").c_str()),
    segment_landcover   (phoreal ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "land_segments/segment_landcover").c_str()),
    segment_snowcover   (phoreal ? info->reader->context08 : NULL, FString("%s/%s", info->prefix, "land_segments/segment_snowcover").c_str()),
    anc_seg_data        (NULL),
    anc_seg_indices     (NULL)
{
    if(ancillary)
    {
        try
        {
            /* Allocate Ancillary Data Dictionary */
            anc_seg_data = new H5DArrayDictionary(Icesat2Fields::EXPECTED_NUM_FIELDS);

            /* Read Ancillary Fields */
            FieldList<string>& atl08_fields = info->reader->parms->atl08Fields;
            for(int i = 0; i < atl08_fields.length(); i++)
            {
                string field_str = atl08_fields[i];
                if(field_str.back() == '%') field_str.pop_back(); // handle interpolation request for atl08 ancillary fields
                const char* field_name = field_str.c_str();
                const FString dataset_name("%s/land_segments/%s", info->prefix, field_name);
                H5DArray* array = new H5DArray(info->reader->context08, dataset_name.c_str());
                const bool status = anc_seg_data->add(field_name, array);
                if(!status) delete array;
                assert(status); // the dictionary add should never fail
            }

            /* Join Ancillary Reads */
            H5DArray* array = NULL;
            const char* dataset_name = anc_seg_data->first(&array);
            while(dataset_name != NULL)
            {
                array->join(info->reader->read_timeout_ms, true);
                dataset_name = anc_seg_data->next(&array);
            }
        }
        catch(const RunTimeException& e)
        {
            delete anc_seg_data;
            throw;
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl08Class::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl08Class::~Atl08Class (void)
{
    delete [] classification;
    delete [] relief;
    delete [] landcover;
    delete [] snowcover;
    delete anc_seg_data;
    delete [] anc_seg_indices;
}

/*----------------------------------------------------------------------------
 * Atl08Class::classify
 *----------------------------------------------------------------------------*/
void Atl03Reader::Atl08Class::classify (const info_t* info, const Region& region, const Atl03Data& atl03)
{
    /* Do Nothing If Not Enabled */
    if(!enabled)
    {
        return;
    }

    /* Wait for Reads to Complete */
    atl08_segment_id.join(info->reader->read_timeout_ms, true);
    atl08_pc_indx.join(info->reader->read_timeout_ms, true);
    atl08_pc_flag.join(info->reader->read_timeout_ms, true);
    if(phoreal || ancillary)
    {
        segment_id_beg.join(info->reader->read_timeout_ms, true);
    }
    if(phoreal)
    {
        atl08_ph_h.join(info->reader->read_timeout_ms, true);
        segment_landcover.join(info->reader->read_timeout_ms, true);
        segment_snowcover.join(info->reader->read_timeout_ms, true);
    }

    /* Allocate ATL08 Classification Array */
    const long num_photons = atl03.dist_ph_along.size;
    classification = new uint8_t [num_photons];

    /* Allocate PhoREAL Arrays */
    if(phoreal)
    {
        relief = new float [num_photons];
        landcover = new uint8_t [num_photons];
        snowcover = new uint8_t [num_photons];
    }

    if(ancillary)
    {
        anc_seg_indices = new int32_t [num_photons];
    }

    /* Populate ATL08 Classifications */
    int32_t atl03_photon = 0;
    int32_t atl08_photon = 0;
    int32_t atl08_segment_index = 0;
    for(int atl03_segment_index = 0; atl03_segment_index < atl03.segment_id.size; atl03_segment_index++)
    {
        const int32_t atl03_segment = atl03.segment_id[atl03_segment_index];

        /* Get ATL08 Land Segment Index */
        if(phoreal || ancillary)
        {
            while( (atl08_segment_index < (segment_id_beg.size - 1)) &&
                   (segment_id_beg[atl08_segment_index + 1] <= atl03_segment) )
            {
                atl08_segment_index++;
            }
        }

        /* Get Per Photon Values */
        const int32_t atl03_segment_count = region.segment_ph_cnt[atl03_segment_index];
        for(int atl03_count = 1; atl03_count <= atl03_segment_count; atl03_count++)
        {
            /* Go To Segment */
            while( (atl08_photon < atl08_segment_id.size) && // atl08 photon is valid
                   (atl08_segment_id[atl08_photon] < atl03_segment) )
            {
                atl08_photon++;
            }

            while( (atl08_photon < atl08_segment_id.size) && // atl08 photon is valid
                   (atl08_segment_id[atl08_photon] == atl03_segment) &&
                   (atl08_pc_indx[atl08_photon] < atl03_count))
            {
                atl08_photon++;
            }

            /* Check Match */
            if( (atl08_photon < atl08_segment_id.size) &&
                (atl08_segment_id[atl08_photon] == atl03_segment) &&
                (atl08_pc_indx[atl08_photon] == atl03_count) )
            {
                /* Assign Classification */
                classification[atl03_photon] = (uint8_t)atl08_pc_flag[atl08_photon];

                /* Populate PhoREAL Fields */
                if(phoreal)
                {
                    relief[atl03_photon] = atl08_ph_h[atl08_photon];
                    landcover[atl03_photon] = (uint8_t)segment_landcover[atl08_segment_index];
                    snowcover[atl03_photon] = (uint8_t)segment_snowcover[atl08_segment_index];

                    /* Run ABoVE Classifier (if specified) */
                    if(info->reader->parms->phoreal.above_classifier && (classification[atl03_photon] != Icesat2Fields::ATL08_TOP_OF_CANOPY))
                    {
                        const uint8_t spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl03.sc_orient[0], (Icesat2Fields::track_t)info->track, info->pair);
                        if( (atl03.solar_elevation[atl03_segment_index] <= 5.0) &&
                            ((spot == 1) || (spot == 3) || (spot == 5)) &&
                            (atl03.signal_conf_ph[atl03_photon] == Icesat2Fields::CNF_SURFACE_HIGH) &&
                            ((relief[atl03_photon] >= 0.0) && (relief[atl03_photon] < 35.0)) )
                            /* TODO: check for valid ground photons in ATL08 segment */
                        {
                            /* Reassign Classification */
                            classification[atl03_photon] = Icesat2Fields::ATL08_TOP_OF_CANOPY;
                        }
                    }
                }

                /* Populate Ancillary Index */
                if(ancillary)
                {
                    anc_seg_indices[atl03_photon] = atl08_segment_index;
                }

                /* Go To Next ATL08 Photon */
                atl08_photon++;
            }
            else
            {
                /* Unclassified */
                classification[atl03_photon] = Icesat2Fields::ATL08_UNCLASSIFIED;

                /* Set PhoREAL Fields to Invalid */
                if(phoreal)
                {
                    relief[atl03_photon] = 0.0;
                    landcover[atl03_photon] = INVALID_FLAG;
                    snowcover[atl03_photon] = INVALID_FLAG;
                }

                /* Set Ancillary Index to Invalid */
                if(ancillary)
                {
                    anc_seg_indices[atl03_photon] = static_cast<int32_t>(INVALID_KEY);
                }
            }

            /* Go To Next ATL03 Photon */
            atl03_photon++;
        }
    }
}

/*----------------------------------------------------------------------------
 * Atl08Class::operator[]
 *----------------------------------------------------------------------------*/
uint8_t Atl03Reader::Atl08Class::operator[] (int index) const
{
    // This operator is called only after the classification array has been fully populated.
    // clang-tidy is not able to determine that the array is always populated.
    // We disable the related warning to avoid false positives.

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.UndefReturn)
    return classification[index];
}

/*----------------------------------------------------------------------------
 * YapcScore::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::YapcScore::YapcScore (const info_t* info, const Region& region, const Atl03Data& atl03):
    enabled {info->reader->parms->stages[Icesat2Fields::STAGE_YAPC]},
    score {NULL}
{
    /* Do Nothing If Not Enabled */
    if(!enabled)
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
    else if(info->reader->parms->yapc.version != 0) // read from file
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid YAPC version specified: %d", info->reader->parms->yapc.version.value);
    }
}

/*----------------------------------------------------------------------------
 * YapcScore::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::YapcScore::~YapcScore (void)
{
    delete [] score;
}

/*----------------------------------------------------------------------------
 * yapcV2
 *----------------------------------------------------------------------------*/
void Atl03Reader::YapcScore::yapcV2 (const info_t* info, const Region& region, const Atl03Data& atl03)
{
    /* YAPC Hard-Coded Parameters */
    const double MAXIMUM_HSPREAD = 15000.0; // meters
    const double HSPREAD_BINSIZE = 1.0; // meters
    const int MAX_KNN = 25;
    double nearest_neighbors[MAX_KNN];

    /* Shortcut to Settings */
    const YapcFields& settings = info->reader->parms->yapc;

    /* Score Photons
     *
     *   CANNOT THROW BELOW THIS POINT
     */

    /* Allocate Yapc Score Array */
    const int32_t num_photons = atl03.dist_ph_along.size;
    score = new uint8_t [num_photons];
    memset(score, 0, num_photons);

    /* Initialize Indices */
    int32_t ph_b0 = 0; // buffer start
    int32_t ph_b1 = 0; // buffer end
    int32_t ph_c0 = 0; // center start
    int32_t ph_c1 = 0; // center end

    /* Loop Through Each ATL03 Segment */
    const int32_t num_segments = atl03.segment_id.size;
    for(int segment_index = 0; segment_index < num_segments; segment_index++)
    {
        /* Determine Indices */
        ph_b0 += segment_index > 1 ? region.segment_ph_cnt[segment_index - 2] : 0; // Center - 2
        ph_c0 += segment_index > 0 ? region.segment_ph_cnt[segment_index - 1] : 0; // Center - 1
        ph_c1 += region.segment_ph_cnt[segment_index]; // Center
        ph_b1 += segment_index < (num_segments - 1) ? region.segment_ph_cnt[segment_index + 1] : 0; // Center + 1

        /* Calculate N and KNN */
        const int32_t N = region.segment_ph_cnt[segment_index];
        int knn = (settings.knn.value != 0) ? settings.knn.value : MAX(1, (sqrt((double)N) + 0.5) / 2);
        knn = MIN(knn, MAX_KNN); // truncate if too large

        /* Check Valid Extent (note check against knn)*/
        if((N <= knn) || (N < info->reader->parms->minPhotonCount.value)) continue;

        /* Calculate Distance and Height Spread */
        double min_h = atl03.h_ph[0];
        double max_h = min_h;
        double min_x = atl03.dist_ph_along[0];
        double max_x = min_x;
        for(int n = 1; n < N; n++)
        {
            const double h = atl03.h_ph[n];
            const double x = atl03.dist_ph_along[n];
            if(h < min_h) min_h = h;
            if(h > max_h) max_h = h;
            if(x < min_x) min_x = x;
            if(x > max_x) max_x = x;
        }
        const double hspread = max_h - min_h;
        const double xspread = max_x - min_x;

        /* Check Window */
        if(hspread <= 0.0 || hspread > MAXIMUM_HSPREAD || xspread <= 0.0)
        {
            mlog(ERROR, "Unable to perform YAPC selection due to invalid photon spread: %lf, %lf\n", hspread, xspread);
            continue;
        }

        /* Bin Photons to Calculate Height Span*/
        const int num_bins = (int)(hspread / HSPREAD_BINSIZE) + 1;
        int8_t* bins = new int8_t [num_bins];
        memset(bins, 0, num_bins);
        for(int n = 0; n < N; n++)
        {
            const unsigned int bin = (unsigned int)((atl03.h_ph[n] - min_h) / HSPREAD_BINSIZE);
            bins[bin] = 1; // mark that photon present
        }

        /* Determine Number of Bins with Photons to Calculate Height Span
        * (and remove potential gaps in telemetry bands) */
        int nonzero_bins = 0;
        for(int b = 0; b < num_bins; b++) nonzero_bins += bins[b];
        delete [] bins;

        /* Calculate Height Span */
        const double h_span = (nonzero_bins * HSPREAD_BINSIZE) / (double)N * (double)knn;

        /* Calculate Window Parameters */
        const double half_win_x = settings.win_x / 2.0;
        const double half_win_h = (settings.win_h != 0.0) ? settings.win_h / 2.0 : h_span / 2.0;

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
                const double delta_x = abs(atl03.dist_ph_along[x] - atl03.dist_ph_along[y]);
                if(delta_x > half_win_x) continue;

                /*  Calculate Weighted Distance */
                const double delta_h = abs(atl03.h_ph[x] - atl03.h_ph[y]);
                const double proximity = half_win_h - delta_h;

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
            score[y] = (uint8_t)((nearest_neighbor_sum / half_win_h) * 0xFF);
        }
    }
}

/*----------------------------------------------------------------------------
 * yapcV3
 *----------------------------------------------------------------------------*/
void Atl03Reader::YapcScore::yapcV3 (const info_t* info, const Region& region, const Atl03Data& atl03)
{
    /* YAPC Parameters */
    const YapcFields& settings = info->reader->parms->yapc;
    const double hWX = settings.win_x / 2; // meters
    const double hWZ = settings.win_h / 2; // meters

    /* Score Photons
     *
     *   CANNOT THROW BELOW THIS POINT
     */

    /* Allocate Photon Arrays */
    const int32_t num_segments = atl03.segment_id.size;
    const int32_t num_photons = atl03.dist_ph_along.size;
    score = new uint8_t [num_photons]; // class member freed in deconstructor
    double* ph_dist = new double[num_photons];  // local array freed below

    /* Populate Distance Array */
    int32_t ph_index = 0;
    for(int segment_index = 0; segment_index < num_segments; segment_index++)
    {
        for(int32_t ph_in_seg_index = 0; ph_in_seg_index < region.segment_ph_cnt[segment_index]; ph_in_seg_index++)
        {
            ph_dist[ph_index] = atl03.segment_dist_x[segment_index] + atl03.dist_ph_along[ph_index];
            ph_index++;
        }
    }

    /* Traverse Each Segment */
    ph_index = 0;
    for(int segment_index = 0; segment_index < num_segments; segment_index++)
    {
        /* Initialize Segment Parameters */
        const int32_t N = region.segment_ph_cnt[segment_index];
        double* ph_weights = new double[N]; // local array freed below
        int max_knn = settings.min_knn;
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
                const double x_dist = ph_dist[ph_index] - ph_dist[neighbor_index];
                if(x_dist <= hWX)
                {
                    /* Check Inside Vertical Window */
                    const double proximity = abs(atl03.h_ph[ph_index] - atl03.h_ph[neighbor_index]);
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
                double x_dist = ph_dist[neighbor_index] - ph_dist[ph_index]; // NOLINT [clang-analyzer-core.UndefinedBinaryOperatorResult]
                if(x_dist <= hWX)
                {
                    /* Check Inside Vertical Window */
                    const double proximity = abs(atl03.h_ph[ph_index] - atl03.h_ph[neighbor_index]);
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
            const double n = sqrt(proximities.length());
            const int knn = MAX(n, settings.min_knn);
            if(knn > max_knn) max_knn = knn;

            /* Calculate Sum of Weights*/
            const int num_nearest_neighbors = MIN(knn, proximities.length());
            double weight_sum = 0.0;
            for(int i = 0; i < num_nearest_neighbors; i++)
            {
                weight_sum += hWZ - proximities.get(i);
            }
            ph_weights[ph_in_seg_index] = weight_sum;

            /* Go To Next Photon */
            ph_index++;
        }

        /* Normalize Weights */
        for(int32_t ph_in_seg_index = 0; ph_in_seg_index < N; ph_in_seg_index++)
        {
            const double Wt = ph_weights[ph_in_seg_index] / (hWZ * max_knn);
            score[start_ph_index] = (uint8_t)(MIN(Wt * 255, 255));
            start_ph_index++;
        }

        /* Free Photon Weights Array */
        delete [] ph_weights;
    }

    /* Free Photon Distance Array */
    delete [] ph_dist;
}

/*----------------------------------------------------------------------------
 * YapcScore::operator[]
 *----------------------------------------------------------------------------*/
uint8_t Atl03Reader::YapcScore::operator[] (int index) const
{
    return score[index];
}

/*----------------------------------------------------------------------------
 * TrackState::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::TrackState::TrackState (const Atl03Data& atl03)
{
    ph_in               = 0;
    seg_in              = 0;
    seg_ph              = 0;
    start_segment       = 0;
    start_distance      = atl03.segment_dist_x[0];
    seg_distance        = 0.0;
    start_seg_portion   = 0.0;
    track_complete      = false;
    bckgrd_in           = 0;
    extent_segment      = 0;
    extent_valid        = true;
    extent_length       = 0.0;
}

/*----------------------------------------------------------------------------
 * TrackState::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::TrackState::~TrackState (void) = default;

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl03Reader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Atl03Reader* reader = info->reader;
    const Icesat2Fields* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};
    List<int32_t>* segment_indices = NULL;    // used for ancillary data
    List<int32_t>* photon_indices = NULL;     // used for ancillary data
    List<int32_t>* atl08_indices = NULL;      // used for ancillary data

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "atl03_subsetter", "{\"context\":\"%s\", \"track\":%d}", info->reader->context->name, info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Start Reading ATL08 Data */
        Atl08Class atl08(info);

        /* Subset to Region of Interest */
        const Region region(info);

        /* Read ATL03 Datasets */
        const Atl03Data atl03(info, region);

        /* Perform YAPC Scoring (if requested) */
        const YapcScore yapc(info, region, atl03);

        /* Perform ATL08 Classification (if requested) */
        atl08.classify(info, region, atl03);

        /* Initialize Track State */
        TrackState state(atl03);

        /* Increment Read Statistics */
        local_stats.segments_read = region.segment_ph_cnt.size;

        /* Calculate Length of Extent in Meters (used for distance) */
        state.extent_length = parms->extentLength.value;
        if(parms->distInSeg) state.extent_length *= ATL03_SEGMENT_LENGTH;

        /* Initialize Extent Counter */
        uint32_t extent_counter = 0;

        /* Traverse All Photons In Dataset */
        while(reader->active && !state.track_complete)
        {
            /* Setup Variables for Extent */
            int32_t current_photon = state.ph_in;
            int32_t current_segment = state.seg_in;
            int32_t current_count = state.seg_ph; // number of photons in current segment already accounted for
            bool extent_complete = false;
            bool step_complete = false;

            /* Set Extent State */
            state.start_seg_portion = atl03.dist_ph_along[current_photon] / ATL03_SEGMENT_LENGTH;
            state.extent_segment = state.seg_in;
            state.extent_valid = true;
            state.extent_photons.clear();

            /* Ancillary Extent Fields */
            if(atl03.anc_geo_data)
            {
                if(segment_indices) segment_indices->clear();
                else                segment_indices = new List<int32_t>;
            }

            /* Ancillary Photon Fields */
            if(atl03.anc_ph_data)
            {
                if(photon_indices) photon_indices->clear();
                else               photon_indices = new List<int32_t>;
            }

            /* Ancillary ATL08 Fields */
            if(atl08.anc_seg_data)
            {
                if(atl08_indices) atl08_indices->clear();
                else              atl08_indices = new List<int32_t>;
            }

            /* Traverse Photons Until Desired Along Track Distance Reached */
            while(!extent_complete || !step_complete)
            {
                /* Go to Photon's Segment */
                current_count++;
                while((current_segment < region.segment_ph_cnt.size) &&
                        (current_count > region.segment_ph_cnt[current_segment]))
                {
                    current_count = 1; // reset photons in segment
                    current_segment++; // go to next segment
                }

                /* Check Current Segment */
                if(current_segment >= atl03.segment_dist_x.size)
                {
                    mlog(ERROR, "Photons with no segments are detected is %s/%d     %d %ld %ld!", info->reader->context->name, info->track, current_segment, atl03.segment_dist_x.size, region.num_segments);
                    state.track_complete = true;
                    break;
                }

                /* Update Along Track Distance and Progress */
                const double delta_distance = atl03.segment_dist_x[current_segment] - state.start_distance;
                const double x_atc = delta_distance + atl03.dist_ph_along[current_photon];
                const int32_t along_track_segments = current_segment - state.extent_segment;

                /* Set Next Extent's First Photon */
                if((!step_complete) &&
                    ((!parms->distInSeg.value && x_atc >= parms->extentStep.value) ||
                    (parms->distInSeg.value && along_track_segments >= (int32_t)parms->extentStep.value)))
                {
                    state.ph_in = current_photon;
                    state.seg_in = current_segment;
                    state.seg_ph = current_count - 1;
                    step_complete = true;
                }

                /* Check if Photon within Extent's Length */
                if((!parms->distInSeg.value && x_atc < parms->extentLength.value) ||
                    (parms->distInSeg.value && along_track_segments < parms->extentLength.value))
                {
                    do
                    {
                        /* Set Signal Confidence Level */
                        int8_t atl03_cnf;
                        if(parms->surfaceType == Icesat2Fields::SRT_DYNAMIC)
                        {
                            /* When dynamic, the signal_conf_ph contains all 5 columns; and the
                            * code below chooses the signal confidence that is the highest
                            * value of the five */
                            const int32_t conf_index = current_photon * Icesat2Fields::NUM_SURFACE_TYPES;
                            atl03_cnf = Icesat2Fields::MIN_ATL03_CNF;
                            for(int i = 0; i < Icesat2Fields::NUM_SURFACE_TYPES; i++)
                            {
                                if(atl03.signal_conf_ph[conf_index + i] > atl03_cnf)
                                {
                                    atl03_cnf = atl03.signal_conf_ph[conf_index + i];
                                }
                            }
                        }
                        else
                        {
                            atl03_cnf = atl03.signal_conf_ph[current_photon];
                        }

                        /* Check Signal Confidence Level */
                        if(atl03_cnf < Icesat2Fields::CNF_POSSIBLE_TEP || atl03_cnf > Icesat2Fields::CNF_SURFACE_HIGH)
                        {
                            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 signal confidence: %d", atl03_cnf);
                        }
                        if(!parms->atl03Cnf[static_cast<Icesat2Fields::signal_conf_t>(atl03_cnf)])
                        {
                            break;
                        }

                        /* Set and Check ATL03 Photon Quality Level */
                        const Icesat2Fields::quality_ph_t quality_ph = static_cast<Icesat2Fields::quality_ph_t>(atl03.quality_ph[current_photon]);
                        if(quality_ph < Icesat2Fields::QUALITY_NOMINAL || quality_ph > Icesat2Fields::QUALITY_POSSIBLE_TEP)
                        {
                            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 photon quality: %d", quality_ph);
                        }
                        if(!parms->qualityPh[quality_ph])
                        {
                            break;
                        }

                        /* Set and Check ATL08 Classification */
                        Icesat2Fields::atl08_class_t atl08_class = Icesat2Fields::ATL08_UNCLASSIFIED;
                        if(atl08.classification)
                        {
                            atl08_class = static_cast<Icesat2Fields::atl08_class_t>(atl08[current_photon]);
                            if(atl08_class < 0 || atl08_class >= Icesat2Fields::NUM_ATL08_CLASSES)
                            {
                                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl08 classification: %d", atl08_class);
                            }
                            if(!parms->atl08Class[atl08_class])
                            {
                                break;
                            }
                        }

                        /* Set and Check YAPC Score */
                        uint8_t yapc_score = 0;
                        if(yapc.score) // dynamically calculated
                        {
                            yapc_score = yapc[current_photon];
                            if(yapc_score < parms->yapc.score)
                            {
                                break;
                            }
                        }
                        else if(atl03.read_yapc) // read from atl03 granule
                        {
                            yapc_score = atl03.weight_ph[current_photon];
                            if(yapc_score < parms->yapc.score)
                            {
                                break;
                            }
                        }

                        /* Check Region */
                        if(region.inclusion_ptr)
                        {
                            if(!region.inclusion_ptr[current_segment])
                            {
                                break;
                            }
                        }

                        /* Set PhoREAL Fields */
                        float relief = 0.0;
                        uint8_t landcover_flag = Atl08Class::INVALID_FLAG;
                        uint8_t snowcover_flag = Atl08Class::INVALID_FLAG;
                        if(atl08.phoreal)
                        {
                            /* Set Relief */
                            if(!parms->phoreal.use_abs_h)
                            {
                                relief = atl08.relief[current_photon];
                            }
                            else
                            {
                                relief = atl03.h_ph[current_photon];
                            }

                            /* Set Flags */
                            landcover_flag = atl08.landcover[current_photon];
                            snowcover_flag = atl08.snowcover[current_photon];
                        }

                        /* Add Photon to Extent */
                        const photon_t ph = {
                            .time_ns = Icesat2Fields::deltatime2timestamp(atl03.delta_time[current_photon]),
                            .latitude = atl03.lat_ph[current_photon],
                            .longitude = atl03.lon_ph[current_photon],
                            .x_atc = static_cast<float>(x_atc - (state.extent_length / 2.0)),
                            .y_atc = atl03.dist_ph_across[current_photon],
                            .height = atl03.h_ph[current_photon],
                            .relief = relief,
                            .landcover = landcover_flag,
                            .snowcover = snowcover_flag,
                            .atl08_class = static_cast<uint8_t>(atl08_class),
                            .atl03_cnf = atl03_cnf,
                            .quality_ph = static_cast<int8_t>(quality_ph),
                            .yapc_score = yapc_score
                        };
                        state.extent_photons.add(ph);

                        /* Index Photon for Ancillary Fields */
                        if(segment_indices)
                        {
                            segment_indices->add(current_segment);
                        }

                        /* Index Photon for Ancillary Fields */
                        if(photon_indices)
                        {
                            photon_indices->add(current_photon);
                        }

                        /* Index ATL08 Segment for Photon for Ancillary Fields */
                        if(atl08_indices)
                        {
                            atl08_indices->add(atl08.anc_seg_indices[current_photon]);
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
                if(current_photon >= atl03.dist_ph_along.size)
                {
                    state.track_complete = true;
                    break;
                }
            }

            /* Save Off Segment Distance to Include in Extent Record */
            state.seg_distance = state.start_distance + (state.extent_length / 2.0);

            /* Add Step to Start Distance */
            if(!parms->distInSeg)
            {
                state.start_distance += parms->extentStep.value; // step start distance

                /* Apply Segment Distance Correction and Update Start Segment */
                while( ((state.start_segment + 1) < atl03.segment_dist_x.size) &&
                        (state.start_distance >= atl03.segment_dist_x[state.start_segment + 1]) )
                {
                    state.start_distance += atl03.segment_dist_x[state.start_segment + 1] - atl03.segment_dist_x[state.start_segment];
                    state.start_distance -= ATL03_SEGMENT_LENGTH;
                    state.start_segment++;
                }
            }
            else // distance in segments
            {
                const int32_t next_segment = state.extent_segment + (int32_t)parms->extentStep.value;
                if(next_segment < atl03.segment_dist_x.size)
                {
                    state.start_distance = atl03.segment_dist_x[next_segment]; // set start distance to next extent's segment distance
                }
            }

            /* Check Photon Count */
            if(state.extent_photons.length() < parms->minPhotonCount.value)
            {
                state.extent_valid = false;
            }

            /* Check Along Track Spread */
            if(state.extent_photons.length() > 1)
            {
                const int32_t last = state.extent_photons.length() - 1;
                const double along_track_spread = state.extent_photons[last].x_atc - state.extent_photons[0].x_atc;
                if(along_track_spread < parms->minAlongTrackSpread.value)
                {
                    state.extent_valid = false;
                }
            }

            /* Create Extent Record */
            if(state.extent_valid || parms->passInvalid)
            {
                /* Generate Extent ID */
                const uint64_t extent_id = Icesat2Fields::generateExtentId(parms->rgt.value, parms->cycle.value, parms->region.value, info->track, info->pair, extent_counter);

                /* Build Extent and Ancillary Records */
                vector<RecordObject*> rec_list;
                try
                {
                    int rec_total_size = 0;
                    reader->generateExtentRecord(extent_id, info, state, atl03, rec_list, rec_total_size);
                    Atl03Reader::generateAncillaryRecords(extent_id, parms->atl03PhFields, atl03.anc_ph_data, Icesat2Fields::PHOTON_ANC_TYPE, photon_indices, rec_list, rec_total_size);
                    Atl03Reader::generateAncillaryRecords(extent_id, parms->atl03GeoFields, atl03.anc_geo_data, Icesat2Fields::EXTENT_ANC_TYPE, segment_indices, rec_list, rec_total_size);
                    Atl03Reader::generateAncillaryRecords(extent_id, parms->atl08Fields, atl08.anc_seg_data, Icesat2Fields::ATL08_ANC_TYPE, atl08_indices, rec_list, rec_total_size);

                    /* Send Records */
                    if(rec_list.size() == 1)
                    {
                        reader->postRecord(*(rec_list[0]), local_stats);
                    }
                    else if(rec_list.size() > 1)
                    {
                        /* Send Container Record */
                        ContainerRecord container(rec_list.size(), rec_total_size);
                        for(size_t i = 0; i < rec_list.size(); i++)
                        {
                            container.addRecord(*(rec_list[i]));
                        }
                        reader->postRecord(container, local_stats);
                    }
                }
                catch(const RunTimeException& e)
                {
                    alert(e.level(), e.code(), reader->outQ, &reader->active, "Error generating results for resource %s track %d.%d: %s", info->reader->context->name, info->track, info->pair, e.what());
                }

                /* Clean Up Records */
                for(const auto& rec: rec_list)
                {
                    delete rec;
                }
            }
            else // neither pair in extent valid
            {
                local_stats.extents_filtered++;
            }

            /* Bump Extent Counter */
            extent_counter++;
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s track %d.%d: %s", info->reader->context->name, info->track, info->pair, e.what());
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
            mlog(INFO, "Completed processing resource %s track %d.%d (f: %u, s: %u, d: %u)", info->reader->context->name, info->track, info->pair, local_stats.extents_filtered, local_stats.extents_sent, local_stats.extents_dropped);

            /* Indicate End of Data */
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s track %d.%d", status, info->reader->context->name, info->track, info->pair);
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s track %d.%d ... trying again", info->reader->context->name, info->track, info->pair);
                    }
                }
            }
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up Indices */
    delete segment_indices;
    delete photon_indices;
    delete atl08_indices;

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
double Atl03Reader::calculateBackground (TrackState& state, const Atl03Data& atl03)
{
    double background_rate = atl03.bckgrd_rate[atl03.bckgrd_rate.size - 1];
    while(state.bckgrd_in < atl03.bckgrd_rate.size)
    {
        const double curr_bckgrd_time = atl03.bckgrd_delta_time[state.bckgrd_in];
        const double segment_time = atl03.segment_delta_time[state.extent_segment];
        if(curr_bckgrd_time >= segment_time)
        {
            /* Interpolate Background Rate */
            if(state.bckgrd_in > 0)
            {
                const double prev_bckgrd_time = atl03.bckgrd_delta_time[state.bckgrd_in - 1];
                const double prev_bckgrd_rate = atl03.bckgrd_rate[state.bckgrd_in - 1];
                const double curr_bckgrd_rate = atl03.bckgrd_rate[state.bckgrd_in];

                const double bckgrd_run = curr_bckgrd_time - prev_bckgrd_time;
                const double bckgrd_rise = curr_bckgrd_rate - prev_bckgrd_rate;
                const double segment_to_bckgrd_delta = segment_time - prev_bckgrd_time;

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
        state.bckgrd_in++;
    }
    return background_rate;
}

/*----------------------------------------------------------------------------
 * calculateSegmentId
 *----------------------------------------------------------------------------*/
uint32_t Atl03Reader::calculateSegmentId (const TrackState& state, const Atl03Data& atl03)
{
    /* Calculate Segment ID (attempt to arrive at closest ATL06 segment ID represented by extent) */
    double atl06_segment_id = (double)atl03.segment_id[state.extent_segment]; // start with first segment in extent
    if(!parms->distInSeg)
    {
        atl06_segment_id += state.start_seg_portion; // add portion of first segment that first photon is included
        atl06_segment_id += (int)((parms->extentLength.value / ATL03_SEGMENT_LENGTH) / 2.0); // add half the length of the extent
    }
    else // distInSeg.value is true
    {
        atl06_segment_id += (int)(parms->extentLength.value / 2.0);
    }

    /* Round Up */
    return (uint32_t)(atl06_segment_id + 0.5);
}

/*----------------------------------------------------------------------------
 * generateExtentRecord
 *----------------------------------------------------------------------------*/
void Atl03Reader::generateExtentRecord (uint64_t extent_id, const info_t* info, TrackState& state, const Atl03Data& atl03, vector<RecordObject*>& rec_list, int& total_size)
{
    /* Calculate Extent Record Size */
    const int num_photons = state.extent_photons.length();
    const int extent_bytes = offsetof(extent_t, photons) + (sizeof(photon_t) * num_photons);

    /* Allocate and Initialize Extent Record */
    RecordObject* record            = new RecordObject(exRecType, extent_bytes);
    extent_t* extent                = reinterpret_cast<extent_t*>(record->getRecordData());
    extent->extent_id               = extent_id;
    extent->region                  = parms->region.value;
    extent->track                   = info->track;
    extent->pair                    = info->pair;
    extent->spacecraft_orientation  = atl03.sc_orient[0];
    extent->reference_ground_track  = parms->rgt.value;
    extent->cycle                   = parms->cycle.value;
    extent->segment_id              = calculateSegmentId(state, atl03);
    extent->segment_distance        = state.seg_distance;
    extent->extent_length           = state.extent_length;
    extent->background_rate         = calculateBackground(state, atl03);
    extent->solar_elevation         = atl03.solar_elevation[state.extent_segment];
    extent->photon_count            = state.extent_photons.length();

    /* Calculate Spacecraft Velocity */
    const int32_t sc_v_offset = state.extent_segment * 3;
    const double sc_v1 = atl03.velocity_sc[sc_v_offset + 0];
    const double sc_v2 = atl03.velocity_sc[sc_v_offset + 1];
    const double sc_v3 = atl03.velocity_sc[sc_v_offset + 2];
    const double spacecraft_velocity = sqrt((sc_v1*sc_v1) + (sc_v2*sc_v2) + (sc_v3*sc_v3));
    extent->spacecraft_velocity  = (float)spacecraft_velocity;

    /* Populate Photons */
    for(int32_t p = 0; p < state.extent_photons.length(); p++)
    {
        extent->photons[p] = state.extent_photons[p];
    }

    /* Add Extent Record */
    total_size += record->getAllocatedMemory();
    rec_list.push_back(record);
}

/*----------------------------------------------------------------------------
 * generateAncillaryRecords
 *----------------------------------------------------------------------------*/
void Atl03Reader::generateAncillaryRecords (uint64_t extent_id, const FieldList<string>& field_list, H5DArrayDictionary* field_dict, Icesat2Fields::anc_type_t type, List<int32_t>* indices, vector<RecordObject*>& rec_list, int& total_size)
{
    if((field_list.length() > 0) && field_dict && indices)
    {
        for(int i = 0; i < field_list.length(); i++)
        {
            /* Get Data Array */
            string anc_field = field_list[i];
            if(anc_field.back() == '%') anc_field.pop_back();
            const H5DArray* array = (*field_dict)[anc_field.c_str()];

            /* Create Ancillary Record */
            const int record_size = offsetof(AncillaryFields::element_array_t, data) +
                                    (array->elementSize() * indices->length());
            RecordObject* record = new RecordObject(AncillaryFields::ancElementRecType, record_size);
            AncillaryFields::element_array_t* data = reinterpret_cast<AncillaryFields::element_array_t*>(record->getRecordData());

            /* Populate Ancillary Record */
            data->extent_id = extent_id;
            data->anc_type = type;
            data->field_index = i;
            data->data_type = array->elementType();
            data->num_elements = indices->length();

            /* Populate Ancillary Data */
            uint64_t bytes_written = 0;
            for(int p = 0; p < indices->length(); p++)
            {
                const int index = indices->get(p);
                if(index != Atl03Reader::INVALID_INDICE)
                {
                    bytes_written += array->serialize(&data->data[bytes_written], index, 1);
                }
                else
                {
                    for(int b = 0; b < array->elementSize(); b++)
                    {
                        data->data[bytes_written++] = 0xFF;
                    }
                }
            }

            /* Add Ancillary Record */
            total_size += record->getAllocatedMemory();
            rec_list.push_back(record);
        }
    }
}

/*----------------------------------------------------------------------------
 * postRecord
 *----------------------------------------------------------------------------*/
void Atl03Reader::postRecord (RecordObject& record, stats_t& local_stats)
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
int Atl03Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<Atl03Reader*>(getLuaSelf(L, 1));
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
