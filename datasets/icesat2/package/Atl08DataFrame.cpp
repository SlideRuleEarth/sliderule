/*
 * Copyright (c) 2025, University of Washington
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

#include "Atl08DataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl08DataFrame::LUA_META_NAME = "Atl08DataFrame";
const struct luaL_Reg Atl08DataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * ATL08 DATAFRAME CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<beam>, <parms>, <hdf08>, <outq_name>)
 *----------------------------------------------------------------------------*/
int Atl08DataFrame::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;
    H5Object* _hdf08 = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        _hdf08 = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return DataFrame Object */
        return createLuaObject(L, new Atl08DataFrame(L, beam_str, _parms, _hdf08, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf08) _hdf08->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl08DataFrame::Atl08DataFrame (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf08, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",                 &time_ns},
        {"latitude",                &latitude},
        {"longitude",               &longitude},
        {"segment_id_beg",          &segment_id_beg},
        {"segment_landcover",       &segment_landcover},
        {"segment_snowcover",       &segment_snowcover},
        {"n_seg_ph",                &n_seg_ph},
        {"solar_elevation",         &solar_elevation},
        {"terrain_slope",           &terrain_slope},
        {"n_te_photons",            &n_te_photons},
        {"h_te_uncertainty",        &h_te_uncertainty},
        {"h_te_median",             &h_te_median},
        {"h_canopy",                &h_canopy},
        {"h_canopy_uncertainty",    &h_canopy_uncertainty},
        {"segment_cover",           &segment_cover},
        {"n_ca_photons",            &n_ca_photons},
        {"h_max_canopy",            &h_max_canopy},
        {"h_min_canopy",            &h_min_canopy},
        {"h_mean_canopy",           &h_mean_canopy},
        {"canopy_openness",         &canopy_openness},
        {"canopy_h_metrics",        &canopy_h_metrics}
    },
    {
        {"spot",                    &spot},
        {"cycle",                   &cycle},
        {"region",                  &region},
        {"rgt",                     &rgt},
        {"gt",                      &gt},
        {"granule",                 &granule}
    },
    Icesat2Fields::defaultITRF(_parms->granuleFields.version.value)),
    spot(0, META_COLUMN),
    cycle(_parms->granuleFields.cycle.value, META_COLUMN),
    region(_parms->granuleFields.region.value, META_COLUMN),
    rgt(_parms->granuleFields.rgt.value, META_COLUMN),
    gt(0, META_COLUMN),
    granule(_hdf08->name, META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    outQ(NULL),
    parms(_parms),
    hdf08(_hdf08),
    dfKey(0),
    beam(StringLib::duplicate(beam_str))
{
    assert(_parms);
    assert(_hdf08);

    /* Calculate Key */
    dfKey = calculateBeamKey(beam);

    /* Optional Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Optional Quality Score Columns */
    if(parms->phoreal.te_quality_filter_provided)
    {
        addColumn("te_quality_score", &te_quality_score, false);
    }
    if(parms->phoreal.can_quality_filter_provided)
    {
        addColumn("can_quality_score", &can_quality_score, false);
    }

    /* Call Parent Class Initialization of GeoColumns */
    populateDataframe();

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId(traceId);

    /* Kickoff Reader Thread */
    active.store(true);
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl08DataFrame::~Atl08DataFrame (void)
{
    active.store(false);
    delete readerPid;
    delete [] beam;
    delete outQ;
    parms->releaseLuaObject();
    hdf08->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t Atl08DataFrame::getKey (void) const
{
    return dfKey;
}

/******************************************************************************
 * AreaOfInterest Subclass
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Atl08Data::Constructor
 *----------------------------------------------------------------------------*/
Atl08DataFrame::Atl08Data::Atl08Data (Atl08DataFrame* df, const AreaOfInterest08& aoi):
    sc_orient               (df->hdf08, "/orbit_info/sc_orient"),
    /* Land Segment Datasets */
    delta_time              (df->hdf08, FString("%s/%s", df->beam, "land_segments/delta_time").c_str(),                            0, aoi.first_index, aoi.count),
    segment_id_beg          (df->hdf08, FString("%s/%s", df->beam, "land_segments/segment_id_beg").c_str(),                        0, aoi.first_index, aoi.count),
    segment_landcover       (df->hdf08, FString("%s/%s", df->beam, "land_segments/segment_landcover").c_str(),                     0, aoi.first_index, aoi.count),
    segment_snowcover       (df->hdf08, FString("%s/%s", df->beam, "land_segments/segment_snowcover").c_str(),                     0, aoi.first_index, aoi.count),
    n_seg_ph                (df->hdf08, FString("%s/%s", df->beam, "land_segments/n_seg_ph").c_str(),                              0, aoi.first_index, aoi.count),
    solar_elevation         (df->hdf08, FString("%s/%s", df->beam, "land_segments/solar_elevation").c_str(),                       0, aoi.first_index, aoi.count),
    /* Terrain Datasets */
    terrain_slope           (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/terrain_slope").c_str(),                 0, aoi.first_index, aoi.count),
    n_te_photons            (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/n_te_photons").c_str(),                  0, aoi.first_index, aoi.count),
    te_quality_score        (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/te_quality_score").c_str(),              0, aoi.first_index, aoi.count),
    h_te_uncertainty        (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/h_te_uncertainty").c_str(),              0, aoi.first_index, aoi.count),
    h_te_median             (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/h_te_median").c_str(),                   0, aoi.first_index, aoi.count),
    /* Canopy Datasets */
    h_canopy                (df->hdf08, FString("%s/%s", df->beam, df->parms->phoreal.use_abs_h.value ? "land_segments/canopy/h_canopy_abs" : "land_segments/canopy/h_canopy").c_str(), 0, aoi.first_index, aoi.count),
    h_canopy_uncertainty    (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/h_canopy_uncertainty").c_str(),           0, aoi.first_index, aoi.count),
    segment_cover           (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/segment_cover").c_str(),                  0, aoi.first_index, aoi.count),
    n_ca_photons            (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/n_ca_photons").c_str(),                   0, aoi.first_index, aoi.count),
    can_quality_score       (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/can_quality_score").c_str(),              0, aoi.first_index, aoi.count),
    h_max_canopy            (df->hdf08, FString("%s/%s", df->beam, df->parms->phoreal.use_abs_h.value ? "land_segments/canopy/h_max_canopy_abs" : "land_segments/canopy/h_max_canopy").c_str(), 0, aoi.first_index, aoi.count),
    h_min_canopy            (df->hdf08, FString("%s/%s", df->beam, df->parms->phoreal.use_abs_h.value ? "land_segments/canopy/h_min_canopy_abs" : "land_segments/canopy/h_min_canopy").c_str(), 0, aoi.first_index, aoi.count),
    h_mean_canopy           (df->hdf08, FString("%s/%s", df->beam, df->parms->phoreal.use_abs_h.value ? "land_segments/canopy/h_mean_canopy_abs" : "land_segments/canopy/h_mean_canopy").c_str(), 0, aoi.first_index, aoi.count),
    canopy_openness         (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/canopy_openness").c_str(),                0, aoi.first_index, aoi.count),
    canopy_h_metrics        (df->hdf08, FString("%s/%s", df->beam, df->parms->phoreal.use_abs_h.value ? "land_segments/canopy/canopy_h_metrics_abs" : "land_segments/canopy/canopy_h_metrics").c_str(), H5Coro::ALL_COLS, aoi.first_index, aoi.count),
    anc_data                (df->parms->atl08Fields, df->hdf08, FString("%s/%s", df->beam, "land_segments").c_str(),H5Coro::ALL_COLS, aoi.first_index, aoi.count)
{
    /* Join Hardcoded Reads */
    sc_orient.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    segment_id_beg.join(df->readTimeoutMs, true);
    segment_landcover.join(df->readTimeoutMs, true);
    segment_snowcover.join(df->readTimeoutMs, true);
    n_seg_ph.join(df->readTimeoutMs, true);
    solar_elevation.join(df->readTimeoutMs, true);
    terrain_slope.join(df->readTimeoutMs, true);
    n_te_photons.join(df->readTimeoutMs, true);
    te_quality_score.join(df->readTimeoutMs, true);
    h_te_uncertainty.join(df->readTimeoutMs, true);
    h_te_median.join(df->readTimeoutMs, true);
    h_canopy.join(df->readTimeoutMs, true);
    h_canopy_uncertainty.join(df->readTimeoutMs, true);
    segment_cover.join(df->readTimeoutMs, true);
    n_ca_photons.join(df->readTimeoutMs, true);
    can_quality_score.join(df->readTimeoutMs, true);
    h_max_canopy.join(df->readTimeoutMs, true);
    h_min_canopy.join(df->readTimeoutMs, true);
    h_mean_canopy.join(df->readTimeoutMs, true);
    canopy_openness.join(df->readTimeoutMs, true);
    canopy_h_metrics.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl08DataFrame::subsettingThread (void* parm)
{
    Atl08DataFrame* df = static_cast<Atl08DataFrame*>(parm);
    const Icesat2Fields& parms = *df->parms;
    using std::numeric_limits;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "atl08_subsetter", "{\"context\":\"%s\", \"beam\":%s}", df->hdf08->name, df->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Area of Interest */
        const AreaOfInterest08 aoi(df->hdf08, df->beam, "land_segments/latitude", "land_segments/longitude", df->parms, df->readTimeoutMs);

        /* Read ATL08 Datasets */
        const Atl08Data atl08(df, aoi);

        /* Set MetaData */
        df->spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl08.sc_orient[0], df->beam);

        /* Check Spot Filter */
        if(!parms.spots[static_cast<Icesat2Fields::spot_t>(df->spot.value)])
        {
            throw RunTimeException(DEBUG, RTE_STATUS, "spot %d filtered out", df->spot.value);
        }

        /* Track/Pair for ground track */
        Icesat2Fields::track_t track;
        int pair;
        if     (df->beam[2] == '1') track = Icesat2Fields::RPT_1;
        else if(df->beam[2] == '2') track = Icesat2Fields::RPT_2;
        else if(df->beam[2] == '3') track = Icesat2Fields::RPT_3;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam: %s", df->beam);

        if     (df->beam[3] == 'l') pair = Icesat2Fields::RPT_L;
        else if(df->beam[3] == 'r') pair = Icesat2Fields::RPT_R;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam: %s", df->beam);

        /* Ground track depends on spacecraft orientation and track/pair */
        df->gt = Icesat2Fields::getGroundTrack((Icesat2Fields::sc_orient_t)atl08.sc_orient[0], track, pair);

        /* Loop Through Each Segment */
        for(long segment = 0; df->active.load() && segment < aoi.count; segment++)
        {
            /* Check for Inclusion Mask */
            if(aoi.inclusion_ptr && !aoi.inclusion_ptr[segment])
            {
                continue;
            }

            /* Apply Quality Filters */
            if(parms.phoreal.te_quality_filter_provided)
            {
                if(atl08.te_quality_score[segment] < parms.phoreal.te_quality_filter.value)
                {
                    continue;
                }
            }
            if(parms.phoreal.can_quality_filter_provided)
            {
                if(atl08.can_quality_score[segment] < parms.phoreal.can_quality_filter.value)
                {
                    continue;
                }
            }

            /* Add Row */
            df->addRow();

            /* Populate Columns */
            df->time_ns.append(Icesat2Fields::deltatime2timestamp(atl08.delta_time[segment]));
            df->latitude.append(static_cast<double>(aoi.latitude[segment]));
            df->longitude.append(static_cast<double>(aoi.longitude[segment]));
            df->segment_id_beg.append(atl08.segment_id_beg[segment]);
            df->segment_landcover.append(atl08.segment_landcover[segment] != numeric_limits<uint8_t>::max() ? atl08.segment_landcover[segment] : 0);
            df->segment_snowcover.append(atl08.segment_snowcover[segment] != numeric_limits<uint8_t>::max() ? atl08.segment_snowcover[segment] : 0);
            df->n_seg_ph.append(atl08.n_seg_ph[segment] != numeric_limits<int32_t>::max() ? atl08.n_seg_ph[segment] : 0);
            df->solar_elevation.append(atl08.solar_elevation[segment] != numeric_limits<float>::max() ? atl08.solar_elevation[segment] : numeric_limits<float>::quiet_NaN());
            df->terrain_slope.append(atl08.terrain_slope[segment] != numeric_limits<float>::max() ? atl08.terrain_slope[segment] : numeric_limits<float>::quiet_NaN());
            df->n_te_photons.append(atl08.n_te_photons[segment] != numeric_limits<int32_t>::max() ? atl08.n_te_photons[segment] : 0);
            df->h_te_uncertainty.append(atl08.h_te_uncertainty[segment] != numeric_limits<float>::max() ? atl08.h_te_uncertainty[segment] : numeric_limits<float>::quiet_NaN());
            df->h_te_median.append(atl08.h_te_median[segment] != numeric_limits<float>::max() ? atl08.h_te_median[segment] : numeric_limits<float>::quiet_NaN());
            df->h_canopy.append(atl08.h_canopy[segment] != numeric_limits<float>::max() ? atl08.h_canopy[segment] : numeric_limits<float>::quiet_NaN());
            df->h_canopy_uncertainty.append(atl08.h_canopy_uncertainty[segment] != numeric_limits<float>::max() ? atl08.h_canopy_uncertainty[segment] : numeric_limits<float>::quiet_NaN());
            df->segment_cover.append(atl08.segment_cover[segment] != numeric_limits<int16_t>::max() ? atl08.segment_cover[segment] : 0);
            df->n_ca_photons.append(atl08.n_ca_photons[segment] != numeric_limits<int32_t>::max() ? atl08.n_ca_photons[segment] : 0);
            df->h_max_canopy.append(atl08.h_max_canopy[segment] != numeric_limits<float>::max() ? atl08.h_max_canopy[segment] : numeric_limits<float>::quiet_NaN());
            df->h_min_canopy.append(atl08.h_min_canopy[segment] != numeric_limits<float>::max() ? atl08.h_min_canopy[segment] : numeric_limits<float>::quiet_NaN());
            df->h_mean_canopy.append(atl08.h_mean_canopy[segment] != numeric_limits<float>::max() ? atl08.h_mean_canopy[segment] : numeric_limits<float>::quiet_NaN());
            df->canopy_openness.append(atl08.canopy_openness[segment] != numeric_limits<float>::max() ? atl08.canopy_openness[segment] : numeric_limits<float>::quiet_NaN());

            FieldArray<float,NUM_CANOPY_METRICS> metrics;
            const long metrics_offset = segment * NUM_CANOPY_METRICS;
            for(int p = 0; p < NUM_CANOPY_METRICS; p++)
            {
                const float value = atl08.canopy_h_metrics.pointer[metrics_offset + p];
                metrics[p] = (value != numeric_limits<float>::max()) ? value : numeric_limits<float>::quiet_NaN();
            }
            df->canopy_h_metrics.append(metrics);

            if(parms.phoreal.te_quality_filter_provided)
            {
                df->te_quality_score.append(atl08.te_quality_score[segment]);
            }
            if(parms.phoreal.can_quality_filter_provided)
            {
                df->can_quality_score.append(atl08.can_quality_score[segment]);
            }

            /* Ancillary Data */
            if(atl08.anc_data.length() > 0)
            {
                atl08.anc_data.addToGDF(df, segment);
            }

        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf08->name, df->beam, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
