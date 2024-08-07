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

#include <cmath>
#include <numeric>
#include <stdarg.h>

#include "OsApi.h"
#include "MsgQ.h"
#include "H5Coro.h"
#include "BathyReader.h"
#include "BathyClassifier.h"
#include "BathyRefractionCorrector.h"
#include "BathyUncertaintyCalculator.h"
#include "GeoLib.h"
#include "RasterObject.h"
#include "BathyFields.h"
#include "BathyParms.h"

using BathyFields::extent_t;
using BathyFields::photon_t;
using BathyFields::classifier_t;
using BathyFields::bathy_class_t;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyReader::OUTPUT_FILE_PREFIX = "bathy_spot";
const char* BathyReader::GLOBAL_BATHYMETRY_MASK_FILE_PATH = "/data/ATL24_Mask_v5_Raster.tif";
const double BathyReader::GLOBAL_BATHYMETRY_MASK_MAX_LAT = 84.25;
const double BathyReader::GLOBAL_BATHYMETRY_MASK_MIN_LAT = -79.0;
const double BathyReader::GLOBAL_BATHYMETRY_MASK_MAX_LON = 180.0;
const double BathyReader::GLOBAL_BATHYMETRY_MASK_MIN_LON = -180.0;
const double BathyReader::GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE = 0.25;
const uint32_t BathyReader::GLOBAL_BATHYMETRY_MASK_OFF_VALUE = 0xFFFFFFFF;

const char* BathyReader::phRecType = "bathyrec.photons";
const RecordObject::fieldDef_t BathyReader::phRecDef[] = {
    {"time",            RecordObject::TIME8,    offsetof(photon_t, time_ns),        1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"index_ph",        RecordObject::INT32,    offsetof(photon_t, index_ph),       1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"index_seg",       RecordObject::INT32,    offsetof(photon_t, index_seg),      1,  NULL, NATIVE_FLAGS},
    {"lat_ph",          RecordObject::DOUBLE,   offsetof(photon_t, lat_ph),         1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"lon_ph",          RecordObject::DOUBLE,   offsetof(photon_t, lon_ph),         1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"x_ph",            RecordObject::DOUBLE,   offsetof(photon_t, x_ph),           1,  NULL, NATIVE_FLAGS},
    {"y_ph",            RecordObject::DOUBLE,   offsetof(photon_t, y_ph),           1,  NULL, NATIVE_FLAGS},
    {"x_atc",           RecordObject::DOUBLE,   offsetof(photon_t, x_atc),          1,  NULL, NATIVE_FLAGS},
    {"y_atc",           RecordObject::DOUBLE,   offsetof(photon_t, y_atc),          1,  NULL, NATIVE_FLAGS},
    {"background_rate", RecordObject::DOUBLE,   offsetof(photon_t, background_rate),1,  NULL, NATIVE_FLAGS},
    {"ellipse_h",       RecordObject::FLOAT,    offsetof(photon_t, ellipse_h),      1,  NULL, NATIVE_FLAGS},
    {"ortho_h",         RecordObject::FLOAT,    offsetof(photon_t, ortho_h),        1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"surface_h",       RecordObject::FLOAT,    offsetof(photon_t, surface_h),      1,  NULL, NATIVE_FLAGS},
    {"yapc_score",      RecordObject::UINT8,    offsetof(photon_t, yapc_score),     1,  NULL, NATIVE_FLAGS},
    {"max_signal_conf", RecordObject::INT8,     offsetof(photon_t, max_signal_conf),1,  NULL, NATIVE_FLAGS},
    {"quality_ph",      RecordObject::INT8,     offsetof(photon_t, quality_ph),     1,  NULL, NATIVE_FLAGS},
};

const char* BathyReader::exRecType = "bathyrec";
const RecordObject::fieldDef_t BathyReader::exRecDef[] = {
    {"region",          RecordObject::UINT8,    offsetof(extent_t, region),                 1,  NULL, NATIVE_FLAGS},
    {"track",           RecordObject::UINT8,    offsetof(extent_t, track),                  1,  NULL, NATIVE_FLAGS},
    {"pair",            RecordObject::UINT8,    offsetof(extent_t, pair),                   1,  NULL, NATIVE_FLAGS},
    {"spot",            RecordObject::UINT8,    offsetof(extent_t, spot),                   1,  NULL, NATIVE_FLAGS},
    {"rgt",             RecordObject::UINT16,   offsetof(extent_t, reference_ground_track), 1,  NULL, NATIVE_FLAGS},
    {"cycle",           RecordObject::UINT8,    offsetof(extent_t, cycle),                  1,  NULL, NATIVE_FLAGS},
    {"utm_zone",        RecordObject::UINT8,    offsetof(extent_t, utm_zone),               1,  NULL, NATIVE_FLAGS},
    {"extent_id",       RecordObject::UINT64,   offsetof(extent_t, extent_id),              1,  NULL, NATIVE_FLAGS},
    {"wind_v",          RecordObject::FLOAT,    offsetof(extent_t, wind_v),                 1,  NULL, NATIVE_FLAGS},
    {"ndwi",            RecordObject::FLOAT,    offsetof(extent_t, ndwi),                   1,  NULL, NATIVE_FLAGS},
    {"photons",         RecordObject::USER,     offsetof(extent_t, photons),                0,  phRecType, NATIVE_FLAGS | RecordObject::BATCH} // variable length
};

const char* BathyReader::OBJECT_TYPE = "BathyReader";
const char* BathyReader::LUA_META_NAME = "BathyReader";
const struct luaL_Reg BathyReader::LUA_META_TABLE[] = {
    {"spoton",      luaSpotEnabled},
    {"classifieron",luaClassifierEnabled},
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(...)
 *----------------------------------------------------------------------------*/
int BathyReader::luaCreate (lua_State* L)
{
    BathyParms* parms = NULL;
    BathyClassifier* qtrees = NULL;
    BathyClassifier* coastnet = NULL;
    BathyRefractionCorrector* refraction = NULL;
    BathyUncertaintyCalculator* uncertainty = NULL;

    try
    {
        /* Get Parameters */
        parms = dynamic_cast<BathyParms*>(getLuaObject(L, 1, BathyParms::OBJECT_TYPE));
        qtrees = dynamic_cast<BathyClassifier*>(getLuaObject(L, 2, BathyClassifier::OBJECT_TYPE));
        coastnet = dynamic_cast<BathyClassifier*>(getLuaObject(L, 3, BathyClassifier::OBJECT_TYPE));
        refraction = dynamic_cast<BathyRefractionCorrector*>(getLuaObject(L, 4, BathyRefractionCorrector::OBJECT_TYPE));
        uncertainty = dynamic_cast<BathyUncertaintyCalculator*>(getLuaObject(L, 5, BathyUncertaintyCalculator::OBJECT_TYPE));
        const char* resource = getLuaString(L, 6);
        const char* outq_name = getLuaString(L, 7);
        const char* shared_directory = getLuaString(L, 8);
        const bool send_terminator = getLuaBoolean(L, 9, true, true);
        
        /* Build Classifier List */
        BathyClassifier* classifiers[BathyFields::NUM_CLASSIFIERS] = {
            qtrees,
            coastnet,
            NULL, // openoceans++
            NULL, // medianfilter
            NULL, // cshelph
            NULL, // bathypathfinder
            NULL, // pointnet
            NULL, // openoceans
            NULL  // ensemble
        };
        /* Return Reader Object */
        return createLuaObject(L, new BathyReader(L, parms, classifiers, refraction, uncertainty, resource, outq_name, shared_directory, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        if(qtrees) qtrees->releaseLuaObject();
        if(coastnet) coastnet->releaseLuaObject();
        if(refraction) refraction->releaseLuaObject();
        if(uncertainty) uncertainty->releaseLuaObject();
        mlog(e.level(), "Error creating BathyReader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void BathyReader::init (void)
{
    RECDEF(phRecType,       phRecDef,       sizeof(photon_t),       NULL);
    RECDEF(exRecType,       exRecDef,       sizeof(extent_t),       NULL /* "extent_id" */);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyReader::BathyReader (lua_State* L,
                          BathyParms* _parms,
                          BathyClassifier** _classifiers,
                          BathyRefractionCorrector* _refraction,
                          BathyUncertaintyCalculator* _uncertainty,
                          const char* _resource,
                          const char* outq_name,
                          const char* shared_directory,
                          bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    refraction(_refraction),
    uncertainty(_uncertainty),
    readTimeoutMs(parms->read_timeout * 1000),
    context(NULL),
    context09(NULL),
    bathyMask(NULL)
{
    assert(_parms);
    assert(_resource);
    assert(outq_name);

    /* Initialize Thread Count */
    threadCount = 0;

    /* Save Info */
    resource = StringLib::duplicate(_resource);
    sharedDirectory = StringLib::duplicate(shared_directory);

    /* Set Classifiers */
    for(int i = 0; i < BathyFields::NUM_CLASSIFIERS; i++)
    {
        classifiers[i] = _classifiers[i];
    }

    /* Set Signal Confidence Index */
    if(parms->surface_type == Icesat2Parms::SRT_DYNAMIC)
    {
        signalConfColIndex = H5Coro::ALL_COLS;
    }
    else
    {
        signalConfColIndex = static_cast<int>(parms->surface_type);
    }

    /* Create Publisher and File Pointer */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Create Global Bathymetry Mask */
    if(parms->reader.use_bathy_mask)
    {
        bathyMask = new GeoLib::TIFFImage(NULL, GLOBAL_BATHYMETRY_MASK_FILE_PATH);
    }

    /* Initialize Stats */
    memset(&stats, 0, sizeof(stats));

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
        context = new H5Coro::Context(parms->reader.asset, resource);
        context09 = new H5Coro::Context(parms->reader.asset09, parms->reader.resource09);

        /* Standard Data Product Variables */
        if(parms->reader.output_as_sdp)
        {
            /* Write Ancillary Data */
            FString ancillary_filename("%s/writer_ancillary.json", sharedDirectory);
            fileptr_t ancillary_file = fopen(ancillary_filename.c_str(), "w");
            if(ancillary_file == NULL)
            {
                char err_buf[256];
                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to create ancillary json file %s: %s", ancillary_filename.c_str(), strerror_r(errno, err_buf, sizeof(err_buf)));
            }
            const AncillaryData ancillary_data(context, readTimeoutMs);
            const char* ancillary_json = ancillary_data.tojson();
            fprintf(ancillary_file, "%s", ancillary_json);
            fclose(ancillary_file);
            delete [] ancillary_json;

            /* Write Orbit Info */
            FString orbit_filename("%s/writer_orbit.json", sharedDirectory);
            fileptr_t orbit_file = fopen(orbit_filename.c_str(), "w");
            if(orbit_file == NULL)
            {
                char err_buf[256];
                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to create orbit json file %s: %s", orbit_filename.c_str(), strerror_r(errno, err_buf, sizeof(err_buf)));
            }
            const OrbitInfo orbit_info(context, readTimeoutMs);
            const char* orbit_json = orbit_info.tojson();
            fprintf(orbit_file, "%s", orbit_json);
            fclose(orbit_file);
            delete [] orbit_json;
        }

        /* Parse Globals (throws) */
        parseResource(resource, granuleDate, startRgt, startCycle, startRegion, sdpVersion);

        /* Create Readers */
        for(int track = 1; track <= BathyParms::NUM_TRACKS; track++)
        {
            for(int pair = 0; pair < BathyParms::NUM_PAIR_TRACKS; pair++)
            {
                const int gt_index = (2 * (track - 1)) + pair;
                if(parms->beams[gt_index] && (parms->track == BathyParms::ALL_TRACKS || track == parms->track))
                {
                    info_t* info = new info_t;
                    info->reader = this;
                    info->parms = parms;
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
BathyReader::~BathyReader (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete context;
    delete context09;

    delete [] sharedDirectory;
    delete bathyMask;
    delete outQ;
    delete [] resource;
    
    for(int i = 0; i < BathyFields::NUM_CLASSIFIERS; i++)
    {
        if(classifiers[i]) classifiers[i]->releaseLuaObject();
    }
    
    if(parms) parms->releaseLuaObject();
    if(uncertainty) uncertainty->releaseLuaObject();
    if(refraction) refraction->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
BathyReader::Region::Region (const info_t* info):
    segment_lat    (info->reader->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str()),
    segment_lon    (info->reader->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str()),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        segment_lat.join(info->reader->readTimeoutMs, true);
        segment_lon.join(info->reader->readTimeoutMs, true);
        segment_ph_cnt.join(info->reader->readTimeoutMs, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(info->parms->raster.valid())
        {
            rasterregion(info);
        }
        else if(info->parms->points_in_poly > 0)
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
BathyReader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void BathyReader::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void BathyReader::Region::polyregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        bool inclusion = false;

        /* Project Segment Coordinate */
        const MathLib::coord_t segment_coord = {segment_lon[segment], segment_lat[segment]};
        const MathLib::point_t segment_point = MathLib::coord2point(segment_coord, info->parms->projection);

        /* Test Inclusion */
        if(MathLib::inpoly(info->parms->projected_poly, info->parms->points_in_poly, segment_point))
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
void BathyReader::Region::rasterregion (const info_t* info)
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
            const bool inclusion = info->parms->raster.includes(segment_lon[segment], segment_lat[segment]);
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
BathyReader::Atl03Data::Atl03Data (const info_t* info, const Region& region):
    sc_orient           (info->reader->context,                                "/orbit_info/sc_orient"),
    velocity_sc         (info->reader->context, FString("%s/%s", info->prefix, "geolocation/velocity_sc").c_str(),     H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_delta_time  (info->reader->context, FString("%s/%s", info->prefix, "geolocation/delta_time").c_str(),      0, region.first_segment, region.num_segments),
    segment_dist_x      (info->reader->context, FString("%s/%s", info->prefix, "geolocation/segment_dist_x").c_str(),  0, region.first_segment, region.num_segments),
    solar_elevation     (info->reader->context, FString("%s/%s", info->prefix, "geolocation/solar_elevation").c_str(), 0, region.first_segment, region.num_segments),
    sigma_h             (info->reader->context, FString("%s/%s", info->prefix, "geolocation/sigma_h").c_str(),         0, region.first_segment, region.num_segments),
    sigma_along         (info->reader->context, FString("%s/%s", info->prefix, "geolocation/sigma_along").c_str(),     0, region.first_segment, region.num_segments),
    sigma_across        (info->reader->context, FString("%s/%s", info->prefix, "geolocation/sigma_across").c_str(),    0, region.first_segment, region.num_segments),
    ref_azimuth         (info->reader->context, FString("%s/%s", info->prefix, "geolocation/ref_azimuth").c_str(),     0, region.first_segment, region.num_segments),
    ref_elev            (info->reader->context, FString("%s/%s", info->prefix, "geolocation/ref_elev").c_str(),        0, region.first_segment, region.num_segments),
    geoid               (info->reader->context, FString("%s/%s", info->prefix, "geophys_corr/geoid").c_str(),          0, region.first_segment, region.num_segments),
    dem_h               (info->reader->context, FString("%s/%s", info->prefix, "geophys_corr/dem_h").c_str(),          0, region.first_segment, region.num_segments),
    dist_ph_along       (info->reader->context, FString("%s/%s", info->prefix, "heights/dist_ph_along").c_str(),       0, region.first_photon,  region.num_photons),
    dist_ph_across      (info->reader->context, FString("%s/%s", info->prefix, "heights/dist_ph_across").c_str(),      0, region.first_photon,  region.num_photons),
    h_ph                (info->reader->context, FString("%s/%s", info->prefix, "heights/h_ph").c_str(),                0, region.first_photon,  region.num_photons),
    signal_conf_ph      (info->reader->context, FString("%s/%s", info->prefix, "heights/signal_conf_ph").c_str(),      info->reader->signalConfColIndex, region.first_photon,  region.num_photons),
    quality_ph          (info->reader->context, FString("%s/%s", info->prefix, "heights/quality_ph").c_str(),          0, region.first_photon,  region.num_photons),
    weight_ph           (info->reader->sdpVersion >= 6 ? info->reader->context : NULL, FString("%s/%s", info->prefix, "heights/weight_ph").c_str(), 0, region.first_photon,  region.num_photons),
    lat_ph              (info->reader->context, FString("%s/%s", info->prefix, "heights/lat_ph").c_str(),              0, region.first_photon,  region.num_photons),
    lon_ph              (info->reader->context, FString("%s/%s", info->prefix, "heights/lon_ph").c_str(),              0, region.first_photon,  region.num_photons),
    delta_time          (info->reader->context, FString("%s/%s", info->prefix, "heights/delta_time").c_str(),          0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (info->reader->context, FString("%s/%s", info->prefix, "bckgrd_atlas/delta_time").c_str()),
    bckgrd_rate         (info->reader->context, FString("%s/%s", info->prefix, "bckgrd_atlas/bckgrd_rate").c_str())
{
    sc_orient.join(info->reader->readTimeoutMs, true);
    velocity_sc.join(info->reader->readTimeoutMs, true);
    segment_delta_time.join(info->reader->readTimeoutMs, true);
    segment_dist_x.join(info->reader->readTimeoutMs, true);
    solar_elevation.join(info->reader->readTimeoutMs, true);
    sigma_h.join(info->reader->readTimeoutMs, true);
    sigma_along.join(info->reader->readTimeoutMs, true);
    sigma_across.join(info->reader->readTimeoutMs, true);
    ref_azimuth.join(info->reader->readTimeoutMs, true);
    ref_elev.join(info->reader->readTimeoutMs, true);
    geoid.join(info->reader->readTimeoutMs, true);
    dem_h.join(info->reader->readTimeoutMs, true);
    dist_ph_along.join(info->reader->readTimeoutMs, true);
    dist_ph_across.join(info->reader->readTimeoutMs, true);
    h_ph.join(info->reader->readTimeoutMs, true);
    signal_conf_ph.join(info->reader->readTimeoutMs, true);
    quality_ph.join(info->reader->readTimeoutMs, true);
    if(info->reader->sdpVersion >= 6) weight_ph.join(info->reader->readTimeoutMs, true);
    lat_ph.join(info->reader->readTimeoutMs, true);
    lon_ph.join(info->reader->readTimeoutMs, true);
    delta_time.join(info->reader->readTimeoutMs, true);
    bckgrd_delta_time.join(info->reader->readTimeoutMs, true);
    bckgrd_rate.join(info->reader->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * Atl09Class::Constructor
 *----------------------------------------------------------------------------*/
BathyReader::Atl09Class::Atl09Class (const info_t* info):
    valid       (false),
    met_u10m    (info->reader->context09, FString("profile_%d/low_rate/met_u10m", info->track).c_str()),
    met_v10m    (info->reader->context09, FString("profile_%d/low_rate/met_v10m", info->track).c_str()),
    delta_time  (info->reader->context09, FString("profile_%d/low_rate/delta_time", info->track).c_str())
{
    try
    {
        met_u10m.join(info->reader->readTimeoutMs, true);
        met_v10m.join(info->reader->readTimeoutMs, true);
        delta_time.join(info->reader->readTimeoutMs, true);
        valid = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "ATL09 data unavailable <%s>", info->parms->reader.resource09);
    }
}

/*----------------------------------------------------------------------------
 * AncillaryData::Constructor
 *----------------------------------------------------------------------------*/
BathyReader::AncillaryData::AncillaryData (H5Coro::Context* context, int timeout):
    atlas_sdp_gps_epoch (context, "/ancillary_data/atlas_sdp_gps_epoch"),
    data_end_utc        (context, "/ancillary_data/data_end_utc"),
    data_start_utc      (context, "/ancillary_data/data_start_utc"),
    end_cycle           (context, "/ancillary_data/end_cycle"),
    end_delta_time      (context, "/ancillary_data/end_delta_time"),
    end_geoseg          (context, "/ancillary_data/end_geoseg"),
    end_gpssow          (context, "/ancillary_data/end_gpssow"),
    end_gpsweek         (context, "/ancillary_data/end_gpsweek"),
    end_orbit           (context, "/ancillary_data/end_orbit"),
    end_region          (context, "/ancillary_data/end_region"),
    end_rgt             (context, "/ancillary_data/end_rgt"),
    release             (context, "/ancillary_data/release"),
    granule_end_utc     (context, "/ancillary_data/granule_end_utc"),
    granule_start_utc   (context, "/ancillary_data/granule_start_utc"),
    start_cycle         (context, "/ancillary_data/start_cycle"),
    start_delta_time    (context, "/ancillary_data/start_delta_time"),
    start_geoseg        (context, "/ancillary_data/start_geoseg"),
    start_gpssow        (context, "/ancillary_data/start_gpssow"),
    start_gpsweek       (context, "/ancillary_data/start_gpsweek"),
    start_orbit         (context, "/ancillary_data/start_orbit"),
    start_region        (context, "/ancillary_data/start_region"),
    start_rgt           (context, "/ancillary_data/start_rgt"),
    version             (context, "/ancillary_data/version")
{
    atlas_sdp_gps_epoch.join(timeout, true);
    data_end_utc.join(timeout, true);
    data_start_utc.join(timeout, true);
    end_cycle.join(timeout, true);
    end_delta_time.join(timeout, true);
    end_geoseg.join(timeout, true);
    end_gpssow.join(timeout, true);
    end_gpsweek.join(timeout, true);
    end_orbit.join(timeout, true);
    end_region.join(timeout, true);
    end_rgt.join(timeout, true);
    release.join(timeout, true);
    granule_end_utc.join(timeout, true);
    granule_start_utc.join(timeout, true);
    start_cycle.join(timeout, true);
    start_delta_time.join(timeout, true);
    start_geoseg.join(timeout, true);
    start_gpssow.join(timeout, true);
    start_gpsweek.join(timeout, true);
    start_orbit.join(timeout, true);
    start_region.join(timeout, true);
    start_rgt.join(timeout, true);
    version.join(timeout, true);
}

/*----------------------------------------------------------------------------
 * AncillaryData::tojson
 *----------------------------------------------------------------------------*/
const char* BathyReader::AncillaryData::tojson (void) const
{
    FString json_contents(R"({)"
    R"("atlas_sdp_gps_epoch":%lf,)"
    R"("data_end_utc":"%s",)"
    R"("data_start_utc":"%s",)"
    R"("end_cycle":%d,)"
    R"("end_delta_time":%lf,)"
    R"("end_geoseg":%d,)"
    R"("end_gpssow":%lf,)"
    R"("end_gpsweek":%d,)"
    R"("end_orbit":%d,)"
    R"("end_region":%d,)"
    R"("end_rgt":%d,)"
    R"("release":"%s",)"
    R"("granule_end_utc":"%s",)"
    R"("granule_start_utc":"%s",)"
    R"("start_cycle":%d,)"
    R"("start_delta_time":%lf,)"
    R"("start_geoseg":%d,)"
    R"("start_gpssow":%lf,)"
    R"("start_gpsweek":%d,)"
    R"("start_orbit":%d,)"
    R"("start_region":%d,)"
    R"("start_rgt":%d,)"
    R"("version":"%s")"
    R"(})",
    atlas_sdp_gps_epoch.value,
    data_end_utc.value,
    data_start_utc.value,
    end_cycle.value,
    end_delta_time.value,
    end_geoseg.value,
    end_gpssow.value,
    end_gpsweek.value,
    end_orbit.value,
    end_region.value,
    end_rgt.value,
    release.value,
    granule_end_utc.value,
    granule_start_utc.value,
    start_cycle.value,
    start_delta_time.value,
    start_geoseg.value,
    start_gpssow.value,
    start_gpsweek.value,
    start_orbit.value,
    start_region.value,
    start_rgt.value,
    version.value);

    return json_contents.c_str(true);
}

/*----------------------------------------------------------------------------
 * OrbitInfo::Constructor
 *----------------------------------------------------------------------------*/
BathyReader::OrbitInfo::OrbitInfo (H5Coro::Context* context, int timeout):
    crossing_time  (context, "/orbit_info/crossing_time"),
    cycle_number   (context, "/orbit_info/cycle_number"),
    lan            (context, "/orbit_info/lan"),
    orbit_number   (context, "/orbit_info/orbit_number"),
    rgt            (context, "/orbit_info/rgt"),
    sc_orient      (context, "/orbit_info/sc_orient"),
    sc_orient_time (context, "/orbit_info/sc_orient_time")
{
    crossing_time.join(timeout, true);
    cycle_number.join(timeout, true);
    lan.join(timeout, true);
    orbit_number.join(timeout, true);
    rgt.join(timeout, true);
    sc_orient.join(timeout, true);
    sc_orient_time.join(timeout, true);
}

/*----------------------------------------------------------------------------
 * OrbitInfo::tojson
 *----------------------------------------------------------------------------*/
const char* BathyReader::OrbitInfo::tojson (void) const
{
    FString json_contents(R"({)"
    R"("crossing_time":%lf,)"
    R"("cycle_number":%d,)"
    R"("lan":%lf,)"
    R"("orbit_number":%d,)"
    R"("rgt":%d,)"
    R"("sc_orient":%d,)"
    R"("sc_orient_time":%lf)"
    R"(})",
    crossing_time.value,
    cycle_number.value,
    lan.value,
    orbit_number.value,
    rgt.value,
    sc_orient.value,
    sc_orient_time.value);

    return json_contents.c_str(true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* BathyReader::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    BathyReader* reader = info->reader;
    const BathyParms* parms = info->parms;
    RasterObject* ndwiRaster = RasterObject::cppCreate(parms->reader.hls);

    /* Thread Variables */
    vector<extent_t*> extents;
    stats_t local_stats = {
        .photon_count = 0,
        .subaqueous_photons = 0,
        .qtrees_duration = 0.0,
        .coastnet_duration = 0.0
    };

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "atl03_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", parms->asset->getName(), reader->resource, info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(info);

        /* Read ATL03/09 Datasets */
        const Atl03Data atl03(info, region);
        const Atl09Class atl09(info);

        /* Initialize Extent State */
        List<photon_t> extent_photons; // list of individual photons in extent
        uint32_t extent_counter = 0;
        int32_t current_photon = 0; // index into the photon rate variables
        int32_t current_segment = 0; // index into the segment rate variables
        int32_t previous_segment = -1; // previous index used to determine when segment has changed (and segment level variables need to be changed)
        int32_t photon_in_segment = 0; // the photon number in the current segment
        int32_t bckgrd_index = 0; // background 50Hz group
        int32_t low_rate_index = 0; // ATL09 low rate group
        bool terminate_extent_on_boundary = false; // terminate the extent when a spatial subsetting boundary is encountered

        /* Initialize Segment Level Fields */
        float wind_v = 0.0;
        float ndwi = std::numeric_limits<float>::quiet_NaN();

        /* Get Dataset Level Parameters */
        GeoLib::UTMTransform utm_transform(region.segment_lat[0], region.segment_lon[0]);
        const uint8_t spot = Icesat2Parms::getSpotNumber((Icesat2Parms::sc_orient_t)atl03.sc_orient[0], (Icesat2Parms::track_t)info->track, info->pair);

        /* Traverse All Photons In Dataset */
        while(reader->active && (current_photon < atl03.dist_ph_along.size))
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
                mlog(ERROR, "Photons with no segments are detected in %s/%d (%d %ld %ld)!", reader->resource, spot, current_segment, atl03.segment_dist_x.size, region.num_segments);
                break;
            }

            do
            {
                /* Reset Boundary Termination */
                terminate_extent_on_boundary = false;

                /* Check Global Bathymetry Mask */
                if(reader->bathyMask)
                {
                    const double degrees_of_latitude = region.segment_lat[current_segment] - GLOBAL_BATHYMETRY_MASK_MIN_LAT;
                    const double latitude_pixels = degrees_of_latitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
                    const uint32_t y = static_cast<uint32_t>(latitude_pixels);

                    const double degrees_of_longitude =  region.segment_lon[current_segment] - GLOBAL_BATHYMETRY_MASK_MIN_LON;
                    const double longitude_pixels = degrees_of_longitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
                    const uint32_t x = static_cast<uint32_t>(longitude_pixels);

                    const uint32_t pixel = reader->bathyMask->getPixel(x, y);
                    if(pixel == GLOBAL_BATHYMETRY_MASK_OFF_VALUE)
                    {
                        terminate_extent_on_boundary = true;
                        break;
                    }
                }

                /* Check Region */
                if(region.inclusion_ptr)
                {
                    if(!region.inclusion_ptr[current_segment])
                    {
                        terminate_extent_on_boundary = true;
                        break;
                    }
                }

                /* Set Signal Confidence Level */
                int8_t atl03_cnf;
                if(parms->surface_type == Icesat2Parms::SRT_DYNAMIC)
                {
                    /* When dynamic, the signal_conf_ph contains all 5 columns; and the
                     * code below chooses the signal confidence that is the highest
                     * value of the five */
                    const int32_t conf_index = current_photon * Icesat2Parms::NUM_SURFACE_TYPES;
                    atl03_cnf = Icesat2Parms::ATL03_INVALID_CONFIDENCE;
                    for(int i = 0; i < Icesat2Parms::NUM_SURFACE_TYPES; i++)
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
                if(atl03_cnf < Icesat2Parms::CNF_POSSIBLE_TEP || atl03_cnf > Icesat2Parms::CNF_SURFACE_HIGH)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 signal confidence: %d", atl03_cnf);
                }
                if(!parms->atl03_cnf[atl03_cnf + Icesat2Parms::SIGNAL_CONF_OFFSET])
                {
                    break;
                }

                /* Set and Check ATL03 Photon Quality Level */
                const int8_t quality_ph = atl03.quality_ph[current_photon];
                if(quality_ph < Icesat2Parms::QUALITY_NOMINAL || quality_ph > Icesat2Parms::QUALITY_POSSIBLE_TEP)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 photon quality: %d", quality_ph);
                }
                if(!parms->quality_ph[quality_ph])
                {
                    break;
                }

                /* Set and Check YAPC Score */
                uint8_t yapc_score = 0;
                if(reader->sdpVersion >= 6)
                {
                    yapc_score = atl03.weight_ph[current_photon];
                    if(yapc_score < parms->yapc.score)
                    {
                        break;
                    }
                }

                /* Check DEM Delta */
                const float dem_delta = atl03.h_ph[current_photon] - atl03.dem_h[current_segment];
                if(dem_delta > parms->reader.max_dem_delta || dem_delta < parms->reader.min_dem_delta)
                {
                    break;
                }

                /* Calculate UTM Coordinates */
                const double latitude = atl03.lat_ph[current_photon];
                const double longitude = atl03.lon_ph[current_photon];
                const GeoLib::point_t coord = utm_transform.calculateCoordinates(latitude, longitude);
                if(utm_transform.in_error)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "unable to convert %lf,%lf to UTM zone %d", latitude, longitude, utm_transform.zone);
                }

                /* Save Off Latest Delta Time */
                const double current_delta_time = atl03.delta_time[current_photon];

                /* Calculate Segment Level Fields */
                if(previous_segment != current_segment)
                {
                    previous_segment = current_segment;

                    /* Calculate Wind Speed */
                    if(atl09.valid)
                    {
                        /* Find Closest ATL09 Low Rate Entry */
                        while((low_rate_index < (atl09.delta_time.size - 1)) && (atl09.delta_time[low_rate_index+1] < current_delta_time))
                        {
                            low_rate_index++;
                        }
                        wind_v = sqrt(pow(atl09.met_u10m[low_rate_index], 2.0) + pow(atl09.met_v10m[low_rate_index], 2.0));
                    }

                    /* Sample Raster for NDWI */
                    ndwi = std::numeric_limits<float>::quiet_NaN();
                    if(ndwiRaster && parms->reader.generate_ndwi)
                    {
                        const double gps = current_delta_time + (double)Icesat2Parms::ATLAS_SDP_EPOCH_GPS;
                        const MathLib::point_3d_t point = {
                            .x = region.segment_lon[current_segment],
                            .y = region.segment_lat[current_segment],
                            .z = 0.0 // since we are not sampling elevation data, this should be fine at zero
                        };
                        List<RasterSample*> slist(1);
                        const uint32_t err = ndwiRaster->getSamples(point, gps, slist);
                        if(!slist.empty()) ndwi = static_cast<float>(slist[0]->value);
                        else mlog(WARNING, "Unable to calculate NDWI for %s at %lf, %lf: %u", reader->resource, point.y, point.x, err);
                    }
                }

                /* Add Photon to Extent */
                const photon_t ph = {
                    .time_ns = Icesat2Parms::deltatime2timestamp(current_delta_time),
                    .index_ph = static_cast<int32_t>(region.first_photon) + current_photon,
                    .index_seg = static_cast<int32_t>(region.first_segment) + current_segment,
                    .lat_ph = latitude,
                    .lon_ph = longitude,
                    .x_ph = coord.x,
                    .y_ph = coord.y,
                    .x_atc = atl03.segment_dist_x[current_segment] + atl03.dist_ph_along[current_photon],
                    .y_atc = atl03.dist_ph_across[current_photon],
                    .background_rate = calculateBackground(current_segment, bckgrd_index, atl03),
                    .delta_h = 0.0, // populated by refraction correction
                    .surface_h = 0.0, // populated by sea surface finder
                    .ortho_h = atl03.h_ph[current_photon] - atl03.geoid[current_segment],
                    .ellipse_h = atl03.h_ph[current_photon],
                    .sigma_thu = 0.0, // populated by uncertainty calculation
                    .sigma_tvu = 0.0, // populated by uncertainty calculation
                    .processing_flags = 0x0,
                    .yapc_score = yapc_score,
                    .max_signal_conf = atl03_cnf,
                    .quality_ph = quality_ph,
                    .class_ph = static_cast<uint8_t>(BathyFields::UNCLASSIFIED),
                    .predictions = {0, 0, 0, 0, 0, 0, 0, 0}
                };
                extent_photons.add(ph);
            } while(false);

            /* Go to Next Photon */
            current_photon++;

            if((extent_photons.length() >= parms->reader.ph_in_extent) ||
               (current_photon >= atl03.dist_ph_along.size) ||
               (!extent_photons.empty() && terminate_extent_on_boundary))
            {
                /* Generate Extent ID */
                const uint64_t extent_id = Icesat2Parms::generateExtentId(reader->startRgt, reader->startCycle, reader->startRegion, info->track, info->pair, extent_counter);

                /* Calculate Extent Record Size */
                const int num_photons = extent_photons.length();
                const int extent_bytes = offsetof(extent_t, photons) + (sizeof(photon_t) * num_photons);

                /* Allocate and Initialize Extent Record */
                // RecordObject record(exRecType, extent_bytes);
                // extent_t* extent                = reinterpret_cast<extent_t*>(record.getRecordData());
                uint8_t* extent_buffer          = new uint8_t [extent_bytes];
                extent_t* extent                = reinterpret_cast<extent_t*>(extent_buffer);
                extent->region                  = reader->startRegion;
                extent->track                   = info->track;
                extent->pair                    = info->pair;
                extent->spot                    = spot;
                extent->reference_ground_track  = reader->startRgt;
                extent->cycle                   = reader->startCycle;
                extent->utm_zone                = utm_transform.zone;
                extent->wind_v                  = wind_v;
                extent->ndwi                    = ndwi;
                extent->photon_count            = extent_photons.length();
                extent->extent_id               = extent_id;

                /* Populate Photons */
                for(int32_t p = 0; p < extent_photons.length(); p++)
                {
                    extent->photons[p] = extent_photons[p];
                }

                /* Update Statistics */
                local_stats.photon_count += extent->photon_count;

                /* Export Data */
                //if(parms->return_inputs)
                //{
                //    /* Post Record */
                //    uint8_t* rec_buf = NULL;
                //    const int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
                //    int post_status = MsgQ::STATE_TIMEOUT;
                //    while(reader->active && (post_status = reader->outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);
                //    if(post_status <= 0)
                //    {
                //        mlog(ERROR, "Atl03 bathy reader failed to post %s to stream %s: %d", record.getRecordType(), reader->outQ->getName(), post_status);
                //    }
                //}

                /* Add Extent */
                extents.push_back(extent);

                /* Update Extent Counters */
                extent_counter++;
                extent_photons.clear();
            }
        }

        /* Run Qtrees on Extents */
        if(parms->reader.classifiers[BathyFields::QTREES])
        {
            double start = TimeLib::latchtime();
            classifiers[BathyFields::QTREES]->run(extents);
            local_stats.qtrees_duration = TimeLib::latchtime() - start;
        }
        else // run native sea surface finder (since other classifiers need surface_h)
        {
            for(auto extent: extents)
            {
                findSeaSurface(*extent);
            }
        }

        /* Run Coastnet on Extents */
        if(parms->reader.classifiers[BathyFields::COASTNET])
        {
            double start = TimeLib::latchtime();
            classifiers[BathyFields::COASTNET]->run(extents);
            local_stats.coastnet_duration = TimeLib::latchtime() - start;
        }

        /* Process Extents */
        for(auto extent: extents)
        {
            refraction->run(*extent, atl03.ref_elev, atl03.ref_azimuth);
            parms->reader.uncertainty->run(*extent, atl03.sigma_across, atl03.sigma_along, atl03.sigma_h, atl03.ref_elev);
        }

        /* Write Extent to CSV File*/
        reader->writeCSV(extents, spot, local_stats);
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on resource %s track %d.%d: %s", reader->resource, info->track, info->pair, e.what());
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Count Completion */
        reader->numComplete++;
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", reader->resource);

            /* Update Statistics */
            reader->stats.photon_count += local_stats.photon_count;

            /* Indicate End of Data */
            if(reader->sendTerminator)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(reader->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = reader->outQ->postCopy("", 0, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) to post terminator for %s", status, reader->resource);
                        break;
                    }
                    else if(status == MsgQ::STATE_TIMEOUT)
                    {
                        mlog(INFO, "Timeout posting terminator for %s ... trying again", reader->resource);
                    }
                }
            }
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up */
    delete info;
    delete ndwiRaster;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * calculateBackground
 *----------------------------------------------------------------------------*/
double BathyReader::calculateBackground (int32_t current_segment, int32_t& bckgrd_index, const Atl03Data& atl03)
{
    double background_rate = atl03.bckgrd_rate[atl03.bckgrd_rate.size - 1];
    while(bckgrd_index < atl03.bckgrd_rate.size)
    {
        const double curr_bckgrd_time = atl03.bckgrd_delta_time[bckgrd_index];
        const double segment_time = atl03.segment_delta_time[current_segment];
        if(curr_bckgrd_time >= segment_time)
        {
            /* Interpolate Background Rate */
            if(bckgrd_index > 0)
            {
                const double prev_bckgrd_time = atl03.bckgrd_delta_time[bckgrd_index - 1];
                const double prev_bckgrd_rate = atl03.bckgrd_rate[bckgrd_index - 1];
                const double curr_bckgrd_rate = atl03.bckgrd_rate[bckgrd_index];

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
        bckgrd_index++;
    }
    return background_rate;
}

/*----------------------------------------------------------------------------
 * findSeaSurface
 *----------------------------------------------------------------------------*/
void BathyReader::findSeaSurface (extent_t& extent)
{
    BathyParms::reader_t& p = parms->reader;
    try
    {
        /* initialize stats on photons */
        double min_h = std::numeric_limits<double>::max();
        double max_h = std::numeric_limits<double>::min();
        double min_t = std::numeric_limits<double>::max();
        double max_t = std::numeric_limits<double>::min();
        double avg_bckgnd = 0.0;

        /* build list photon heights */
        vector<double> heights;
        for(long i = 0; i < extent.photon_count; i++)
        {
            const double height = extent.photons[i].ortho_h;
            const double time_secs = static_cast<double>(extent.photons[i].time_ns) / 1000000000.0;

            /* get min and max height */
            if(height < min_h) min_h = height;
            if(height > max_h) max_h = height;

            /* get min and max time */
            if(time_secs < min_t) min_t = time_secs;
            if(time_secs > max_t) max_t = time_secs;

            /* accumulate background (divided out below) */
            avg_bckgnd = extent.photons[i].background_rate;

            /* add to list of photons to process */
            heights.push_back(height);
        }

        /* check if photons are left to process */
        if(heights.empty())
        {
            throw RunTimeException(WARNING, RTE_INFO, "No valid photons when determining sea surface");
        }

        /* calculate and check range */
        const double range_h = max_h - min_h;
        if(range_h <= 0 || range_h > p.max_range)
        {
            throw RunTimeException(ERROR, RTE_ERROR, "Invalid range <%lf> when determining sea surface", range_h);
        }

        /* calculate and check number of bins in histogram
        *  - the number of bins is increased by 1 in case the ceiling and the floor
        *    of the max range is both the same number */
        const long num_bins = static_cast<long>(std::ceil(range_h / p.bin_size)) + 1;
        if(num_bins <= 0 || num_bins > p.max_bins)
        {
            throw RunTimeException(ERROR, RTE_ERROR, "Invalid combination of range <%lf> and bin size <%lf> produced out of range histogram size <%ld>", range_h, p.bin_size, num_bins);
        }

        /* calculate average background */
        avg_bckgnd /= heights.size();

        /* build histogram of photon heights */
        vector<long> histogram(num_bins);
        std::for_each (std::begin(heights), std::end(heights), [&](const double h) {
            const long bin = static_cast<long>(std::floor((h - min_h) / p.bin_size));
            histogram[bin]++;
        });

        /* calculate mean and standard deviation of histogram */
        double bckgnd = 0.0;
        double stddev = 0.0;
        if(p.model_as_poisson)
        {
            const long num_shots = std::round((max_t - min_t) / 0.0001);
            const double bin_t = p.bin_size * 0.00000002 / 3.0; // bin size from meters to seconds
            const double bin_pe = bin_t * num_shots * avg_bckgnd; // expected value
            bckgnd = bin_pe;
            stddev = sqrt(bin_pe);
        }
        else
        {
            const double bin_avg = static_cast<double>(heights.size()) / static_cast<double>(num_bins);
            double accum = 0.0;
            std::for_each (std::begin(histogram), std::end(histogram), [&](const double h) {
                accum += (h - bin_avg) * (h - bin_avg);
            });
            bckgnd = bin_avg;
            stddev = sqrt(accum / heights.size());
        }

        /* build guassian kernel (from -k to k)*/
        const double kernel_size = 6.0 * stddev + 1.0;
        const long k = (static_cast<long>(std::ceil(kernel_size / p.bin_size)) & ~0x1) / 2;
        const long kernel_bins = 2 * k + 1;
        double kernel_sum = 0.0;
        vector<double> kernel(kernel_bins);
        for(long x = -k; x <= k; x++)
        {
            const long i = x + k;
            const double r = x / stddev;
            kernel[i] = exp(-0.5 * r * r);
            kernel_sum += kernel[i];
        }
        for(int i = 0; i < kernel_bins; i++)
        {
            kernel[i] /= kernel_sum;
        }

        /* build filtered histogram */
        vector<double> smoothed_histogram(num_bins);
        for(long i = 0; i < num_bins; i++)
        {
            double output = 0.0;
            long num_samples = 0;
            for(long j = -k; j <= k; j++)
            {
                const long index = i + k;
                if(index >= 0 && index < num_bins)
                {
                    output += kernel[j + k] * static_cast<double>(histogram[index]);
                    num_samples++;
                }
            }
            smoothed_histogram[i] = output * static_cast<double>(kernel_bins) / static_cast<double>(num_samples);
        }

        /* find highest peak */
        long highest_peak_bin = 0;
        double highest_peak = smoothed_histogram[0];
        for(int i = 1; i < num_bins; i++)
        {
            if(smoothed_histogram[i] > highest_peak)
            {
                highest_peak = smoothed_histogram[i];
                highest_peak_bin = i;
            }
        }

        /* find second highest peak */
        const long peak_separation_in_bins = static_cast<long>(std::ceil(p.min_peak_separation / p.bin_size));
        long second_peak_bin = -1; // invalid
        double second_peak = std::numeric_limits<double>::min();
        for(int i = 0; i < num_bins; i++)
        {
            if(std::abs(i - highest_peak_bin) > peak_separation_in_bins)
            {
                if(smoothed_histogram[i] > second_peak)
                {
                    second_peak = smoothed_histogram[i];
                    second_peak_bin = i;
                }
            }
        }

        /* determine which peak is sea surface */
        if( (second_peak_bin != -1) &&
            (second_peak * p.highest_peak_ratio >= highest_peak) ) // second peak is close in size to highest peak
        {
            /* select peak that is highest in elevation */
            if(highest_peak_bin < second_peak_bin)
            {
                highest_peak = second_peak;
                highest_peak_bin = second_peak_bin;
            }
        }

        /* check if sea surface signal is significant */
        const double signal_threshold = bckgnd + (stddev * p.signal_threshold);
        if(highest_peak < signal_threshold)
        {
            throw RunTimeException(WARNING, RTE_INFO, "Unable to determine sea surface (%lf < %lf)", highest_peak, signal_threshold);
        }

        /* calculate width of highest peak */
        const double peak_above_bckgnd = smoothed_histogram[highest_peak_bin] - bckgnd;
        const double peak_half_max = (peak_above_bckgnd * 0.4) + bckgnd;
        long peak_width = 1;
        for(long i = highest_peak_bin + 1; i < num_bins; i++)
        {
            if(smoothed_histogram[i] > peak_half_max) peak_width++;
            else break;
        }
        for(long i = highest_peak_bin - 1; i >= 0; i--)
        {
            if(smoothed_histogram[i] > peak_half_max) peak_width++;
            else break;
        }
        const double peak_stddev = (peak_width * p.bin_size) / 2.35;

        /* calculate sea surface height and label sea surface photons */
        const float surface_h = min_h + (highest_peak_bin * p.bin_size) + (p.bin_size / 2.0);
        const double min_surface_h = surface_h - (peak_stddev * p.surface_width);
        const double max_surface_h = surface_h + (peak_stddev * p.surface_width);
        for(long i = 0; i < extent.photon_count; i++)
        {
            extent.photons[i].surface_h = surface_h;
            if( extent.photons[i].ortho_h >= min_surface_h &&
                extent.photons[i].ortho_h <= max_surface_h )
            {
                extent.photons[i].class_ph = BathyFields::SEA_SURFACE;
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to find sea surface for spot %d [extend_id=0x%16lX]: %s", extent.spot, extent.extent_id, e.what());
        for(long i = 0; i < extent.photon_count; i++)
        {
            extent.photons[i].processing_flags |= BathyFields::SEA_SURFACE_UNDETECTED;
        }
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
void BathyReader::parseResource (const char* _resource, TimeLib::date_t& date, uint16_t& rgt, uint8_t& cycle, uint8_t& region, uint8_t& version)
{
    long val;

    if(StringLib::size(_resource) < 29)
    {
        rgt = 0;
        cycle = 0;
        region = 0;
        date.year = 0;
        date.month = 0;
        date.day = 0;
        return; // early exit on error
    }

    /* get year */
    char year_str[5];
    year_str[0] = _resource[6];
    year_str[1] = _resource[7];
    year_str[2] = _resource[8];
    year_str[3] = _resource[9];
    year_str[4] = '\0';
    if(StringLib::str2long(year_str, &val, 10))
    {
        date.year = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse year from resource %s: %s", _resource, year_str);
    }

    /* get month */
    char month_str[3];
    month_str[0] = _resource[10];
    month_str[1] = _resource[11];
    month_str[2] = '\0';
    if(StringLib::str2long(month_str, &val, 10))
    {
        date.month = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse month from resource %s: %s", _resource, month_str);
    }

    /* get month */
    char day_str[3];
    day_str[0] = _resource[12];
    day_str[1] = _resource[13];
    day_str[2] = '\0';
    if(StringLib::str2long(day_str, &val, 10))
    {
        date.day = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse day from resource %s: %s", _resource, day_str);
    }

    /* get RGT */
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

    /* get cycle */
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
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse cycle from resource %s: %s", _resource, cycle_str);
    }

    /* get region */
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
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse region from resource %s: %s", _resource, region_str);
    }

    /* get version */
    char version_str[4];
    version_str[0] = _resource[30];
    version_str[1] = _resource[31];
    version_str[2] = _resource[32];
    version_str[3] = '\0';
    if(StringLib::str2long(version_str, &val, 10))
    {
        version = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse version from resource %s: %s", _resource, version_str);
    }
}
/*----------------------------------------------------------------------------
 * writeCSV
 *----------------------------------------------------------------------------*/
void BathyReader::writeCSV (const vector<extent_t*>& extents, int spot, const stats_t& local_stats)
{
    /* Check for Empty */
    if(extents.empty()) return;

    /* Open JSON File */
    FString json_filename("%s/%s_%d.json", sharedDirectory, OUTPUT_FILE_PREFIX, spot);
    fileptr_t json_file = fopen(json_filename.c_str(), "w");
    if(json_file == NULL)
    {
        char err_buf[256];
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to create output json file %s: %s", json_filename.c_str(), strerror_r(errno, err_buf, sizeof(err_buf)));
    }

    /* Build JSON File */
    FString json_contents(R"({)"
    R"("track":%d,)"
    R"("pair":%d,)"
    R"("beam":"gt%d%c",)"
    R"("spot":%d,)"
    R"("year":%d,)"
    R"("month":%d,)"
    R"("day":%d,)"
    R"("rgt":%d,)"
    R"("cycle":%d,)"
    R"("region":%d,)"
    R"("utm_zone":%d,)"
    R"("photon_count":%ld,)"
    R"("subaqueous_photons":%ld,)"
    R"("qtrees_duration":%0.3lf,)"
    R"("coastnet_duration":%0.3lf)"
    R"(})",
    extents[0]->track,
    extents[0]->pair,
    extents[0]->track, extents[0]->pair == 0 ? 'l' : 'r',
    extents[0]->spot,
    granuleDate.year,
    granuleDate.month,
    granuleDate.day,
    extents[0]->reference_ground_track,
    extents[0]->cycle,
    extents[0]->region,
    extents[0]->utm_zone,
    local_stats.photon_count,
    local_stats.subaqueous_photons,
    local_stats.qtrees_duration,
    local_stats.coastnet_duration);

    /* Write and Close JSON File */
    fprintf(json_file, "%s", json_contents.c_str());
    fclose(json_file);

    /* Open Data File */
    FString filename("%s/%s_%d.csv", sharedDirectory, OUTPUT_FILE_PREFIX, spot);
    fileptr_t out_file = fopen(filename.c_str(), "w");
    if(out_file == NULL)
    {
        char err_buf[256];
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to create output daata file %s: %s", filename.c_str(), strerror_r(errno, err_buf, sizeof(err_buf)));
    }

    /* Write Header */
    fprintf(out_file, "index_ph,index_seg,time,lat_ph,lon_ph,x_ph,y_ph,x_atc,y_atc,background_rate,surface_h,ortho_h,ellipse_h,sigma_thu,sigma_tvu,delta_h,yapc_score,max_signal_conf,quality_ph,flags,");
    for(int j = 0; j < BathyFields::NUM_CLASSIFIERS; j++)
    {
        BathyFields::classifier_t classifier = static_cast<BathyFields::classifier_t>(j);
        fprintf(out_file, "%s,", BathyFields::classifier2str(classifier));
    }
    fprintf(out_file, "class_ph\n");

    /* Write Data */
    for(auto extent: extents)
    {
        for(unsigned i = 0; i < extent->photon_count; i++)
        {
            fprintf(out_file, "%d,", extent->photons[i].index_ph);
            fprintf(out_file, "%d,", extent->photons[i].index_seg);
            fprintf(out_file, "%ld,", extent->photons[i].time_ns);
            fprintf(out_file, "%lf,", extent->photons[i].lat_ph);
            fprintf(out_file, "%lf,", extent->photons[i].lon_ph);
            fprintf(out_file, "%lf,", extent->photons[i].x_ph);
            fprintf(out_file, "%lf,", extent->photons[i].y_ph);
            fprintf(out_file, "%lf,", extent->photons[i].x_atc);
            fprintf(out_file, "%lf,", extent->photons[i].y_atc);
            fprintf(out_file, "%lf,", extent->photons[i].background_rate);
            fprintf(out_file, "%f,", extent->photons[i].surface_h);
            fprintf(out_file, "%f,", extent->photons[i].ortho_h);
            fprintf(out_file, "%f,", extent->photons[i].ellipse_h);
            fprintf(out_file, "%f,", extent->photons[i].sigma_thu);
            fprintf(out_file, "%f,", extent->photons[i].sigma_tvu);
            fprintf(out_file, "%f,", extent->photons[i].delta_h);
            fprintf(out_file, "%d,", extent->photons[i].yapc_score);
            fprintf(out_file, "%d,", extent->photons[i].max_signal_conf);
            fprintf(out_file, "%d,", extent->photons[i].quality_ph);
            fprintf(out_file, "%u,", extent->photons[i].processing_flags);
            for(int j = 0; j < BathyFields::NUM_CLASSIFIERS; j++)
            {
                fprintf(out_file, "%d,", extent->photons[i].predictions[j]);
            }
            fprintf(out_file, "%d", extent->photons[i].class_ph);
            fprintf(out_file, "\n");
        }
    }

    /* Close Output File */
    fclose(out_file);
}

/*----------------------------------------------------------------------------
 * luaSpotEnabled - :spoton(<spot>) --> true|false
 *----------------------------------------------------------------------------*/
int BathyReader::luaSpotEnabled (lua_State* L)
{
    bool status = false;
    BathyReader* lua_obj = NULL;

    try
    {
        lua_obj = dynamic_cast<BathyReader*>(getLuaSelf(L, 1));
        const int spot = getLuaInteger(L, 2);
        if(spot >= 1 && spot <= Icesat2Parms::NUM_SPOTS)
        {
            status = lua_obj->parms->reader.spots[spot-1];
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error retrieving spot status: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaClassifierEnabled - :classifieron(<spot>) --> true|false
 *----------------------------------------------------------------------------*/
int BathyReader::luaClassifierEnabled (lua_State* L)
{
    bool status = false;
    BathyReader* lua_obj = NULL;

    try
    {
        lua_obj = dynamic_cast<BathyReader*>(getLuaSelf(L, 1));
        const char* classifier_str = getLuaString(L, 2);
        const classifier_t classifier = BathyFields::str2classifier(classifier_str);
        if(classifier != BathyFields::INVALID_CLASSIFIER)
        {
            const int index = static_cast<int>(classifier);
            status = lua_obj->parms->reader.classifiers[index];
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error retrieving classifier status: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int BathyReader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    BathyReader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<BathyReader*>(getLuaSelf(L, 1));
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
        LuaEngine::setAttrInt(L, "photon_count", lua_obj->stats.photon_count);

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
