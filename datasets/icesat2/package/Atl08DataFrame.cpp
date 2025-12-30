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
#include "TraceGuard.h"
#include "AreaOfInterest.h"
#include "LuaObject.h"
#include "StringLib.h"
#include "RunTimeException.h"

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
        {"extent_id",               &extent_id},
        {"time_ns",                 &time_ns},
        {"delta_time",              &delta_time_col},
        {"latitude",                &latitude},
        {"longitude",               &longitude},
        {"segment_id_beg",          &segment_id_beg},
        {"segment_id_end",          &segment_id_end},
        {"night_flag",              &night_flag},
        {"n_seg_ph",                &n_seg_ph},
        {"solar_elevation",         &solar_elevation},
        {"solar_azimuth",           &solar_azimuth},
        {"terrain_flg",             &terrain_flg},
        {"brightness_flag",         &brightness_flag},
        {"cloud_flag_atm",          &cloud_flag_atm},
        {"h_te_best_fit",           &h_te_best_fit},
        {"h_te_interp",             &h_te_interp},
        {"terrain_slope",           &terrain_slope},
        {"n_te_photons",            &n_te_photons},
        {"te_quality_score",        &te_quality_score},
        {"h_te_uncertainty",        &h_te_uncertainty},
        {"h_canopy",                &h_canopy},
        {"h_canopy_abs",            &h_canopy_abs},
        {"h_canopy_uncertainty",    &h_canopy_uncertainty},
        {"segment_cover",           &segment_cover},
        {"n_ca_photons",            &n_ca_photons},
        {"can_quality_score",       &can_quality_score}
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
    delta_time              (df->hdf08, FString("%s/%s", df->beam, "land_segments/delta_time").c_str(),                            0, aoi.first_segment, aoi.num_segments),
    segment_id_beg          (df->hdf08, FString("%s/%s", df->beam, "land_segments/segment_id_beg").c_str(),                        0, aoi.first_segment, aoi.num_segments),
    segment_id_end          (df->hdf08, FString("%s/%s", df->beam, "land_segments/segment_id_end").c_str(),                        0, aoi.first_segment, aoi.num_segments),
    night_flag              (df->hdf08, FString("%s/%s", df->beam, "land_segments/night_flag").c_str(),                            0, aoi.first_segment, aoi.num_segments),
    n_seg_ph                (df->hdf08, FString("%s/%s", df->beam, "land_segments/n_seg_ph").c_str(),                              0, aoi.first_segment, aoi.num_segments),
    solar_elevation         (df->hdf08, FString("%s/%s", df->beam, "land_segments/solar_elevation").c_str(),                       0, aoi.first_segment, aoi.num_segments),
    solar_azimuth           (df->hdf08, FString("%s/%s", df->beam, "land_segments/solar_azimuth").c_str(),                         0, aoi.first_segment, aoi.num_segments),
    terrain_flg             (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain_flg").c_str(),                           0, aoi.first_segment, aoi.num_segments),
    brightness_flag         (df->hdf08, FString("%s/%s", df->beam, "land_segments/brightness_flag").c_str(),                       0, aoi.first_segment, aoi.num_segments),
    cloud_flag_atm          (df->hdf08, FString("%s/%s", df->beam, "land_segments/cloud_flag_atm").c_str(),                        0, aoi.first_segment, aoi.num_segments),
    /* Terrain Datasets */
    h_te_best_fit           (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/h_te_best_fit").c_str(),                 0, aoi.first_segment, aoi.num_segments),
    h_te_interp             (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/h_te_interp").c_str(),                   0, aoi.first_segment, aoi.num_segments),
    terrain_slope           (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/terrain_slope").c_str(),                 0, aoi.first_segment, aoi.num_segments),
    n_te_photons            (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/n_te_photons").c_str(),                  0, aoi.first_segment, aoi.num_segments),
    te_quality_score        (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/te_quality_score").c_str(),              0, aoi.first_segment, aoi.num_segments),
    h_te_uncertainty        (df->hdf08, FString("%s/%s", df->beam, "land_segments/terrain/h_te_uncertainty").c_str(),              0, aoi.first_segment, aoi.num_segments),
    /* Canopy Datasets */
    h_canopy                (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/h_canopy").c_str(),                       0, aoi.first_segment, aoi.num_segments),
    h_canopy_abs            (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/h_canopy_abs").c_str(),                   0, aoi.first_segment, aoi.num_segments),
    h_canopy_uncertainty    (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/h_canopy_uncertainty").c_str(),           0, aoi.first_segment, aoi.num_segments),
    segment_cover           (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/segment_cover").c_str(),                  0, aoi.first_segment, aoi.num_segments),
    n_ca_photons            (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/n_ca_photons").c_str(),                   0, aoi.first_segment, aoi.num_segments),
    can_quality_score       (df->hdf08, FString("%s/%s", df->beam, "land_segments/canopy/can_quality_score").c_str(),              0, aoi.first_segment, aoi.num_segments),
    anc_data                (df->parms->atl08Fields, df->hdf08, FString("%s/%s", df->beam, "land_segments").c_str(),H5Coro::ALL_COLS, aoi.first_segment, aoi.num_segments)
{
    /* Join Hardcoded Reads */
    sc_orient.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    segment_id_beg.join(df->readTimeoutMs, true);
    segment_id_end.join(df->readTimeoutMs, true);
    night_flag.join(df->readTimeoutMs, true);
    n_seg_ph.join(df->readTimeoutMs, true);
    solar_elevation.join(df->readTimeoutMs, true);
    solar_azimuth.join(df->readTimeoutMs, true);
    terrain_flg.join(df->readTimeoutMs, true);
    brightness_flag.join(df->readTimeoutMs, true);
    cloud_flag_atm.join(df->readTimeoutMs, true);
    h_te_best_fit.join(df->readTimeoutMs, true);
    h_te_interp.join(df->readTimeoutMs, true);
    terrain_slope.join(df->readTimeoutMs, true);
    n_te_photons.join(df->readTimeoutMs, true);
    te_quality_score.join(df->readTimeoutMs, true);
    h_te_uncertainty.join(df->readTimeoutMs, true);
    h_canopy.join(df->readTimeoutMs, true);
    h_canopy_abs.join(df->readTimeoutMs, true);
    h_canopy_uncertainty.join(df->readTimeoutMs, true);
    segment_cover.join(df->readTimeoutMs, true);
    n_ca_photons.join(df->readTimeoutMs, true);
    can_quality_score.join(df->readTimeoutMs, true);

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
    const TraceGuard trace(INFO, df->traceId, "atl08_subsetter", "{\"context\":\"%s\", \"beam\":%s}", df->hdf08->name, df->beam);
    trace.stash(); // set thread specific trace id for H5Coro

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

        /* Track/Pair for extent_id generation */
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
        uint32_t extent_counter = 0;
        for(long segment = 0; df->active.load() && segment < aoi.num_segments; segment++)
        {
            /* Check for Inclusion Mask */
            if(aoi.inclusion_ptr && !aoi.inclusion_ptr[segment])
            {
                continue;
            }

            /* Add Row */
            df->addRow();

            /* Populate Columns */
            const uint64_t extent_id = Icesat2Fields::generateExtentId(parms.granuleFields.rgt.value, parms.granuleFields.cycle.value, parms.granuleFields.region.value, track, pair, extent_counter) | Icesat2Fields::EXTENT_ID_ELEVATION;
            df->extent_id.append(extent_id);
            df->time_ns.append(Icesat2Fields::deltatime2timestamp(atl08.delta_time[segment]));
            df->delta_time_col.append(atl08.delta_time[segment]);
            df->latitude.append(static_cast<double>(aoi.latitude[segment]));
            df->longitude.append(static_cast<double>(aoi.longitude[segment]));
            df->segment_id_beg.append(atl08.segment_id_beg[segment]);
            df->segment_id_end.append(atl08.segment_id_end[segment]);
            df->night_flag.append(atl08.night_flag[segment]);
            df->n_seg_ph.append(atl08.n_seg_ph[segment] != numeric_limits<int32_t>::max() ? atl08.n_seg_ph[segment] : 0);
            df->solar_elevation.append(atl08.solar_elevation[segment] != numeric_limits<float>::max() ? atl08.solar_elevation[segment] : numeric_limits<float>::quiet_NaN());
            df->solar_azimuth.append(atl08.solar_azimuth[segment] != numeric_limits<float>::max() ? atl08.solar_azimuth[segment] : numeric_limits<float>::quiet_NaN());
            df->terrain_flg.append(atl08.terrain_flg[segment]);
            df->brightness_flag.append(atl08.brightness_flag[segment]);
            df->cloud_flag_atm.append(atl08.cloud_flag_atm[segment]);
            df->h_te_best_fit.append(atl08.h_te_best_fit[segment] != numeric_limits<float>::max() ? atl08.h_te_best_fit[segment] : numeric_limits<float>::quiet_NaN());
            df->h_te_interp.append(atl08.h_te_interp[segment] != numeric_limits<float>::max() ? atl08.h_te_interp[segment] : numeric_limits<float>::quiet_NaN());
            df->terrain_slope.append(atl08.terrain_slope[segment] != numeric_limits<float>::max() ? atl08.terrain_slope[segment] : numeric_limits<float>::quiet_NaN());
            df->n_te_photons.append(atl08.n_te_photons[segment] != numeric_limits<int32_t>::max() ? atl08.n_te_photons[segment] : 0);
            df->te_quality_score.append(atl08.te_quality_score[segment]);
            df->h_te_uncertainty.append(atl08.h_te_uncertainty[segment] != numeric_limits<float>::max() ? atl08.h_te_uncertainty[segment] : numeric_limits<float>::quiet_NaN());
            df->h_canopy.append(atl08.h_canopy[segment] != numeric_limits<float>::max() ? atl08.h_canopy[segment] : numeric_limits<float>::quiet_NaN());
            df->h_canopy_abs.append(atl08.h_canopy_abs[segment] != numeric_limits<float>::max() ? atl08.h_canopy_abs[segment] : numeric_limits<float>::quiet_NaN());
            df->h_canopy_uncertainty.append(atl08.h_canopy_uncertainty[segment] != numeric_limits<float>::max() ? atl08.h_canopy_uncertainty[segment] : numeric_limits<float>::quiet_NaN());
            df->segment_cover.append(atl08.segment_cover[segment] != numeric_limits<int16_t>::max() ? atl08.segment_cover[segment] : 0);
            df->n_ca_photons.append(atl08.n_ca_photons[segment] != numeric_limits<int32_t>::max() ? atl08.n_ca_photons[segment] : 0);
            df->can_quality_score.append(atl08.can_quality_score[segment]);

            /* Ancillary Data */
            if(atl08.anc_data.length() > 0)
            {
                atl08.anc_data.addToGDF(df, segment);
            }

            /* Bump Extent Counter */
            extent_counter++;
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf08->name, df->beam, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Return */
    return NULL;
}
