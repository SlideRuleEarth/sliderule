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
#include <limits>
#include <stdarg.h>
#include <algorithm>

#include "OsApi.h"
#include "MsgQ.h"
#include "H5Coro.h"
#include "BathyDataFrame.h"
#include "GeoLib.h"
#include "BathyFields.h"
#include "BathyStats.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyDataFrame::OUTPUT_FILE_PREFIX = "bathy_spot";
const char* BathyDataFrame::GLOBAL_BATHYMETRY_MASK_FILE_PATH = "/data/ATL24_Mask_v5_Raster.tif";
const double BathyDataFrame::GLOBAL_BATHYMETRY_MASK_MAX_LAT = 84.25;
const double BathyDataFrame::GLOBAL_BATHYMETRY_MASK_MIN_LAT = -79.0;
const double BathyDataFrame::GLOBAL_BATHYMETRY_MASK_MAX_LON = 180.0;
const double BathyDataFrame::GLOBAL_BATHYMETRY_MASK_MIN_LON = -180.0;
const double BathyDataFrame::GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE = 0.25;
const uint32_t BathyDataFrame::GLOBAL_BATHYMETRY_MASK_OFF_VALUE = 0xFFFFFFFF;

const char* BathyDataFrame::LUA_META_NAME = "BathyDataFrame";
const struct luaL_Reg BathyDataFrame::LUA_META_TABLE[] = {
    {"isvalid",     luaIsValid},
    {NULL,          NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(...)
 *----------------------------------------------------------------------------*/
int BathyDataFrame::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 2, BathyFields::OBJECT_TYPE));
        const char* rqstq_name = getLuaString(L, 3);

        /* Return Reader Object */
        return createLuaObject(L, new BathyDataFrame(L, string(beam_str), _parms, rqstq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating BathyDataFrame: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyDataFrame::BathyDataFrame (lua_State* L, const string& beam_str, BathyFields* _parms, const char* rqstq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {   
        {"time_ns",             &time_ns},
        {"index_ph",            &index_ph},
        {"index_seg",           &index_seg},
        {"lat_ph",              lat_ph},
        {"lon_ph",              &lon_ph},
        {"x_ph",                &x_ph},
        {"y_ph",                &y_ph},
        {"x_atc",               &x_atc},
        {"y_atc",               &y_atc},
        {"background_rate",     &background_rate},
        {"delta_h",             &delta_h},
        {"surface_h",           &surface_h},
        {"ortho_h",             &ortho_h},
        {"ellipse_h",           &ellipse_h},
        {"sigma_thu",           &sigma_thu},
        {"sigma_tvu",           &sigma_tvu},
        {"processing_flags",    &processing_flags},
        {"yapc_score",          &yapc_score},
        {"max_signal_conf",     &max_signal_conf},
        {"quality_ph",          &quality_ph},
        {"class_ph",            &class_ph},
        {"predictions",         &predictions},
        {"wind_v",              &wind_v},
        {"ref_el",              &ref_el},
        {"ref_az",              &ref_az},
        {"sigma_across",        &sigma_across},
        {"sigma_along",         &sigma_along},
        {"sigma_h",             &sigma_h},
    }, 
    {
        {"spot",                &spot},
        {"beam",                &beam},
        {"track",               &track},
        {"pair",                &pair} 
    }),
    parmsPtr(_parms),
    parms(*_parms),
    rqstQ(rqstq_name),
    readTimeoutMs(parms.readTimeout.value * 1000),
    context(NULL),
    context09(NULL),
    bathyMask(NULL),
    beam(beam_str),
    valid(true)
{
    assert(rqstq_name);

    try
    {
        /* Get Standard Data Product Version for ATL03 */
        getResourceVersion(parms.resource.value.c_str(), sdpVersion);

        /* Set Signal Confidence Index */
        if(parms.surfaceType == Icesat2Fields::SRT_DYNAMIC)
        {
            signalConfColIndex = H5Coro::ALL_COLS;
        }
        else
        {
            signalConfColIndex = static_cast<int>(parms.surfaceType.value);
        }

        /* Create Global Bathymetry Mask */
        if(parms.useBathyMask.value)
        {
            bathyMask = new GeoLib::TIFFImage(NULL, GLOBAL_BATHYMETRY_MASK_FILE_PATH);
        }

        /* Set Thread Specific Trace ID for H5Coro */
        EventLib::stashId (traceId);

        /* Create H5Coro Contexts */
        context = new H5Coro::Context(parms.asset, parms.resource.value.c_str());
        context09 = new H5Coro::Context(parms.asset09, parms.atl09Resource.value.c_str());

        /* Create Reader */
        active = true;
        pid = new Thread(subsettingThread, this);
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, &rqstQ, &active, "Failure on resource %s: %s", parms.resource, e.what());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, &rqstQ, &active, "Failure on resource %s: %s", parms.resource, e.what());

        /* Indicate End of Data */
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyDataFrame::~BathyDataFrame (void)
{
    active = false;
    delete pid;

    delete context;
    delete context09;
    delete bathyMask;

    parmsPtr->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
BathyDataFrame::Region::Region (const BathyDataFrame& dataframe):
    segment_lat    (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/reference_photon_lat").c_str()),
    segment_lon    (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/segment_ph_cnt").c_str()),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        segment_lat.join(dataframe.readTimeoutMs, true);
        segment_lon.join(dataframe.readTimeoutMs, true);
        segment_ph_cnt.join(dataframe.readTimeoutMs, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(dataframe.parms.regionMask.provided)
        {
            rasterregion(dataframe);
        }
        else if(dataframe.parms.pointsInPolygon.value > 0)
        {
            polyregion(dataframe);
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
BathyDataFrame::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void BathyDataFrame::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void BathyDataFrame::Region::polyregion (const BathyDataFrame& dataframe)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        /* Test Inclusion */
        const bool inclusion = dataframe.parms.polyIncludes(segment_lon[segment], segment_lat[segment]);

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
void BathyDataFrame::Region::rasterregion (const BathyDataFrame& dataframe)
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
            const bool inclusion = dataframe.parms.maskIncludes(segment_lon[segment], segment_lat[segment]);
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
BathyDataFrame::Atl03Data::Atl03Data (const BathyDataFrame& dataframe, const Region& region):
    sc_orient           (dataframe.context,                                "/orbit_info/sc_orient"),
    velocity_sc         (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/velocity_sc").c_str(),     H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_delta_time  (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/delta_time").c_str(),      0, region.first_segment, region.num_segments),
    segment_dist_x      (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/segment_dist_x").c_str(),  0, region.first_segment, region.num_segments),
    solar_elevation     (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/solar_elevation").c_str(), 0, region.first_segment, region.num_segments),
    sigma_h             (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/sigma_h").c_str(),         0, region.first_segment, region.num_segments),
    sigma_along         (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/sigma_along").c_str(),     0, region.first_segment, region.num_segments),
    sigma_across        (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/sigma_across").c_str(),    0, region.first_segment, region.num_segments),
    ref_azimuth         (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/ref_azimuth").c_str(),     0, region.first_segment, region.num_segments),
    ref_elev            (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/ref_elev").c_str(),        0, region.first_segment, region.num_segments),
    geoid               (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geophys_corr/geoid").c_str(),          0, region.first_segment, region.num_segments),
    dem_h               (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "geophys_corr/dem_h").c_str(),          0, region.first_segment, region.num_segments),
    dist_ph_along       (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/dist_ph_along").c_str(),       0, region.first_photon,  region.num_photons),
    dist_ph_across      (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/dist_ph_across").c_str(),      0, region.first_photon,  region.num_photons),
    h_ph                (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/h_ph").c_str(),                0, region.first_photon,  region.num_photons),
    signal_conf_ph      (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/signal_conf_ph").c_str(),      dataframe.signalConfColIndex, region.first_photon,  region.num_photons),
    quality_ph          (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/quality_ph").c_str(),          0, region.first_photon,  region.num_photons),
    weight_ph           (dataframe.sdpVersion >= 6 ? dataframe.context : NULL, FString("%s/%s", dataframe.beam.value.c_str(), "heights/weight_ph").c_str(), 0, region.first_photon,  region.num_photons),
    lat_ph              (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/lat_ph").c_str(),              0, region.first_photon,  region.num_photons),
    lon_ph              (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/lon_ph").c_str(),              0, region.first_photon,  region.num_photons),
    delta_time          (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "heights/delta_time").c_str(),          0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "bckgrd_atlas/delta_time").c_str()),
    bckgrd_rate         (dataframe.context, FString("%s/%s", dataframe.beam.value.c_str(), "bckgrd_atlas/bckgrd_rate").c_str())
{
    sc_orient.join(dataframe.readTimeoutMs, true);
    velocity_sc.join(dataframe.readTimeoutMs, true);
    segment_delta_time.join(dataframe.readTimeoutMs, true);
    segment_dist_x.join(dataframe.readTimeoutMs, true);
    solar_elevation.join(dataframe.readTimeoutMs, true);
    sigma_h.join(dataframe.readTimeoutMs, true);
    sigma_along.join(dataframe.readTimeoutMs, true);
    sigma_across.join(dataframe.readTimeoutMs, true);
    ref_azimuth.join(dataframe.readTimeoutMs, true);
    ref_elev.join(dataframe.readTimeoutMs, true);
    geoid.join(dataframe.readTimeoutMs, true);
    dem_h.join(dataframe.readTimeoutMs, true);
    dist_ph_along.join(dataframe.readTimeoutMs, true);
    dist_ph_across.join(dataframe.readTimeoutMs, true);
    h_ph.join(dataframe.readTimeoutMs, true);
    signal_conf_ph.join(dataframe.readTimeoutMs, true);
    quality_ph.join(dataframe.readTimeoutMs, true);
    if(dataframe.sdpVersion >= 6) weight_ph.join(dataframe.readTimeoutMs, true);
    lat_ph.join(dataframe.readTimeoutMs, true);
    lon_ph.join(dataframe.readTimeoutMs, true);
    delta_time.join(dataframe.readTimeoutMs, true);
    bckgrd_delta_time.join(dataframe.readTimeoutMs, true);
    bckgrd_rate.join(dataframe.readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * Atl09Class::Constructor
 *----------------------------------------------------------------------------*/
BathyDataFrame::Atl09Class::Atl09Class (const BathyDataFrame& dataframe):
    valid       (false),
    met_u10m    (dataframe.context09, FString("profile_%d/low_rate/met_u10m", dataframe.track).c_str()),
    met_v10m    (dataframe.context09, FString("profile_%d/low_rate/met_v10m", dataframe.track).c_str()),
    delta_time  (dataframe.context09, FString("profile_%d/low_rate/delta_time", dataframe.track).c_str())
{
    try
    {
        met_u10m.join(dataframe.readTimeoutMs, true);
        met_v10m.join(dataframe.readTimeoutMs, true);
        delta_time.join(dataframe.readTimeoutMs, true);
        valid = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "ATL09 data unavailable <%s>", dataframe.parms.atl09Resource.value.c_str());
    }
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* BathyDataFrame::subsettingThread (void* parm)
{
    /* Get Thread Info */
    BathyDataFrame* dataframe_ptr = static_cast<BathyDataFrame*>(parm);
    BathyDataFrame& dataframe = *dataframe_ptr;
    const BathyFields& parms = dataframe.parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, dataframe.traceId, "atl03_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", parms.asset->getName(), parms.resource, dataframe.track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Region of Interest */
        const Region region(dataframe);

        /* Read ATL03/09 Datasets */
        const Atl03Data atl03(dataframe, region);
        const Atl09Class atl09(dataframe);

        /* Initialize Extent State */
        int32_t current_photon = 0; // index into the photon rate variables
        int32_t current_segment = 0; // index into the segment rate variables
        int32_t previous_segment = -1; // previous index used to determine when segment has changed (and segment level variables need to be changed)
        int32_t photon_in_segment = 0; // the photon number in the current segment
        int32_t bckgrd_index = 0; // background 50Hz group
        int32_t low_rate_index = 0; // ATL09 low rate group
        float wind_v = 0.0; // segment level wind speed
        bool on_boundary = true; // true when a spatial subsetting boundary is encountered

        /* Get Dataset Level Parameters */
        GeoLib::UTMTransform utm_transform(region.segment_lat[0], region.segment_lon[0]);
        const uint8_t spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl03.sc_orient[0], (Icesat2Fields::track_t)dataframe.track.value, dataframe.pair.value);

        /* Traverse All Photons In Dataset */
        while(dataframe.active && (current_photon < atl03.dist_ph_along.size))
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
                mlog(ERROR, "Photons with no segments are detected in %s/%s (%d %ld %ld)!", parms.resource.value.c_str(), parms.beam.value.c_str(), current_segment, atl03.segment_dist_x.size, region.num_segments);
                break;
            }

            do
            {
                /* Check Global Bathymetry Mask */
                if(dataframe.bathyMask)
                {
                    const double degrees_of_latitude = region.segment_lat[current_segment] - GLOBAL_BATHYMETRY_MASK_MIN_LAT;
                    const double latitude_pixels = degrees_of_latitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
                    const uint32_t y = static_cast<uint32_t>(latitude_pixels);

                    const double degrees_of_longitude =  region.segment_lon[current_segment] - GLOBAL_BATHYMETRY_MASK_MIN_LON;
                    const double longitude_pixels = degrees_of_longitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
                    const uint32_t x = static_cast<uint32_t>(longitude_pixels);

                    const GeoLib::TIFFImage::val_t pixel = dataframe.bathyMask->getPixel(x, y);
                    if(pixel.u32 == GLOBAL_BATHYMETRY_MASK_OFF_VALUE)
                    {
                        on_boundary = true;
                        break;
                    }
                }

                /* Check Region */
                if(region.inclusion_ptr)
                {
                    if(!region.inclusion_ptr[current_segment])
                    {
                        on_boundary = true;
                        break;
                    }
                }

                /* Set Signal Confidence Level */
                Icesat2Fields::signal_conf_t atl03_cnf;
                if(parms.surfaceType == Icesat2Fields::SRT_DYNAMIC)
                {
                    /* When dynamic, the signal_conf_ph contains all 5 columns; and the
                     * code below chooses the signal confidence that is the highest
                     * value of the five */
                    const int32_t conf_index = current_photon * Icesat2Fields::NUM_SURFACE_TYPES;
                    atl03_cnf = Icesat2Fields::CNF_POSSIBLE_TEP;
                    for(int i = 0; i < Icesat2Fields::NUM_SURFACE_TYPES; i++)
                    {
                        if(atl03.signal_conf_ph[conf_index + i] > atl03_cnf)
                        {
                            atl03_cnf = static_cast<Icesat2Fields::signal_conf_t>(atl03.signal_conf_ph[conf_index + i]);
                        }
                    }
                }
                else
                {
                    atl03_cnf = static_cast<Icesat2Fields::signal_conf_t>(atl03.signal_conf_ph[current_photon]);
                }

                /* Check Signal Confidence Level */
                if(!parms.atl03Cnf[atl03_cnf])
                {
                    break;
                }

                /* Set and Check ATL03 Photon Quality Level */
                const Icesat2Fields::quality_ph_t quality_ph = static_cast<Icesat2Fields::quality_ph_t>(atl03.quality_ph[current_photon]);
                if(!parms.qualityPh[quality_ph])
                {
                    break;
                }

                /* Set and Check YAPC Score */
                uint8_t yapc_score = 0;
                if(dataframe.sdpVersion >= 6)
                {
                    yapc_score = atl03.weight_ph[current_photon];
                    if(yapc_score < parms.yapc.value.score.value)
                    {
                        break;
                    }
                }

                /* Check DEM Delta */
                const float dem_delta = atl03.h_ph[current_photon] - atl03.dem_h[current_segment];
                if(dem_delta > parms.maxDemDelta.value || dem_delta < parms.minDemDelta.value)
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
                }

                /* Add Photon to DataFrame */
                dataframe.addRow(); // starts new row in dataframe
                dataframe.time_ns.append(Icesat2Fields::deltatime2timestamp(current_delta_time));
                dataframe.index_ph.append(static_cast<int32_t>(region.first_photon) + current_photon);
                dataframe.index_seg.append(static_cast<int32_t>(region.first_segment) + current_segment);
                dataframe.lat_ph.append(latitude);
                dataframe.lon_ph.append(longitude);
                dataframe.x_ph.append(coord.x);
                dataframe.y_ph.append(coord.y);
                dataframe.x_atc.append(atl03.segment_dist_x[current_segment] + atl03.dist_ph_along[current_photon]);
                dataframe.y_atc.append(atl03.dist_ph_across[current_photon]);
                dataframe.background_rate.append(calculateBackground(current_segment, bckgrd_index, atl03));
                dataframe.ortho_h.append(atl03.h_ph[current_photon] - atl03.geoid[current_segment]);
                dataframe.ellipse_h.append(atl03.h_ph[current_photon]);
                dataframe.yapc_score.append(yapc_score);
                dataframe.max_signal_conf.append(atl03_cnf);
                dataframe.quality_ph.append(quality_ph);
                dataframe.processing_flags.append(on_boundary ? BathyFields::ON_BOUNDARY : 0x00);

                /* Reset Boundary Termination */
                on_boundary = false;

            } while(false);

            /* Go to Next Photon */
            current_photon++;
        }

        /* Initialize Additional DataFrame Columns (populated later) */
        dataframe.class_ph.initialize(dataframe.length(), static_cast<uint8_t>(BathyFields::UNCLASSIFIED));
        dataframe.delta_h.initialize(dataframe.length(), 0.0); // populated by refraction correction
        dataframe.surface_h.initialize(dataframe.length(), 0.0); // populated by sea surface finder
        dataframe.sigma_thu.initialize(dataframe.length(), 0.0); // populated by uncertainty calculation
        dataframe.sigma_tvu.initialize(dataframe.length(), 0.0); // populated by uncertainty calculation
        dataframe.predictions.initialize(dataframe.length(), {0, 0, 0, 0, 0, 0, 0, 0});

        /* Find Sea Surface (if selection) */
        if(parms.findSeaSurface.value)
        {
            dataframe.findSeaSurface();
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), &dataframe.rqstQ, &dataframe.active, "Failure on resource %s track %d.%d: %s", parms.resource, dataframe.track, dataframe.pair, e.what());
        dataframe.valid = false;
    }

    /* Mark Completion */
    mlog(INFO, "Completed processing resource %s", parms.resource);
    dataframe.signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * calculateBackground
 *----------------------------------------------------------------------------*/
double BathyDataFrame::calculateBackground (int32_t current_segment, int32_t& bckgrd_index, const Atl03Data& atl03)
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
 * getResourceVersion
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
void BathyDataFrame::getResourceVersion (const char* _resource, int& version)
{
    long val;
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
 * findSeaSurface
 *----------------------------------------------------------------------------*/
void BathyDataFrame::findSeaSurface (void)
{
    const SurfaceFields& surface_parms = parms.surface.value;

    /* for each extent (p0 = start photon) */
    for(long p0 = 0; p0 < length(); p0 += parms.phInExtent.value)
    {
        /* calculate last photon in extent */
        long p1 = MIN(length(), p0 + parms.phInExtent.value);

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
            for(long i = p0; i < p1; i++)
            {
                const double height = ortho_h[i];
                const double time_secs = static_cast<double>(time_ns[i]) / 1000000000.0;

                /* get min and max height */
                if(height < min_h) min_h = height;
                if(height > max_h) max_h = height;

                /* get min and max time */
                if(time_secs < min_t) min_t = time_secs;
                if(time_secs > max_t) max_t = time_secs;

                /* accumulate background (divided out below) */
                avg_bckgnd = background_rate[i];

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
            if(range_h <= 0 || range_h > surface_parms.maxRange.value)
            {
                throw RunTimeException(ERROR, RTE_ERROR, "Invalid range <%lf> when determining sea surface", range_h);
            }

            /* calculate and check number of bins in histogram
            *  - the number of bins is increased by 1 in case the ceiling and the floor
            *    of the max range is both the same number */
            const long num_bins = static_cast<long>(std::ceil(range_h / surface_parms.binSize.value)) + 1;
            if(num_bins <= 0 || num_bins > surface_parms.maxBins.value)
            {
                throw RunTimeException(ERROR, RTE_ERROR, "Invalid combination of range <%lf> and bin size <%lf> produced out of range histogram size <%ld>", range_h, surface_parms.bin_size, num_bins);
            }

            /* calculate average background */
            avg_bckgnd /= heights.size();

            /* build histogram of photon heights */
            vector<long> histogram(num_bins);
            std::for_each (std::begin(heights), std::end(heights), [&](const double h) {
                const long bin = static_cast<long>(std::floor((h - min_h) / surface_parms.binSize.value));
                histogram[bin]++;
            });

            /* calculate mean and standard deviation of histogram */
            double bckgnd = 0.0;
            double stddev = 0.0;
            if(surface_parms.modelAsPoisson.value)
            {
                const long num_shots = std::round((max_t - min_t) / 0.0001);
                const double bin_t = surface_parms.binSize.value * 0.00000002 / 3.0; // bin size from meters to seconds
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
            const long k = (static_cast<long>(std::ceil(kernel_size / surface_parms.binSize.value)) & ~0x1) / 2;
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
            const long peak_separation_in_bins = static_cast<long>(std::ceil(surface_parms.minPeakSeparation.value / surface_parms.binSize.value));
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
                (second_peak * surface_parms.highestPeakRatio.value >= highest_peak) ) // second peak is close in size to highest peak
            {
                /* select peak that is highest in elevation */
                if(highest_peak_bin < second_peak_bin)
                {
                    highest_peak = second_peak;
                    highest_peak_bin = second_peak_bin;
                }
            }

            /* check if sea surface signal is significant */
            const double signal_threshold = bckgnd + (stddev * surface_parms.signalThreshold.value);
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
            const double peak_stddev = (peak_width * surface_parms.binSize.value) / 2.35;

            /* calculate sea surface height and label sea surface photons */
            const float cur_surface_h = min_h + (highest_peak_bin * surface_parms.binSize.value) + (surface_parms.binSize.value / 2.0);
            const double min_surface_h = cur_surface_h - (peak_stddev * surface_parms.surfaceWidth.value);
            const double max_surface_h = cur_surface_h + (peak_stddev * surface_parms.surfaceWidth.value);
            for(long i = p0; i < p1; i++)
            {
                surface_h[i] = cur_surface_h;
                if( ortho_h[i] >= min_surface_h &&
                    ortho_h[i] <= max_surface_h )
                {
                    class_ph[i] = BathyFields::SEA_SURFACE;
                }
            }
        }
        catch(const RunTimeException& e)
        {
            mlog(e.level(), "Failed to find sea surface for beam %s at photon %ld: %s", beam.value.c_str(), p0, e.what());
            for(long i = p0; i < p1; i++)
            {
                processing_flags[i] = processing_flags[i] | BathyFields::SEA_SURFACE_UNDETECTED;
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * luaIsValid
 *----------------------------------------------------------------------------*/
int BathyDataFrame::luaIsValid (lua_State* L)
{
    bool status = false;
    try
    {
        BathyDataFrame* lua_obj = dynamic_cast<BathyDataFrame*>(getLuaSelf(L, 1));
        status = lua_obj->valid;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting status of %s: %s", OBJECT_TYPE, e.what());
    }

    return returnLuaStatus(L, status);
}
