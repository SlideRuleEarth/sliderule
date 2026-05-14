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

#include "Atl06DataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl06DataFrame::LUA_META_NAME = "Atl06DataFrame";
const struct luaL_Reg Atl06DataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * ATL06 DATAFRAME CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<beam>, <parms>, <hdf06>, <outq_name>)
 *----------------------------------------------------------------------------*/
int Atl06DataFrame::luaCreate (lua_State* L)
{
    Atl06Parameters* _parms = NULL;
    H5Object* _hdf06 = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<Atl06Parameters*>(getLuaObject(L, 2, Atl06Parameters::OBJECT_TYPE, Atl06Parameters::LUA_META_NAME));
        _hdf06 = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE, NULL, true, NULL));
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return DataFrame Object */
        return createLuaObject(L, new Atl06DataFrame(L, beam_str, _parms, _hdf06, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf06) _hdf06->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06DataFrame::Atl06DataFrame (lua_State* L, const char* beam_str, Atl06Parameters* _parms, H5Object* _hdf06, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",                 &time_ns,                   "Unix time (nanoseconds) of the photon measurement"},
        {"latitude",                &latitude,                  "Latitude (EPSG:9989)"},
        {"longitude",               &longitude,                 "Longitude (EPSG:9989)"},
        {"x_atc",                   &x_atc,                     "Along-track x-coordinate of the segment (in meters), measured parallel to the RGT, measured from the ascending node of the equatorial crossing of a given RGT"},
        {"y_atc",                   &y_atc,                     "Along-track y-coordinate of the segment (in meters), relative to the RGT, measured along the perpendicular to the RGT, positive to the right of the RGT"},
        {"h_li",                    &h_li,                      "Standard land-ice segment height determined by land ice algorithm, corrected for first-photon bias, representing the median-based height of the selected PEs"},
        {"h_li_sigma",              &h_li_sigma,                "Propagated error due to sampling error and FPB correction from the land ice algorithm"},
        {"sigma_geo_h",             &sigma_geo_h,               "Total vertical geolocation error due to PPD and POD, including the effects of horizontal geolocation error on the segment vertical error"},
        {"atl06_quality_summary",   &atl06_quality_summary,     "Flag; 0: No likely problems identified for the segment, 1: One or more likely problems identified for the segment"},
        {"segment_id",              &segment_id,                "Segment number, counting from the equator; equal to the segment_id for the second of the two 20-m ATL03 segments included in the 40-m ATL06 segment"},
        {"seg_azimuth",             &seg_azimuth,               "The azimuth of the pair track, east of local north"},
        {"dh_fit_dx",               &dh_fit_dx,                 "Along-track slope from along-track segment fit"},
        {"h_robust_sprd",           &h_robust_sprd,             "RDE of misfit between PE heights and the along-track segment fit"},
        {"w_surface_window_final",  &w_surface_window_final,    "Width of the surface window (in meters), top to bottom"},
        {"bsnow_conf",              &bsnow_conf,                "Blowing snow confidence; -3: surface not detected, -2: no surface wind, -1: no scattering layer found, 0: no top layer found, 1: none-little, 2: weak, 3: moderate, 4: moderate-high, 5: high, 6: very high"},
        {"bsnow_h",                 &bsnow_h,                   "Blowing snow layer top height"},
        {"r_eff",                   &r_eff,                     "Effective reflectance, uncorrected for atmospheric effects"},
        {"tide_ocean",              &tide_ocean,                "Ocean tide"},
        {"n_fit_photons",           &n_fit_photons,             "Number of PEs used in determining h_li after editing"}
    },
    {
        {"spot",                    &spot,                      "ATLAS detector spot"},
        {"cycle",                   &cycle,                     "ICESat-2 Cycle number"},
        {"region",                  &region,                    "ICESat-2 Region (0 to 14, see ATL03 ATBD)"},
        {"rgt",                     &rgt,                       "ICESat-2 Reference ground track"},
        {"gt",                      &gt,                        "Ground track; integer representation of beam"},
        {"granule",                 &granule,                   "Name of the source ATL03 granule"}
    },
    Icesat2Parameters::defaultITRF(_parms->granuleFields.version.value), // crs
    Icesat2Parameters::calculateBeamKey(beam_str)), // dfKey
    spot(0, META_COLUMN),
    cycle(_parms->granuleFields.cycle.value, META_COLUMN),
    region(_parms->granuleFields.region.value, META_COLUMN),
    rgt(_parms->granuleFields.rgt.value, META_COLUMN),
    gt(0, META_COLUMN),
    granule(_hdf06 ? _hdf06->name : "null", META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    outQ(NULL),
    parms(_parms),
    hdf06(_hdf06),
    beam(StringLib::duplicate(beam_str))
{
    /* Optional Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Call Parent Class Initialization of GeoColumns */
    populateGeoColumns();

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Start Reader Thread */
    if(_hdf06)
    {
        active.store(true);
        readerPid = new Thread(subsettingThread, this);
    }
    else // nothing to do
    {
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl06DataFrame::~Atl06DataFrame (void)
{
    active.store(false);
    delete readerPid;
    delete [] beam;
    delete outQ;
    parms->releaseLuaObject();
    if(hdf06) hdf06->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Atl06Data::Constructor
 *----------------------------------------------------------------------------*/
Atl06DataFrame::Atl06Data::Atl06Data (Atl06DataFrame* df, const AreaOfInterest<double>& aoi):
    sc_orient               (df->hdf06, "/orbit_info/sc_orient"),
    delta_time              (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/delta_time").c_str(),                            0, aoi.first_index, aoi.count),
    h_li                    (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/h_li").c_str(),                                  0, aoi.first_index, aoi.count),
    h_li_sigma              (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/h_li_sigma").c_str(),                            0, aoi.first_index, aoi.count),
    atl06_quality_summary   (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/atl06_quality_summary").c_str(),                 0, aoi.first_index, aoi.count),
    segment_id              (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/segment_id").c_str(),                            0, aoi.first_index, aoi.count),
    sigma_geo_h             (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/sigma_geo_h").c_str(),                           0, aoi.first_index, aoi.count),
    x_atc                   (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/ground_track/x_atc").c_str(),                    0, aoi.first_index, aoi.count),
    y_atc                   (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/ground_track/y_atc").c_str(),                    0, aoi.first_index, aoi.count),
    seg_azimuth             (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/ground_track/seg_azimuth").c_str(),              0, aoi.first_index, aoi.count),
    dh_fit_dx               (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/fit_statistics/dh_fit_dx").c_str(),              0, aoi.first_index, aoi.count),
    h_robust_sprd           (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/fit_statistics/h_robust_sprd").c_str(),          0, aoi.first_index, aoi.count),
    n_fit_photons           (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/fit_statistics/n_fit_photons").c_str(),          0, aoi.first_index, aoi.count),
    w_surface_window_final  (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/fit_statistics/w_surface_window_final").c_str(), 0, aoi.first_index, aoi.count),
    bsnow_conf              (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/geophysical/bsnow_conf").c_str(),                0, aoi.first_index, aoi.count),
    bsnow_h                 (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/geophysical/bsnow_h").c_str(),                   0, aoi.first_index, aoi.count),
    r_eff                   (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/geophysical/r_eff").c_str(),                     0, aoi.first_index, aoi.count),
    tide_ocean              (df->hdf06, FString("%s/%s", df->beam, "land_ice_segments/geophysical/tide_ocean").c_str(),                0, aoi.first_index, aoi.count),
    anc_data                (df->parms->atl06Fields, df->hdf06, FString("%s/%s", df->beam, "land_ice_segments").c_str(),H5Coro::ALL_COLS, aoi.first_index, aoi.count)
{
    /* Join Hardcoded Reads */
    sc_orient.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    h_li.join(df->readTimeoutMs, true);
    h_li_sigma.join(df->readTimeoutMs, true);
    atl06_quality_summary.join(df->readTimeoutMs, true);
    segment_id.join(df->readTimeoutMs, true);
    sigma_geo_h.join(df->readTimeoutMs, true);
    x_atc.join(df->readTimeoutMs, true);
    y_atc.join(df->readTimeoutMs, true);
    seg_azimuth.join(df->readTimeoutMs, true);
    dh_fit_dx.join(df->readTimeoutMs, true);
    h_robust_sprd.join(df->readTimeoutMs, true);
    n_fit_photons.join(df->readTimeoutMs, true);
    w_surface_window_final.join(df->readTimeoutMs, true);
    bsnow_conf.join(df->readTimeoutMs, true);
    bsnow_h.join(df->readTimeoutMs, true);
    r_eff.join(df->readTimeoutMs, true);
    tide_ocean.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl06DataFrame::subsettingThread (void* parm)
{
    Atl06DataFrame* df = static_cast<Atl06DataFrame*>(parm);
    const Atl06Parameters& parms = *df->parms;
    using std::numeric_limits;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "atl06_subsetter", "{\"context\":\"%s\", \"beam\":%s}", df->hdf06->name, df->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Area of Interest */
        const AreaOfInterest<double> aoi(df->hdf06, df->beam, "land_ice_segments/latitude", "land_ice_segments/longitude", df->parms, df->readTimeoutMs);

        /* Read ATL06 Datasets */
        const Atl06Data atl06(df, aoi);

        /* Set MetaData */
        df->spot = Icesat2Parameters::getSpotNumber((Icesat2Parameters::sc_orient_t)atl06.sc_orient[0], df->beam);

        /* Check Spot Filter */
        if(!parms.spots[static_cast<Icesat2Parameters::spot_t>(df->spot.value)])
        {
            throw RunTimeException(DEBUG, RTE_STATUS, "spot %d filtered out", df->spot.value);
        }

        /* Track/Pair for ground track */
        Icesat2Parameters::track_t track;
        int pair;
        if     (df->beam[2] == '1') track = Icesat2Parameters::RPT_1;
        else if(df->beam[2] == '2') track = Icesat2Parameters::RPT_2;
        else if(df->beam[2] == '3') track = Icesat2Parameters::RPT_3;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam: %s", df->beam);

        if     (df->beam[3] == 'l') pair = Icesat2Parameters::RPT_L;
        else if(df->beam[3] == 'r') pair = Icesat2Parameters::RPT_R;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam: %s", df->beam);

        /* Ground track depends on spacecraft orientation and track/pair */
        df->gt = Icesat2Parameters::getGroundTrack((Icesat2Parameters::sc_orient_t)atl06.sc_orient[0], track, pair);

        /* Loop Through Each Segment */
        for(long segment = 0; df->active.load() && segment < aoi.count; segment++)
        {
            /* Check for Inclusion Mask */
            if(aoi.inclusion_ptr && !aoi.inclusion_ptr[segment])
            {
                continue;
            }

            /* Add Row */
            df->addRow();

            /* Populate Columns */
            df->time_ns.append(Icesat2Parameters::deltatime2timestamp(atl06.delta_time[segment]));
            df->latitude.append(aoi.latitude[segment]);
            df->longitude.append(aoi.longitude[segment]);
            df->segment_id.append(atl06.segment_id[segment]);
            df->atl06_quality_summary.append(atl06.atl06_quality_summary[segment]);
            df->bsnow_conf.append(atl06.bsnow_conf[segment]);
            df->n_fit_photons.append(atl06.n_fit_photons[segment] != numeric_limits<int32_t>::max() ? atl06.n_fit_photons[segment] : 0);
            df->x_atc.append(atl06.x_atc[segment] != numeric_limits<double>::max() ? atl06.x_atc[segment] : numeric_limits<double>::quiet_NaN());
            df->y_atc.append(atl06.y_atc[segment] != numeric_limits<float>::max() ? atl06.y_atc[segment] : numeric_limits<float>::quiet_NaN());
            df->h_li.append(atl06.h_li[segment] != numeric_limits<float>::max() ? atl06.h_li[segment] : numeric_limits<float>::quiet_NaN());
            df->h_li_sigma.append(atl06.h_li_sigma[segment] != numeric_limits<float>::max() ? atl06.h_li_sigma[segment] : numeric_limits<float>::quiet_NaN());
            df->sigma_geo_h.append(atl06.sigma_geo_h[segment] != numeric_limits<float>::max() ? atl06.sigma_geo_h[segment] : numeric_limits<float>::quiet_NaN());
            df->seg_azimuth.append(atl06.seg_azimuth[segment] != numeric_limits<float>::max() ? atl06.seg_azimuth[segment] : numeric_limits<float>::quiet_NaN());
            df->dh_fit_dx.append(atl06.dh_fit_dx[segment] != numeric_limits<float>::max() ? atl06.dh_fit_dx[segment] : numeric_limits<float>::quiet_NaN());
            df->h_robust_sprd.append(atl06.h_robust_sprd[segment] != numeric_limits<float>::max() ? atl06.h_robust_sprd[segment] : numeric_limits<float>::quiet_NaN());
            df->w_surface_window_final.append(atl06.w_surface_window_final[segment] != numeric_limits<float>::max() ? atl06.w_surface_window_final[segment] : numeric_limits<float>::quiet_NaN());
            df->bsnow_h.append(atl06.bsnow_h[segment] != numeric_limits<float>::max() ? atl06.bsnow_h[segment] : numeric_limits<float>::quiet_NaN());
            df->r_eff.append(atl06.r_eff[segment] != numeric_limits<float>::max() ? atl06.r_eff[segment] : numeric_limits<float>::quiet_NaN());
            df->tide_ocean.append(atl06.tide_ocean[segment] != numeric_limits<float>::max() ? atl06.tide_ocean[segment] : numeric_limits<float>::quiet_NaN());

            /* Ancillary Data */
            if(atl06.anc_data.length() > 0)
            {
                atl06.anc_data.addToGDF(df, segment);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf06->name, df->beam, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
