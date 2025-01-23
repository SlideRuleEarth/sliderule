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
#include "Atl03DataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03DataFrame::OBJECT_TYPE = "Atl03DataFrame";
const char* Atl03DataFrame::LUA_META_NAME = "Atl03DataFrame";
const struct luaL_Reg Atl03DataFrame::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int Atl03DataFrame::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 1, Icesat2Fields::OBJECT_TYPE));

        /* Check for Null Resource and Asset */
        if(_parms->resource.value.empty()) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply a resource to process");
        else if(_parms->asset.asset == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply a valid asset");

        /* Return Reader Object */
        return createLuaObject(L, new Atl03DataFrame(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating Atl03DataFrame: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl03DataFrame (lua_State* L, Icesat2Fields* _parms):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",             &time_ns},
        {"latitude",            &latitude},
        {"longitude",           &longitude},
        {"x_atc",               &x_atc},
        {"y_atc",               &y_atc},
        {"height",              &height},
        {"relief",              &relief},
        {"landcover",           &landcover},
        {"snowcover",           &snowcover},
        {"atl08_class",         &atl08_class},
        {"atl03_cnf",           &atl03_cnf},
        {"quality_ph",          &quality_ph},
        {"yapc_score",          &yapc_score},
        {"segment_id",          &segment_id},
        {"solar_elevation",     &solar_elevation},
        {"spacecraft_velocity", &spacecraft_velocity},
        {"background_rate",     &background_rate}
    },
    {
        {"spot",                    &spot},
        {"track",                   &track},
        {"pair",                    &pair},
        {"cycle",                   &cycle},
        {"spacecraft_orientation",  &spacecraft_orientation},
        {"reference_ground_track",  &reference_ground_track}
    }),
    read_timeout_ms(_parms->readTimeout.value * 1000),
    parms(_parms),
    context(NULL),
    context08(NULL)
{
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
                        info->dataframe = this;
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
Atl03DataFrame::~Atl03DataFrame (void)
{
    active = false;

    for(int pid = 0; pid < threadCount; pid++)
    {
        delete readerPid[pid];
    }

    delete context;
    delete context08;

    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Region::Region (const info_t* info):
    segment_lat    (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lat").c_str()),
    segment_lon    (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/segment_ph_cnt").c_str()),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        segment_lat.join(info->dataframe->read_timeout_ms, true);
        segment_lon.join(info->dataframe->read_timeout_ms, true);
        segment_ph_cnt.join(info->dataframe->read_timeout_ms, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(info->dataframe->parms->regionMask.valid())
        {
            rasterregion(info);
        }
        else if(info->dataframe->parms->pointsInPolygon.value > 0)
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
Atl03DataFrame::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::Region::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::Region::polyregion (const info_t* info)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        /* Test Inclusion */
        const bool inclusion = info->dataframe->parms->polyIncludes(segment_lon[segment], segment_lat[segment]);

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
void Atl03DataFrame::Region::rasterregion (const info_t* info)
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
            const bool inclusion = info->dataframe->parms->maskIncludes(segment_lon[segment], segment_lat[segment]);
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
Atl03DataFrame::Atl03Data::Atl03Data (const info_t* info, const Region& region):
    read_yapc           (info->dataframe->parms->stages[Icesat2Fields::STAGE_YAPC] && (info->dataframe->parms->yapc.version == 0) && (info->dataframe->parms->version.value >= 6)),
    sc_orient           (info->dataframe->context,                                "/orbit_info/sc_orient"),
    velocity_sc         (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/velocity_sc").c_str(),      H5Coro::ALL_COLS, region.first_segment, region.num_segments),
    segment_delta_time  (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/delta_time").c_str(),       0, region.first_segment, region.num_segments),
    segment_id          (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/segment_id").c_str(),       0, region.first_segment, region.num_segments),
    segment_dist_x      (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/segment_dist_x").c_str(),   0, region.first_segment, region.num_segments),
    solar_elevation     (info->dataframe->context, FString("%s/%s", info->prefix, "geolocation/solar_elevation").c_str(),  0, region.first_segment, region.num_segments),
    dist_ph_along       (info->dataframe->context, FString("%s/%s", info->prefix, "heights/dist_ph_along").c_str(),        0, region.first_photon,  region.num_photons),
    dist_ph_across      (info->dataframe->context, FString("%s/%s", info->prefix, "heights/dist_ph_across").c_str(),       0, region.first_photon,  region.num_photons),
    h_ph                (info->dataframe->context, FString("%s/%s", info->prefix, "heights/h_ph").c_str(),                 0, region.first_photon,  region.num_photons),
    signal_conf_ph      (info->dataframe->context, FString("%s/%s", info->prefix, "heights/signal_conf_ph").c_str(),       info->dataframe->signalConfColIndex, region.first_photon,  region.num_photons),
    quality_ph          (info->dataframe->context, FString("%s/%s", info->prefix, "heights/quality_ph").c_str(),           0, region.first_photon,  region.num_photons),
    weight_ph           (read_yapc ? info->dataframe->context : NULL, FString("%s/%s", info->prefix, "heights/weight_ph").c_str(), 0, region.first_photon,  region.num_photons),
    lat_ph              (info->dataframe->context, FString("%s/%s", info->prefix, "heights/lat_ph").c_str(),               0, region.first_photon,  region.num_photons),
    lon_ph              (info->dataframe->context, FString("%s/%s", info->prefix, "heights/lon_ph").c_str(),               0, region.first_photon,  region.num_photons),
    delta_time          (info->dataframe->context, FString("%s/%s", info->prefix, "heights/delta_time").c_str(),           0, region.first_photon,  region.num_photons),
    bckgrd_delta_time   (info->dataframe->context, FString("%s/%s", info->prefix, "bckgrd_atlas/delta_time").c_str()),
    bckgrd_rate         (info->dataframe->context, FString("%s/%s", info->prefix, "bckgrd_atlas/bckgrd_rate").c_str()),
    anc_geo_data        (info->dataframe->parms->atl03GeoFields,  info->dataframe->context, FString("%s/%s", info->prefix, "geolocation").c_str(),  0, region.first_segment, region.num_segments),
    anc_corr_data       (info->dataframe->parms->atl03CorrFields, info->dataframe->context, FString("%s/%s", info->prefix, "geophys_corr").c_str(), 0, region.first_segment, region.num_segments),
    anc_ph_data         (info->dataframe->parms->atl03PhFields,   info->dataframe->context, FString("%s/%s", info->prefix, "heights").c_str(),      0, region.first_photon,  region.num_photons)
{
    Atl03DataFrame* dataframe = info->dataframe;

    /* Join Hardcoded Reads */
    sc_orient.join(dataframe->read_timeout_ms, true);
    velocity_sc.join(dataframe->read_timeout_ms, true);
    segment_delta_time.join(dataframe->read_timeout_ms, true);
    segment_id.join(dataframe->read_timeout_ms, true);
    segment_dist_x.join(dataframe->read_timeout_ms, true);
    solar_elevation.join(dataframe->read_timeout_ms, true);
    dist_ph_along.join(dataframe->read_timeout_ms, true);
    dist_ph_across.join(dataframe->read_timeout_ms, true);
    h_ph.join(dataframe->read_timeout_ms, true);
    signal_conf_ph.join(dataframe->read_timeout_ms, true);
    quality_ph.join(dataframe->read_timeout_ms, true);
    if(read_yapc) weight_ph.join(dataframe->read_timeout_ms, true);
    lat_ph.join(dataframe->read_timeout_ms, true);
    lon_ph.join(dataframe->read_timeout_ms, true);
    delta_time.join(dataframe->read_timeout_ms, true);
    bckgrd_delta_time.join(dataframe->read_timeout_ms, true);
    bckgrd_rate.join(dataframe->read_timeout_ms, true);

    /* Join and Add Ancillary Columns */
    anc_geo_data.joinToGDF(dataframe, dataframe->read_timeout_ms, true);
    anc_corr_data.joinToGDF(dataframe, dataframe->read_timeout_ms, true);
    anc_ph_data.joinToGDF(dataframe, dataframe->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * Atl08Class::Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl08Class::Atl08Class (const info_t* info):
    enabled             (info->dataframe->parms->stages[Icesat2Fields::STAGE_ATL08]),
    phoreal             (info->dataframe->parms->stages[Icesat2Fields::STAGE_PHOREAL]),
    ancillary           (info->dataframe->parms->atl08Fields.length() > 0),
    classification      {NULL},
    relief              {NULL},
    landcover           {NULL},
    snowcover           {NULL},
    atl08_segment_id    (enabled ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/ph_segment_id").c_str()),
    atl08_pc_indx       (enabled ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/classed_pc_indx").c_str()),
    atl08_pc_flag       (enabled ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/classed_pc_flag").c_str()),
    atl08_ph_h          (phoreal ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "signal_photons/ph_h").c_str()),
    segment_id_beg      ((phoreal || ancillary) ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "land_segments/segment_id_beg").c_str()),
    segment_landcover   (phoreal ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "land_segments/segment_landcover").c_str()),
    segment_snowcover   (phoreal ? info->dataframe->context08 : NULL, FString("%s/%s", info->prefix, "land_segments/segment_snowcover").c_str()),
    anc_seg_data        (info->dataframe->parms->atl08Fields, info->dataframe->context08, FString("%s/land_segments", info->prefix).c_str()),
    anc_seg_indices     (NULL)
{
    anc_seg_data.joinToGDF(info->dataframe, info->dataframe->read_timeout_ms, true);
}

/*----------------------------------------------------------------------------
 * Atl08Class::Destructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl08Class::~Atl08Class (void)
{
    delete [] classification;
    delete [] relief;
    delete [] landcover;
    delete [] snowcover;
    delete [] anc_seg_indices;
}

/*----------------------------------------------------------------------------
 * Atl08Class::classify
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::Atl08Class::classify (const info_t* info, const Region& region, const Atl03Data& atl03)
{
    /* Do Nothing If Not Enabled */
    if(!enabled)
    {
        return;
    }

    /* Wait for Reads to Complete */
    atl08_segment_id.join(info->dataframe->read_timeout_ms, true);
    atl08_pc_indx.join(info->dataframe->read_timeout_ms, true);
    atl08_pc_flag.join(info->dataframe->read_timeout_ms, true);
    if(phoreal || ancillary)
    {
        segment_id_beg.join(info->dataframe->read_timeout_ms, true);
    }
    if(phoreal)
    {
        atl08_ph_h.join(info->dataframe->read_timeout_ms, true);
        segment_landcover.join(info->dataframe->read_timeout_ms, true);
        segment_snowcover.join(info->dataframe->read_timeout_ms, true);
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
                    if(info->dataframe->parms->phoreal.above_classifier && (classification[atl03_photon] != Icesat2Fields::ATL08_TOP_OF_CANOPY))
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
                    anc_seg_indices[atl03_photon] = Atl03DataFrame::INVALID_INDICE;
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
uint8_t Atl03DataFrame::Atl08Class::operator[] (int index) const
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
Atl03DataFrame::YapcScore::YapcScore (const info_t* info, const Region& region, const Atl03Data& atl03):
    enabled {info->dataframe->parms->stages[Icesat2Fields::STAGE_YAPC]},
    score {NULL}
{
    /* Do Nothing If Not Enabled */
    if(!enabled)
    {
        return;
    }

    /* Run YAPC */
    if(info->dataframe->parms->yapc.version == 3)
    {
        yapcV3(info, region, atl03);
    }
    else if(info->dataframe->parms->yapc.version == 2 || info->dataframe->parms->yapc.version == 1)
    {
        yapcV2(info, region, atl03);
    }
    else if(info->dataframe->parms->yapc.version != 0) // read from file
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid YAPC version specified: %d", info->dataframe->parms->yapc.version.value);
    }
}

/*----------------------------------------------------------------------------
 * YapcScore::Destructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::YapcScore::~YapcScore (void)
{
    delete [] score;
}

/*----------------------------------------------------------------------------
 * yapcV2
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::YapcScore::yapcV2 (const info_t* info, const Region& region, const Atl03Data& atl03)
{
    /* YAPC Hard-Coded Parameters */
    const double MAXIMUM_HSPREAD = 15000.0; // meters
    const double HSPREAD_BINSIZE = 1.0; // meters
    const int MAX_KNN = 25;
    double nearest_neighbors[MAX_KNN];

    /* Shortcut to Settings */
    const YapcFields& settings = info->dataframe->parms->yapc;

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
        if((N <= knn) || (N < info->dataframe->parms->minPhotonCount.value)) continue;

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
void Atl03DataFrame::YapcScore::yapcV3 (const info_t* info, const Region& region, const Atl03Data& atl03)
{
    /* YAPC Parameters */
    const YapcFields& settings = info->dataframe->parms->yapc;
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
uint8_t Atl03DataFrame::YapcScore::operator[] (int index) const
{
    return score[index];
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl03DataFrame::subsettingThread (void* parm)
{
    /* Get Thread Info */
    const info_t* info = static_cast<info_t*>(parm);
    Atl03DataFrame& dataframe = *info->dataframe;
    const Icesat2Fields& parms = *dataframe.parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, dataframe.traceId, "atl03_subsetter", "{\"context\":\"%s\", \"track\":%d}", dataframe.context->name, info->track);
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

        /* Calculate Spot Number */
        const uint8_t spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl03.sc_orient[0], (Icesat2Fields::track_t)info->track, info->pair);

        /* Set MetaData */
        dataframe.spacecraft_orientation = atl03.sc_orient[0];
        dataframe.reference_ground_track  = parms.rgt.value;

        /* Initialize Indices */
        int32_t current_photon = -1;
        int32_t current_segment = 0;
        int32_t current_count = 0; // number of photons in current segment already accounted for
        int32_t background_index = 0;

        /* Traverse All Photons In Dataset */
        while(dataframe.active && (current_photon < atl03.dist_ph_along.size))
        {
            /* Go to Next Photon */
            current_photon++;

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
                throw RunTimeException(ERROR, RTE_ERROR, "Photons with no segments are detected is %s/%d (%d %ld %ld)!", info->dataframe->context->name, info->track, current_segment, atl03.segment_dist_x.size, region.num_segments);
            }

            /* Check Region Mask */
            if(region.inclusion_ptr)
            {
                if(!region.inclusion_ptr[current_segment])
                {
                    continue;
                }
            }

            /* Set Signal Confidence Level */
            int8_t atl03_cnf;
            if(parms.surfaceType == Icesat2Fields::SRT_DYNAMIC)
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
            else if(!parms.atl03Cnf[static_cast<Icesat2Fields::signal_conf_t>(atl03_cnf)])
            {
                continue;
            }

            /* Set and Check ATL03 Photon Quality Level */
            const Icesat2Fields::quality_ph_t quality_ph = static_cast<Icesat2Fields::quality_ph_t>(atl03.quality_ph[current_photon]);
            if(quality_ph < Icesat2Fields::QUALITY_NOMINAL || quality_ph > Icesat2Fields::QUALITY_POSSIBLE_TEP)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl03 photon quality: %d", quality_ph);
            }
            else if(!parms.qualityPh[quality_ph])
            {
                continue;
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
                else if(!parms.atl08Class[atl08_class])
                {
                    continue;
                }
            }

            /* Set and Check YAPC Score */
            uint8_t yapc_score = 0;
            if(yapc.score) // dynamically calculated
            {
                yapc_score = yapc[current_photon];
                if(yapc_score < parms.yapc.score)
                {
                    continue;
                }
            }
            else if(atl03.read_yapc) // read from atl03 granule
            {
                yapc_score = atl03.weight_ph[current_photon];
                if(yapc_score < parms.yapc.score)
                {
                    continue;
                }
            }

            /* Set PhoREAL Fields */
            float relief = 0.0;
            uint8_t landcover_flag = Atl08Class::INVALID_FLAG;
            uint8_t snowcover_flag = Atl08Class::INVALID_FLAG;
            if(atl08.phoreal)
            {
                /* Set Relief */
                if(!parms.phoreal.use_abs_h)
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

            /* Calculate Spacecraft Velocity */
            const int32_t sc_v_offset = current_segment * 3;
            const double sc_v1 = atl03.velocity_sc[sc_v_offset + 0];
            const double sc_v2 = atl03.velocity_sc[sc_v_offset + 1];
            const double sc_v3 = atl03.velocity_sc[sc_v_offset + 2];
            const float spacecraft_velocity = static_cast<float>(sqrt((sc_v1*sc_v1) + (sc_v2*sc_v2) + (sc_v3*sc_v3)));

            /* Calculate Background Rate */
            double background_rate = atl03.bckgrd_rate[atl03.bckgrd_rate.size - 1];
            while(background_index < atl03.bckgrd_rate.size)
            {
                const double curr_bckgrd_time = atl03.bckgrd_delta_time[background_index];
                const double segment_time = atl03.segment_delta_time[current_segment];
                if(curr_bckgrd_time >= segment_time)
                {
                    /* Interpolate Background Rate */
                    if(background_index > 0)
                    {
                        const double prev_bckgrd_time = atl03.bckgrd_delta_time[background_index - 1];
                        const double prev_bckgrd_rate = atl03.bckgrd_rate[background_index - 1];
                        const double curr_bckgrd_rate = atl03.bckgrd_rate[background_index];

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
                background_index++;
            }

            /* Add Photon to DataFrame */
            dataframe.addRow();
            dataframe.time_ns.append(Icesat2Fields::deltatime2timestamp(atl03.delta_time[current_photon]));
            dataframe.latitude.append(atl03.lat_ph[current_photon]);
            dataframe.longitude.append(atl03.lon_ph[current_photon]);
            dataframe.x_atc.append(atl03.dist_ph_along[current_photon] + atl03.segment_dist_x[current_segment]);
            dataframe.y_atc.append(atl03.dist_ph_across[current_photon]);
            dataframe.height.append(atl03.h_ph[current_photon]);
            dataframe.relief.append(relief);
            dataframe.solar_elevation.append(atl03.solar_elevation[current_segment]);
            dataframe.background_rate.append(background_rate);
            dataframe.landcover.append(landcover_flag);
            dataframe.snowcover.append(snowcover_flag);
            dataframe.atl08_class.append(static_cast<uint8_t>(atl08_class));
            dataframe.atl03_cnf.append(atl03_cnf);
            dataframe.quality_ph.append(static_cast<int8_t>(quality_ph));
            dataframe.yapc_score.append(yapc_score);
            dataframe.spacecraft_velocity.append(spacecraft_velocity);
            dataframe.spot.append(spot);
            dataframe.segment_id.append(atl03.segment_id[current_segment]);

            /* Add Ancillary Elements */
            atl03.anc_geo_data.addToGDF(&dataframe, current_segment);
            atl03.anc_corr_data.addToGDF(&dataframe, current_segment);
            atl03.anc_ph_data.addToGDF(&dataframe, current_photon);
            atl08.anc_seg_data.addToGDF(&dataframe, atl08.anc_seg_indices[current_photon]);
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), NULL, &dataframe.active, "Failure on resource %s track %d.%d: %s", dataframe.context->name, info->track, info->pair, e.what());
    }

    /* Handle Global Reader Updates */
    dataframe.threadMut.lock();
    {
        /* Count Completion */
        dataframe.numComplete++;
        if(dataframe.numComplete == dataframe.threadCount)
        {
            mlog(INFO, "Completed processing resource %s track %d.%d", dataframe.context->name, info->track, info->pair);
            dataframe.signalComplete();
        }
    }
    dataframe.threadMut.unlock();

    /* Clean Up Info */
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
