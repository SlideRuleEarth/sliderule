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
#include "H5Object.h"
#include "BathyDataFrame.h"
#include "GeoLib.h"
#include "BathyFields.h"
#include "BathyMask.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyDataFrame::LUA_META_NAME = "BathyDataFrame";
const struct luaL_Reg BathyDataFrame::LUA_META_TABLE[] = {
    {"length",      luaLength},
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
    BathyMask* _mask = NULL;
    H5Object* _hdf03 = NULL;
    H5Object* _hdf09 = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 2, BathyFields::OBJECT_TYPE));
        _mask = dynamic_cast<BathyMask*>(getLuaObject(L, 3, GeoLib::TIFFImage::OBJECT_TYPE, true, NULL));
        _hdf03 = dynamic_cast<H5Object*>(getLuaObject(L, 4, H5Object::OBJECT_TYPE));
        _hdf09 = dynamic_cast<H5Object*>(getLuaObject(L, 5, H5Object::OBJECT_TYPE, true, NULL));
        const char* rqstq_name = getLuaString(L, 6, true, NULL);

        /* Check for Null Resource and Asset */
        if(_parms->resource.value.empty()) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply a resource to process");
        else if(_parms->asset.asset == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply a valid asset");

        /* Return Reader Object */
        return createLuaObject(L, new BathyDataFrame(L, beam_str, _parms, _hdf03, _hdf09, rqstq_name, _mask));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_mask) _mask->releaseLuaObject();
        if(_hdf03) _hdf03->releaseLuaObject();
        if(_hdf09) _hdf09->releaseLuaObject();
        mlog(e.level(), "Error creating BathyDataFrame: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyDataFrame::BathyDataFrame (lua_State* L, const char* beam_str, BathyFields* _parms, H5Object* _hdf03, H5Object* _hdf09, const char* rqstq_name, BathyMask* _mask):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",             &time_ns},
        {"index_ph",            &index_ph},
        {"index_seg",           &index_seg},
        {"lat_ph",              &lat_ph},
        {"lon_ph",              &lon_ph},
        {"x_ph",                &x_ph},
        {"y_ph",                &y_ph},
        {"x_atc",               &x_atc},
        {"y_atc",               &y_atc},
        {"surface_h",           &surface_h},
        {"ortho_h",             &ortho_h},
        {"ellipse_h",           &ellipse_h},
        {"sigma_thu",           &sigma_thu},
        {"sigma_tvu",           &sigma_tvu},
        {"processing_flags",    &processing_flags},
        {"max_signal_conf",     &max_signal_conf},
        {"quality_ph",          &quality_ph},
        {"class_ph",            &class_ph},
        {"predictions",         &predictions},
        {"geoid_corr_h",        &geoid_corr_h},
        // temporary columns for python code
        {"refracted_dZ",        &refracted_dZ},
        {"refracted_lat",       &refracted_lat},
        {"refracted_lon",       &refracted_lon},
        {"subaqueous_sigma_thu", &subaqueous_sigma_thu},
        {"subaqueous_sigma_tvu", &subaqueous_sigma_tvu}
    },
    {
        {"spot",                &spot},
        {"beam",                &beam},
        {"track",               &track},
        {"pair",                &pair},
        {"utm_zone",            &utm_zone},
        {"utm_is_north",        &utm_is_north},
    }),
    beam(beam_str),
    parmsPtr(_parms),
    parms(*_parms),
    bathyMask(_mask),
    hdf03(_hdf03),
    hdf09(_hdf09),
    rqstQ(NULL),
    readTimeoutMs(parms.readTimeout.value * 1000)
{
    /* Create Request Queue Publisher (if supplied) */
    if(rqstq_name) rqstQ = new Publisher(rqstq_name);

    /* Set MetaData Sent as Columns */
    spot.setEncodingFlags(META_COLUMN);
    utm_zone.setEncodingFlags(META_COLUMN);

    /* Call Parent Class Initialization of GeoColumns */
    populateGeoColumns();

    try
    {
        /* Set Signal Confidence Index */
        if(parms.surfaceType == Icesat2Fields::SRT_DYNAMIC)
        {
            signalConfColIndex = H5Coro::ALL_COLS;
        }
        else
        {
            signalConfColIndex = static_cast<int>(parms.surfaceType.value);
        }

        /* Set Track and Pair ( gt<track><pair> - e.g. gt1l )*/
        track = static_cast<int>(beam.value[2]) - 0x30;
        pair = beam.value[3] == 'l' ? Icesat2Fields::RPT_L : Icesat2Fields::RPT_R;

        /* Set Thread Specific Trace ID for H5Coro */
        EventLib::stashId (traceId);

        /* Create Reader */
        active = true;
        pid = new Thread(subsettingThread, this);
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, rqstQ, &active, "Failure on resource %s: %s", parms.resource.value.c_str(), e.what());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, rqstQ, &active, "Failure on resource %s: %s", parms.resource.value.c_str(), e.what());

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

    delete rqstQ;

    if(hdf03) hdf03->releaseLuaObject();
    if(hdf09) hdf09->releaseLuaObject();

    if(parmsPtr) parmsPtr->releaseLuaObject();
    if(bathyMask) bathyMask->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
BathyDataFrame::Region::Region (const BathyDataFrame& dataframe):
    segment_lat    (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/reference_photon_lat").c_str()),
    segment_lon    (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/segment_ph_cnt").c_str()),
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
        if(dataframe.parms.regionMask.valid())
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
    sc_orient           (dataframe.hdf03,                                                "/orbit_info/sc_orient"),
    velocity_sc         (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/velocity_sc").c_str(),     H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_delta_time  (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/delta_time").c_str(),      0, region.first_segment, region.num_segments),
    segment_dist_x      (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/segment_dist_x").c_str(),  0, region.first_segment, region.num_segments),
    solar_elevation     (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/solar_elevation").c_str(), 0, region.first_segment, region.num_segments),
    sigma_h             (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/sigma_h").c_str(),         0, region.first_segment, region.num_segments),
    sigma_along         (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/sigma_along").c_str(),     0, region.first_segment, region.num_segments),
    sigma_across        (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/sigma_across").c_str(),    0, region.first_segment, region.num_segments),
    ref_azimuth         (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/ref_azimuth").c_str(),     0, region.first_segment, region.num_segments),
    ref_elev            (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geolocation/ref_elev").c_str(),        0, region.first_segment, region.num_segments),
    geoid               (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geophys_corr/geoid").c_str(),          0, region.first_segment, region.num_segments),
    dem_h               (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "geophys_corr/dem_h").c_str(),          0, region.first_segment, region.num_segments),
    dist_ph_along       (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/dist_ph_along").c_str(),       0, region.first_photon,  region.num_photons),
    dist_ph_across      (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/dist_ph_across").c_str(),      0, region.first_photon,  region.num_photons),
    h_ph                (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/h_ph").c_str(),                0, region.first_photon,  region.num_photons),
    signal_conf_ph      (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/signal_conf_ph").c_str(),      dataframe.signalConfColIndex, region.first_photon,  region.num_photons),
    quality_ph          (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/quality_ph").c_str(),          0, region.first_photon,  region.num_photons),
    weight_ph           (dataframe.parms.version.value >= 6 ? dataframe.hdf03 : NULL, FString("%s/%s", dataframe.beam.value.c_str(), "heights/weight_ph").c_str(), 0, region.first_photon,  region.num_photons),
    lat_ph              (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/lat_ph").c_str(),              0, region.first_photon,  region.num_photons),
    lon_ph              (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/lon_ph").c_str(),              0, region.first_photon,  region.num_photons),
    delta_time          (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "heights/delta_time").c_str(),          0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "bckgrd_atlas/delta_time").c_str()),
    bckgrd_rate         (dataframe.hdf03, FString("%s/%s", dataframe.beam.value.c_str(), "bckgrd_atlas/bckgrd_rate").c_str())
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
    if(dataframe.parms.version.value >= 6) weight_ph.join(dataframe.readTimeoutMs, true);
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
    met_u10m    (dataframe.hdf09, FString("profile_%d/low_rate/met_u10m", dataframe.track.value).c_str()),
    met_v10m    (dataframe.hdf09, FString("profile_%d/low_rate/met_v10m", dataframe.track.value).c_str()),
    delta_time  (dataframe.hdf09, FString("profile_%d/low_rate/delta_time", dataframe.track.value).c_str())
{
    try
    {
        if(!dataframe.hdf09) throw RunTimeException(CRITICAL, RTE_ERROR, "invalid HDF5 ATL09 object");
        met_u10m.join(dataframe.readTimeoutMs, true);
        met_v10m.join(dataframe.readTimeoutMs, true);
        delta_time.join(dataframe.readTimeoutMs, true);
        valid = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "ATL09 data unavailable for <%s>: %s", dataframe.parms.resource.value.c_str(), e.what());
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
    const uint32_t trace_id = start_trace(INFO, dataframe.traceId, "bathy_subsetter", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", parms.asset.getName(), parms.getResource(), dataframe.track.value);
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
        float wind_v = BathyFields::DEFAULT_WIND_SPEED; // segment level wind speed
        bool on_boundary = true; // true when a spatial subsetting boundary is encountered

        /* Set Spot*/
        dataframe.spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl03.sc_orient[0], (Icesat2Fields::track_t)dataframe.track.value, dataframe.pair.value);

        /* Get UTM Transformation and Set UTM Zone */
        GeoLib::UTMTransform utm_transform(region.segment_lat[0], region.segment_lon[0]);
        dataframe.utm_zone = utm_transform.zone;
        dataframe.utm_is_north = region.segment_lat[0] >= 0.0;

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
                mlog(ERROR, "Photons with no segments are detected in %s/%s (%d %ld %ld)!", parms.resource.value.c_str(), dataframe.beam.value.c_str(), current_segment, atl03.segment_dist_x.size, region.num_segments);
                break;
            }

            do
            {
                /* Check Global Bathymetry Mask */
                if(dataframe.bathyMask)
                {
                    if(dataframe.bathyMask->includes(region.segment_lon[current_segment], region.segment_lat[current_segment]))
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
                if(parms.version.value >= 6)
                {
                    yapc_score = atl03.weight_ph[current_photon];
                    if(yapc_score < parms.yapc.score.value)
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

                /* Calculate Geoid Corrected Height and Check Delta */
                const float geoid_corr_h = atl03.h_ph[current_photon] - atl03.geoid[current_segment];
                if(geoid_corr_h > parms.maxGeoidDelta.value || geoid_corr_h < parms.minGeoidDelta.value)
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

                /* Set Initial Processing Flags */
                uint32_t processing_flags = BathyFields::FLAGS_CLEAR;
                processing_flags |= static_cast<uint32_t>(yapc_score) << 24;
                if(on_boundary) processing_flags |= BathyFields::ON_BOUNDARY;
                if(atl03.solar_elevation[current_segment] < BathyFields::NIGHT_SOLAR_ELEVATION_THRESHOLD) processing_flags |= BathyFields::NIGHT_FLAG;
                if(!atl09.valid) processing_flags |= BathyFields::INVALID_WIND_SPEED;

                /* Add Photon to DataFrame */
                dataframe.addRow(); // start new row in dataframe
                dataframe.time_ns.append(Icesat2Fields::deltatime2timestamp(current_delta_time));
                dataframe.index_ph.append(static_cast<int32_t>(region.first_photon) + current_photon);
                dataframe.index_seg.append(static_cast<int32_t>(region.first_segment) + current_segment);
                dataframe.lat_ph.append(latitude);  // corrected by refraction correction
                dataframe.lon_ph.append(longitude);  // corrected by refraction correction
                dataframe.x_ph.append(coord.x);
                dataframe.y_ph.append(coord.y);
                dataframe.x_atc.append(atl03.segment_dist_x[current_segment] + atl03.dist_ph_along[current_photon]);
                dataframe.y_atc.append(atl03.dist_ph_across[current_photon]);
                dataframe.ellipse_h.append(atl03.h_ph[current_photon]); // later corrected by refraction correction
                dataframe.ortho_h.append(atl03.h_ph[current_photon] - atl03.geoid[current_segment]); // later corrected by refraction correction
                dataframe.max_signal_conf.append(atl03_cnf);
                dataframe.quality_ph.append(quality_ph);
                dataframe.processing_flags.append(processing_flags);

                /* Add Additional Photon Data to DataFrame */
                dataframe.background_rate.append(calculateBackground(current_segment, bckgrd_index, atl03));
                dataframe.geoid_corr_h.append(atl03.h_ph[current_photon] - atl03.geoid[current_segment]);
                dataframe.wind_v.append(wind_v);
                dataframe.ref_el.append(atl03.ref_elev[current_segment]);
                dataframe.ref_az.append(atl03.ref_azimuth[current_segment]);
                dataframe.sigma_across.append(atl03.sigma_across[current_segment]);
                dataframe.sigma_along.append(atl03.sigma_along[current_segment]);
                dataframe.sigma_h.append(atl03.sigma_h[current_segment]);

                /* Reset Boundary Termination */
                on_boundary = false;

            } while(false);

            /* Go to Next Photon */
            current_photon++;
        }

        /* Initialize Additional DataFrame Columns (populated later) */
        dataframe.class_ph.initialize(dataframe.length(), static_cast<uint8_t>(BathyFields::UNCLASSIFIED));
        dataframe.surface_h.initialize(dataframe.length(), 0.0); // populated by sea surface finder
        dataframe.sigma_thu.initialize(dataframe.length(), 0.0); // populated by uncertainty calculation
        dataframe.sigma_tvu.initialize(dataframe.length(), 0.0); // populated by uncertainty calculation
        dataframe.predictions.initialize(dataframe.length(), {0, 0, 0, 0, 0, 0, 0, 0, 0});

        /* Initialize Temporary Columns to Support Python Code */
        dataframe.refracted_dZ.initialize(dataframe.length(), 0.0);
        dataframe.refracted_lat.initialize(dataframe.length(), 0.0);
        dataframe.refracted_lon.initialize(dataframe.length(), 0.0);
        dataframe.subaqueous_sigma_thu.initialize(dataframe.length(), 0.0);
        dataframe.subaqueous_sigma_tvu.initialize(dataframe.length(), 0.0);

    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), dataframe.rqstQ, &dataframe.active, "Failure on resource %s track %d.%d: %s", parms.resource.value.c_str(), dataframe.track.value, dataframe.pair.value, e.what());
        dataframe.inError = true;
    }

    /* Mark Completion */
    mlog(INFO, "Completed processing spot %d for resource %s (%ld rows)", dataframe.spot.value, parms.resource.value.c_str(), dataframe.length());
    dataframe.signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * calculateBackground
 *----------------------------------------------------------------------------*/
float BathyDataFrame::calculateBackground (int32_t current_segment, int32_t& bckgrd_index, const Atl03Data& atl03)
{
    float background_rate = atl03.bckgrd_rate[atl03.bckgrd_rate.size - 1];
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
 * luaLength
 *----------------------------------------------------------------------------*/
int BathyDataFrame::luaLength (lua_State* L)
{
    try
    {
        const BathyDataFrame* lua_obj = dynamic_cast<BathyDataFrame*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->length());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting length of %s: %s", OBJECT_TYPE, e.what());
        lua_pushinteger(L, 0);
    }

    return 1;
}
