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
#include "LuaObject.h"
#include "H5Object.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03DataFrame::LUA_META_NAME = "Atl03DataFrame";
const struct luaL_Reg Atl03DataFrame::LUA_META_TABLE[] = {
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
    H5Object* _hdf03 = NULL;
    H5Object* _hdf08 = NULL;
    H5Object* _hdf24 = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        _hdf03 = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        _hdf08 = dynamic_cast<H5Object*>(getLuaObject(L, 4, H5Object::OBJECT_TYPE, true, NULL));
        _hdf24 = dynamic_cast<H5Object*>(getLuaObject(L, 5, H5Object::OBJECT_TYPE, true, NULL));
        const char* outq_name = getLuaString(L, 6, true, NULL);

        /* Return Reader Object */
        return createLuaObject(L, new Atl03DataFrame(L, beam_str, _parms, _hdf03, _hdf08, _hdf24, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf03) _hdf03->releaseLuaObject();
        if(_hdf08) _hdf08->releaseLuaObject();
        if(_hdf24) _hdf08->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl03DataFrame (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf03, H5Object* _hdf08, H5Object* _hdf24, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",             &time_ns},
        {"latitude",            &latitude},
        {"longitude",           &longitude},
        {"x_atc",               &x_atc},
        {"y_atc",               &y_atc},
        {"height",              &height},
        {"solar_elevation",     &solar_elevation},
        {"background_rate",     &background_rate},
        {"spacecraft_velocity", &spacecraft_velocity},
        {"atl03_cnf",           &atl03_cnf},
        {"quality_ph",          &quality_ph},
        {"ph_index",            &ph_index},
    },
    {
        {"spot",                &spot},
        {"cycle",               &cycle},
        {"region",              &region},
        {"rgt",                 &rgt},
        {"gt",                  &gt},
        {"granule",             &granule}
    }),
    spot(0, META_COLUMN),
    cycle(_parms->granuleFields.cycle.value, META_COLUMN),
    region(_parms->granuleFields.region.value, META_COLUMN),
    rgt(_parms->granuleFields.rgt.value, META_COLUMN),
    gt(0, META_COLUMN),
    granule(_hdf03->name, META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    signalConfColIndex(H5Coro::ALL_COLS),
    beam(FString("%s", beam_str).c_str(true)),
    outQ(NULL),
    parms(_parms),
    hdf03(_hdf03),
    hdf08(_hdf08),
    hdf24(_hdf24)
{
    assert(_parms);
    assert(_hdf03);

    /* Set Optional PhoREAL Columns */
    if(parms->stages[Icesat2Fields::STAGE_PHOREAL])
    {
        addColumn("relief",             &relief,            false);
        addColumn("landcover",          &landcover,         false);
        addColumn("snowcover",          &snowcover,         false);
    }

    /* Set Optional YAPC Columns */
    if(parms->stages[Icesat2Fields::STAGE_YAPC])
    {
        addColumn("yapc_score",         &yapc_score,        false);
    }

    /* Set Optional ATL08 Columns */
    if(parms->stages[Icesat2Fields::STAGE_ATL08])
    {
        addColumn("atl08_class",        &atl08_class,       false);
    }

    /* Set Optional ATL24 Columns */
    if(parms->stages[Icesat2Fields::STAGE_ATL24])
    {
        addColumn("atl24_class",        &atl24_class,       false);
        addColumn("atl24_confidence",   &atl24_confidence,  false);
    }

    /* Call Parent Class Initialization of GeoColumns */
    populateDataframe();

    /* Set Signal Confidence Index */
    if(parms->surfaceType != Icesat2Fields::SRT_DYNAMIC)
    {
        signalConfColIndex = static_cast<int>(parms->surfaceType.value);
    }

    /* Calculate Key */
    dfKey = 0;
    const int exp_beam_str_len = 4;
    for(int i = 0; i < exp_beam_str_len; i++)
    {
        if(beam[i] == '\0') break;
        dfKey += static_cast<int>(beam[i]);
    }

    /* Setup Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Kickoff Reader Thread */
    active = true;
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::~Atl03DataFrame (void)
{
    active = false;
    delete readerPid;
    delete [] beam;
    delete outQ;
    parms->releaseLuaObject();
    hdf03->releaseLuaObject();
    if(hdf08) hdf08->releaseLuaObject();
    if(hdf24) hdf24->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t Atl03DataFrame::getKey(void) const
{
    return dfKey;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::AreaOfInterest::AreaOfInterest (const Atl03DataFrame* df):
    segment_lat    (df->hdf03, FString("/%s/%s", df->beam, "geolocation/reference_photon_lat").c_str()),
    segment_lon    (df->hdf03, FString("/%s/%s", df->beam, "geolocation/reference_photon_lon").c_str()),
    segment_ph_cnt (df->hdf03, FString("/%s/%s", df->beam, "geolocation/segment_ph_cnt").c_str()),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        segment_lat.join(df->readTimeoutMs, true);
        segment_lon.join(df->readTimeoutMs, true);
        segment_ph_cnt.join(df->readTimeoutMs, true);

        /* Initialize AreaOfInterest */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(df->parms->regionMask.valid())
        {
            rasterregion(df);
        }
        else if(df->parms->pointsInPolygon.value > 0)
        {
            polyregion(df);
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
 * AreaOfInterest::Destructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::AreaOfInterest::~AreaOfInterest (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::cleanup
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::AreaOfInterest::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::polyregion
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::AreaOfInterest::polyregion (const Atl03DataFrame* df)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = 0;
    while(segment < segment_ph_cnt.size)
    {
        /* Test Inclusion */
        const bool inclusion = df->parms->polyIncludes(segment_lon[segment], segment_lat[segment]);

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
 * AreaOfInterest::rasterregion
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::AreaOfInterest::rasterregion (const Atl03DataFrame* df)
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
            const bool inclusion = df->parms->maskIncludes(segment_lon[segment], segment_lat[segment]);
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
Atl03DataFrame::Atl03Data::Atl03Data (Atl03DataFrame* df, const AreaOfInterest& aoi):
    read_yapc           (df->parms->stages[Icesat2Fields::STAGE_YAPC] && (df->parms->yapc.version == 0) && (df->parms->granuleFields.version.value >= 6)),
    read_geoid          (df->parms->datum == MathLib::EGM08),
    sc_orient           (df->hdf03,                            "/orbit_info/sc_orient"),
    velocity_sc         (df->hdf03, FString("%s/%s", df->beam, "geolocation/velocity_sc").c_str(),      H5Coro::ALL_COLS, aoi.first_segment, aoi.num_segments),
    segment_delta_time  (df->hdf03, FString("%s/%s", df->beam, "geolocation/delta_time").c_str(),       0, aoi.first_segment, aoi.num_segments),
    segment_id          (df->hdf03, FString("%s/%s", df->beam, "geolocation/segment_id").c_str(),       0, aoi.first_segment, aoi.num_segments),
    segment_dist_x      (df->hdf03, FString("%s/%s", df->beam, "geolocation/segment_dist_x").c_str(),   0, aoi.first_segment, aoi.num_segments),
    solar_elevation     (df->hdf03, FString("%s/%s", df->beam, "geolocation/solar_elevation").c_str(),  0, aoi.first_segment, aoi.num_segments),
    dist_ph_along       (df->hdf03, FString("%s/%s", df->beam, "heights/dist_ph_along").c_str(),        0, aoi.first_photon,  aoi.num_photons),
    dist_ph_across      (df->hdf03, FString("%s/%s", df->beam, "heights/dist_ph_across").c_str(),       0, aoi.first_photon,  aoi.num_photons),
    h_ph                (df->hdf03, FString("%s/%s", df->beam, "heights/h_ph").c_str(),                 0, aoi.first_photon,  aoi.num_photons),
    signal_conf_ph      (df->hdf03, FString("%s/%s", df->beam, "heights/signal_conf_ph").c_str(),       df->signalConfColIndex, aoi.first_photon,  aoi.num_photons),
    quality_ph          (df->hdf03, FString("%s/%s", df->beam, "heights/quality_ph").c_str(),           0, aoi.first_photon,  aoi.num_photons),
    weight_ph           (read_yapc ? df->hdf03 : NULL, FString("%s/%s", df->beam, "heights/weight_ph").c_str(), 0, aoi.first_photon,  aoi.num_photons),
    lat_ph              (df->hdf03, FString("%s/%s", df->beam, "heights/lat_ph").c_str(),               0, aoi.first_photon,  aoi.num_photons),
    lon_ph              (df->hdf03, FString("%s/%s", df->beam, "heights/lon_ph").c_str(),               0, aoi.first_photon,  aoi.num_photons),
    delta_time          (df->hdf03, FString("%s/%s", df->beam, "heights/delta_time").c_str(),           0, aoi.first_photon,  aoi.num_photons),
    bckgrd_delta_time   (df->hdf03, FString("%s/%s", df->beam, "bckgrd_atlas/delta_time").c_str()),
    bckgrd_rate         (df->hdf03, FString("%s/%s", df->beam, "bckgrd_atlas/bckgrd_rate").c_str()),
    geoid               (read_geoid ? df->hdf03 : NULL,         FString("%s/%s", df->beam, "geophys_corr/geoid").c_str(), 0, aoi.first_segment, aoi.num_segments),
    anc_geo_data        (df->parms->atl03GeoFields,  df->hdf03, FString("%s/%s", df->beam, "geolocation").c_str(),        0, aoi.first_segment, aoi.num_segments),
    anc_corr_data       (df->parms->atl03CorrFields, df->hdf03, FString("%s/%s", df->beam, "geophys_corr").c_str(),       0, aoi.first_segment, aoi.num_segments),
    anc_ph_data         (df->parms->atl03PhFields,   df->hdf03, FString("%s/%s", df->beam, "heights").c_str(),            0, aoi.first_photon,  aoi.num_photons)
{
    /* Join Hardcoded Reads */
    sc_orient.join(df->readTimeoutMs, true);
    velocity_sc.join(df->readTimeoutMs, true);
    segment_delta_time.join(df->readTimeoutMs, true);
    segment_id.join(df->readTimeoutMs, true);
    segment_dist_x.join(df->readTimeoutMs, true);
    solar_elevation.join(df->readTimeoutMs, true);
    dist_ph_along.join(df->readTimeoutMs, true);
    dist_ph_across.join(df->readTimeoutMs, true);
    h_ph.join(df->readTimeoutMs, true);
    signal_conf_ph.join(df->readTimeoutMs, true);
    quality_ph.join(df->readTimeoutMs, true);
    if(read_yapc) weight_ph.join(df->readTimeoutMs, true);
    lat_ph.join(df->readTimeoutMs, true);
    lon_ph.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    bckgrd_delta_time.join(df->readTimeoutMs, true);
    bckgrd_rate.join(df->readTimeoutMs, true);
    if(read_geoid) geoid.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_geo_data.joinToGDF(df, df->readTimeoutMs, true);
    anc_corr_data.joinToGDF(df, df->readTimeoutMs, true);
    anc_ph_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * Atl08Class::Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl08Class::Atl08Class (Atl03DataFrame* df):
    enabled             (df->parms->stages[Icesat2Fields::STAGE_ATL08]),
    phoreal             (df->parms->stages[Icesat2Fields::STAGE_PHOREAL]),
    ancillary           (df->parms->atl08Fields.length() > 0),
    classification      {NULL},
    relief              {NULL},
    landcover           {NULL},
    snowcover           {NULL},
    atl08_segment_id    (enabled ? df->hdf08 : NULL, FString("%s/%s", df->beam, "signal_photons/ph_segment_id").c_str()),
    atl08_pc_indx       (enabled ? df->hdf08 : NULL, FString("%s/%s", df->beam, "signal_photons/classed_pc_indx").c_str()),
    atl08_pc_flag       (enabled ? df->hdf08 : NULL, FString("%s/%s", df->beam, "signal_photons/classed_pc_flag").c_str()),
    atl08_ph_h          (phoreal ? df->hdf08 : NULL, FString("%s/%s", df->beam, "signal_photons/ph_h").c_str()),
    segment_id_beg      ((phoreal || ancillary) ? df->hdf08 : NULL, FString("%s/%s", df->beam, "land_segments/segment_id_beg").c_str()),
    segment_landcover   (phoreal ? df->hdf08 : NULL, FString("%s/%s", df->beam, "land_segments/segment_landcover").c_str()),
    segment_snowcover   (phoreal ? df->hdf08 : NULL, FString("%s/%s", df->beam, "land_segments/segment_snowcover").c_str()),
    anc_seg_data        (df->parms->atl08Fields, df->hdf08, FString("%s/land_segments", df->beam).c_str()),
    anc_seg_indices     (NULL)
{
    anc_seg_data.joinToGDF(df, df->readTimeoutMs, true);
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
void Atl03DataFrame::Atl08Class::classify (const Atl03DataFrame* df, const AreaOfInterest& aoi, const Atl03Data& atl03)
{
    /* Do Nothing If Not Enabled */
    if(!enabled)
    {
        return;
    }

    /* Wait for Reads to Complete */
    atl08_segment_id.join(df->readTimeoutMs, true);
    atl08_pc_indx.join(df->readTimeoutMs, true);
    atl08_pc_flag.join(df->readTimeoutMs, true);
    if(phoreal || ancillary)
    {
        segment_id_beg.join(df->readTimeoutMs, true);
    }
    if(phoreal)
    {
        atl08_ph_h.join(df->readTimeoutMs, true);
        segment_landcover.join(df->readTimeoutMs, true);
        segment_snowcover.join(df->readTimeoutMs, true);
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
        const int32_t atl03_segment_count = aoi.segment_ph_cnt[atl03_segment_index];
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
                    if(df->parms->phoreal.above_classifier && (classification[atl03_photon] != Icesat2Fields::ATL08_TOP_OF_CANOPY))
                    {
                        if( (atl03.solar_elevation[atl03_segment_index] <= 5.0) &&
                            ((df->spot.value == 1) || (df->spot.value == 3) || (df->spot.value == 5)) &&
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
uint8_t Atl03DataFrame::Atl08Class::operator[] (int index) const
{
    // This operator is called only after the classification array has been fully populated.
    // clang-tidy is not able to determine that the array is always populated.
    // We disable the related warning to avoid false positives.

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.UndefReturn)
    return classification[index];
}

/*----------------------------------------------------------------------------
 * Atl24Class::Constructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl24Class::Atl24Class (Atl03DataFrame* df):
    enabled             (df->parms->stages[Icesat2Fields::STAGE_ATL24]),
    classification      {NULL},
    confidence          {NULL},
    atl24_index_ph      (enabled ? df->hdf24 : NULL, FString("%s/%s", df->beam, "index_ph").c_str()),
    atl24_class_ph      (enabled ? df->hdf24 : NULL, FString("%s/%s", df->beam, "class_ph").c_str()),
    atl24_confidence    (enabled ? df->hdf24 : NULL, FString("%s/%s", df->beam, "confidence").c_str())
{
}

/*----------------------------------------------------------------------------
 * Atl24Class::Destructor
 *----------------------------------------------------------------------------*/
Atl03DataFrame::Atl24Class::~Atl24Class (void)
{
    delete [] classification;
    delete [] confidence;
}

/*----------------------------------------------------------------------------
 * Atl24Class::classify
 *----------------------------------------------------------------------------*/
void Atl03DataFrame::Atl24Class::classify (const Atl03DataFrame* df, const AreaOfInterest& aoi, const Atl03Data& atl03)
{
    /* Do Nothing If Not Enabled */
    if(!enabled) return;

    /* Wait for Reads to Complete */
    atl24_index_ph.join(df->readTimeoutMs, true);
    atl24_class_ph.join(df->readTimeoutMs, true);
    atl24_confidence.join(df->readTimeoutMs, true);

    /* Allocate ATL08 Classification Array */
    const long num_photons = atl03.dist_ph_along.size;
    classification = new uint8_t [num_photons];
    confidence = new float [num_photons];

    /* Initialize Classification */
    memset(classification, Atl24Fields::UNCLASSIFIED, sizeof(uint8_t) * num_photons);

    /* Go To First Photon in ATL24 Data */
    long i = 0;
    while(atl24_index_ph[i] < aoi.first_photon) i++;

    /* Populate ATL24 Classifications */
    for(long j = i; j < atl24_index_ph.size; j++)
    {
        const long index = atl24_index_ph[j] - aoi.first_photon;
        if(index < num_photons)
        {
            classification[index] = static_cast<uint8_t>(atl24_class_ph[j]);
            confidence[index] = static_cast<float>(atl24_confidence[j]);
        }
        else break;
    }
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl03DataFrame::subsettingThread (void* parm)
{
    /* Get Thread Info */
    Atl03DataFrame* df = static_cast<Atl03DataFrame*>(parm);
    const Icesat2Fields& parms = *df->parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "atl03_subsetter", "{\"context\":\"%s\", \"beam\":%s}", df->hdf03->name, df->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Start Reading ATL08 Data */
        Atl08Class atl08(df);

        /* Start Reading ATL24 Data */
        Atl24Class atl24(df);

        /* Subset to AreaOfInterest of Interest */
        const AreaOfInterest aoi(df);

        /* Read ATL03 Datasets */
        const Atl03Data atl03(df, aoi);

        /* Set MetaData */
        df->spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl03.sc_orient[0], df->beam);
        df->gt = Icesat2Fields::getGroundTrack(df->beam);

        /* Check Spot Filter */
        if(!parms.spots[static_cast<Icesat2Fields::spot_t>(df->spot.value)])
        {
            throw RunTimeException(DEBUG, RTE_STATUS, "spot %d filtered out", df->spot.value);
        }

        /* Perform ATL08 Classification (if requested) */
        atl08.classify(df, aoi, atl03);

        /* Perform ATL24 Classification (if requested) */
        atl24.classify(df, aoi, atl03);

        /* Initialize Indices */
        int32_t current_photon = -1;
        int32_t current_segment = 0;
        int32_t current_count = 0; // number of photons in current segment already accounted for
        int32_t background_index = 0;

        /* Traverse All Photons In Dataset */
        while(df->active && (++current_photon < atl03.dist_ph_along.size))
        {
            /* Go to Photon's Segment */
            current_count++;
            while((current_segment < aoi.segment_ph_cnt.size) &&
                  (current_count > aoi.segment_ph_cnt[current_segment]))
            {
                current_count = 1; // reset photons in segment
                current_segment++; // go to next segment
            }

            /* Check Current Segment */
            if(current_segment >= atl03.segment_dist_x.size)
            {
                throw RunTimeException(ERROR, RTE_FAILURE, "Photons with no segments are detected in %s/%s (%d %ld %ld) (%d %d)", df->hdf03->name, df->beam, current_segment, atl03.segment_dist_x.size, aoi.num_segments, current_photon, current_count);
            }

            /* Check AreaOfInterest Mask */
            if(aoi.inclusion_ptr)
            {
                if(!aoi.inclusion_ptr[current_segment])
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
                atl03_cnf = atl03.signal_conf_ph[conf_index];
                for(int i = 1; i < Icesat2Fields::NUM_SURFACE_TYPES; i++)
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
                throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl03 signal confidence: %d", atl03_cnf);
            }
            else if(!parms.atl03Cnf[static_cast<Icesat2Fields::signal_conf_t>(atl03_cnf)])
            {
                continue;
            }

            /* Set and Check ATL03 Photon Quality Level */
            const Icesat2Fields::quality_ph_t quality_ph = static_cast<Icesat2Fields::quality_ph_t>(atl03.quality_ph[current_photon]);
            if(quality_ph < Icesat2Fields::QUALITY_NOMINAL || quality_ph > Icesat2Fields::QUALITY_POSSIBLE_TEP)
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl03 photon quality: %d", quality_ph);
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
                    throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl08 classification: %d", atl08_class);
                }
                else if(!parms.atl08Class[atl08_class])
                {
                    continue;
                }
            }

            /* Set and Check YAPC Score */
            uint8_t yapc_score = 0;
            if(atl03.read_yapc) // read from atl03 granule
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

            /* Set ATL24 Fields */
            Atl24Fields::class_t atl24_class = Atl24Fields::UNCLASSIFIED;
            float atl24_confidence = 0.0;
            if(atl24.classification)
            {
                atl24_class = static_cast<Atl24Fields::class_t>(atl24.classification[current_photon]);
                if(atl24_class != Atl24Fields::UNCLASSIFIED)
                {
                    atl24_confidence = atl24.confidence[current_photon];
                }

                /* Check ATL24 Class */
                if(!parms.atl24.class_ph[atl24_class])
                {
                    continue;
                }

                /* Check ATL24 Confidence */
                if(parms.atl24.confidence_threshold.value > atl24_confidence)
                {
                    continue;
                }
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

            /* Calculate Height */
            float height = atl03.h_ph[current_photon];
            if(atl03.read_geoid) height -= atl03.geoid[current_segment];

            /* Add Photon to DataFrame */
            df->addRow();
            df->time_ns.append(Icesat2Fields::deltatime2timestamp(atl03.delta_time[current_photon]));
            df->latitude.append(atl03.lat_ph[current_photon]);
            df->longitude.append(atl03.lon_ph[current_photon]);
            df->x_atc.append(atl03.dist_ph_along[current_photon] + atl03.segment_dist_x[current_segment]);
            df->y_atc.append(atl03.dist_ph_across[current_photon]);
            df->height.append(height);
            df->solar_elevation.append(atl03.solar_elevation[current_segment]);
            df->background_rate.append(background_rate);
            df->atl03_cnf.append(atl03_cnf);
            df->quality_ph.append(static_cast<int8_t>(quality_ph));
            df->spacecraft_velocity.append(spacecraft_velocity);
            df->ph_index.append(current_photon + aoi.first_photon);

            /* Add Optional PhoREAL Data */
            if(atl08.phoreal)
            {
                df->relief.append(relief);
                df->landcover.append(landcover_flag);
                df->snowcover.append(snowcover_flag);
            }

            /* Add Optional YAPC Data */
            if(parms.stages[Icesat2Fields::STAGE_YAPC])
            {
                df->yapc_score.append(yapc_score);
            }

            /* Add Optional ATL08 Data */
            if(atl08.classification)
            {
                df->atl08_class.append(static_cast<uint8_t>(atl08_class));
            }

            /* Add Optional ATL24 Data */
            if(atl24.classification)
            {
                df->atl24_class.append(static_cast<uint8_t>(atl24_class));
                df->atl24_confidence.append(atl24_confidence);
            }

            /* Add Ancillary Elements */
            if(atl03.anc_geo_data.length() > 0)     atl03.anc_geo_data.addToGDF(df, current_segment);
            if(atl03.anc_corr_data.length() > 0)    atl03.anc_corr_data.addToGDF(df, current_segment);
            if(atl03.anc_ph_data.length() > 0)      atl03.anc_ph_data.addToGDF(df, current_photon);
            if(atl08.anc_seg_indices)               atl08.anc_seg_data.addToGDF(df, atl08.anc_seg_indices[current_photon]);
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf03->name, df->beam, e.what());
    }

    /* Dataframe Complete */
    mlog(INFO, "Completed processing resource %s beam %s", df->hdf03->name, df->beam);
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
