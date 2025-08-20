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
#include "Atl24DataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl24DataFrame::LUA_META_NAME = "Atl24DataFrame";
const struct luaL_Reg Atl24DataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * atl24 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int Atl24DataFrame::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;
    H5Object* _hdf24 = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 2, Icesat2Fields::OBJECT_TYPE));
        _hdf24 = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 5, true, NULL);

        /* Return Reader Object */
        return createLuaObject(L, new Atl24DataFrame(L, beam_str, _parms, _hdf24, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf24) _hdf24->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl24DataFrame::Atl24DataFrame (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf24, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"class_ph",            &class_ph},
        {"confidence",          &confidence},
        {"time_ns",             &time_ns},
        {"lat_ph",              &lat_ph},
        {"lon_ph",              &lon_ph},
        {"ortho_h",             &ortho_h},
        {"surface_h",           &surface_h},
        {"x_atc",               &x_atc},
        {"y_atc",               &y_atc},
    },
    {
        {"spot",                &spot},
        {"cycle",               &cycle},
        {"region",              &region},
        {"rgt",                 &rgt},
        {"gt",                  &gt},
        {"granule",             &granule}
    },
    Icesat2Fields::missionCRS(_parms->datum.value)),
    granule(_hdf24->name, META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    beam(FString("%s", beam_str).c_str(true)),
    outQ(NULL),
    parms(_parms),
    hdf24(_hdf24)
{
    assert(_parms);
    assert(_hdf24);

    /* Set Non-Compact Columns */
    if(!parms->atl24.compact.value)
    {
        addColumn("ellipse_h",              &ellipse_h,             false);
        addColumn("invalid_kd",             &invalid_kd,            false);
        addColumn("invalid_wind_speed",     &invalid_wind_speed,    false);
        addColumn("low_confidence_flag",    &low_confidence_flag,   false);
        addColumn("night_flag",             &night_flag,            false);
        addColumn("sensor_depth_exceeded",  &sensor_depth_exceeded, false);
        addColumn("sigma_thu",              &sigma_thu,             false);
        addColumn("sigma_tvu",              &sigma_tvu,             false);
    }

    /* Set MetaData from Parameters */
    cycle = parms->granuleFields.cycle.value;
    region = parms->granuleFields.region.value;
    rgt = parms->granuleFields.rgt.value;

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
Atl24DataFrame::~Atl24DataFrame (void)
{
    active = false;
    delete readerPid;
    delete [] beam;
    delete outQ;
    parms->releaseLuaObject();
    hdf24->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t Atl24DataFrame::getKey(void) const
{
    return dfKey;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::Constructor
 *----------------------------------------------------------------------------*/
Atl24DataFrame::AreaOfInterest::AreaOfInterest (const Atl24DataFrame* df):
    lat_ph    (df->hdf24, FString("%s/%s", df->beam, "lat_ph").c_str()),
    lon_ph    (df->hdf24, FString("%s/%s", df->beam, "lon_ph").c_str()),
    inclusion_mask {NULL},
    inclusion_ptr  {NULL}
{
    try
    {
        /* Join Reads */
        lat_ph.join(df->readTimeoutMs, true);
        lon_ph.join(df->readTimeoutMs, true);

        /* Initialize AreaOfInterest */
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
            num_photons = lat_ph.size;
        }

        /* Check If Anything to Process */
        if(num_photons <= 0)
        {
            throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        lat_ph.trim(first_photon);
        lon_ph.trim(first_photon);
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
Atl24DataFrame::AreaOfInterest::~AreaOfInterest (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::cleanup
 *----------------------------------------------------------------------------*/
void Atl24DataFrame::AreaOfInterest::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::polyregion TODO: optimize with binary search
 *----------------------------------------------------------------------------*/
void Atl24DataFrame::AreaOfInterest::polyregion (const Atl24DataFrame* df)
{
    /* Find First Photon In Polygon */
    bool first_photon_found = false;
    int photon = 0;
    while(photon < lat_ph.size)
    {
        /* Test Inclusion */
        const bool inclusion = df->parms->polyIncludes(lon_ph[photon], lat_ph[photon]);

        /* Check First Photon */
        if(!first_photon_found)
        {
            /* If Coordinate Is In Polygon */
            if(inclusion)
            {
                /* Set First Segment */
                first_photon_found = true;
                first_photon = photon;
            }
            else
            {
                /* Update Photon Index */
                first_photon++;
            }
        }
        else
        {
            /* If Coordinate Is NOT In Polygon */
            if(!inclusion)
            {
                break; // full extent found!
            }
        }

        /* Bump Photon */
        photon++;
    }

    /* Set Number of Photons */
    if(first_photon_found)
    {
        num_photons = photon - first_photon;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::rasterregion
 *----------------------------------------------------------------------------*/
void Atl24DataFrame::AreaOfInterest::rasterregion (const Atl24DataFrame* df)
{
    /* Find First Photon In Polygon */
    bool first_photon_found = false;

    /* Check Size */
    if(lat_ph.size <= 0)
    {
        return;
    }

    /* Allocate Inclusion Mask */
    inclusion_mask = new bool [lat_ph.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    long last_photon = 0;
    int photon = 0;
    while(photon < lat_ph.size)
    {
        if(lat_ph[photon] != 0)
        {
            /* Check Inclusion */
            const bool inclusion = df->parms->maskIncludes(lon_ph[photon], lat_ph[photon]);
            inclusion_mask[photon] = inclusion;

            /* Check For First Photon */
            if(!first_photon_found)
            {
                /* If Coordinate Is In Raster */
                if(inclusion)
                {
                    first_photon_found = true;

                    /* Set First Photon */
                    first_photon = photon;
                    last_photon = photon;
                }
                else
                {
                    /* Update Photon Index */
                    first_photon += lat_ph[photon];
                }
            }
            else
            {
                /* If Coordinate Is In Raster */
                if(inclusion)
                {
                    /* Update Last Photon */
                    last_photon = photon;
                }
            }
        }
        else
        {
            inclusion_mask[photon] = false;
        }

        /* Bump Photon */
        photon++;
    }

    /* Set Number of Photons */
    if(first_photon_found)
    {
        num_photons = last_photon - first_photon + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_photon];
    }
    else
    {
        num_photons = 0;
    }
}

/*----------------------------------------------------------------------------
 * Atl24Data::Constructor
 *----------------------------------------------------------------------------*/
Atl24DataFrame::Atl24Data::Atl24Data (Atl24DataFrame* df, const AreaOfInterest& aoi):
    compact                 (df->parms->atl24.compact.value),
    sc_orient               (                 df->hdf24,                            "orbit_info/sc_orient"),
    class_ph                (                 df->hdf24, FString("%s/%s", df->beam, "class_ph").c_str(),                0, aoi.first_photon, aoi.num_photons),
    confidence              (                 df->hdf24, FString("%s/%s", df->beam, "confidence").c_str(),              0, aoi.first_photon, aoi.num_photons),
    delta_time              (                 df->hdf24, FString("%s/%s", df->beam, "delta_time").c_str(),              0, aoi.first_photon, aoi.num_photons),
    ellipse_h               (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "ellipse_h").c_str(),               0, aoi.first_photon, aoi.num_photons),
    invalid_kd              (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "invalid_kd").c_str(),              0, aoi.first_photon, aoi.num_photons),
    invalid_wind_speed      (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "invalid_wind_speed").c_str(),      0, aoi.first_photon, aoi.num_photons),
    low_confidence_flag     (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "low_confidence_flag").c_str(),     0, aoi.first_photon, aoi.num_photons),
    night_flag              (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "night_flag").c_str(),              0, aoi.first_photon, aoi.num_photons),
    ortho_h                 (                 df->hdf24, FString("%s/%s", df->beam, "ortho_h").c_str(),                 0, aoi.first_photon, aoi.num_photons),
    sensor_depth_exceeded   (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "sensor_depth_exceeded").c_str(),   0, aoi.first_photon, aoi.num_photons),
    sigma_thu               (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "sigma_thu").c_str(),               0, aoi.first_photon, aoi.num_photons),
    sigma_tvu               (compact ? NULL : df->hdf24, FString("%s/%s", df->beam, "sigma_tvu").c_str(),               0, aoi.first_photon, aoi.num_photons),
    surface_h               (                 df->hdf24, FString("%s/%s", df->beam, "surface_h").c_str(),               0, aoi.first_photon, aoi.num_photons),
    x_atc                   (                 df->hdf24, FString("%s/%s", df->beam, "x_atc").c_str(),                   0, aoi.first_photon, aoi.num_photons),
    y_atc                   (                 df->hdf24, FString("%s/%s", df->beam, "y_atc").c_str(),                   0, aoi.first_photon, aoi.num_photons),
    anc_data                (                 df->parms->atl24.anc_fields, df->hdf24, FString("%s", df->beam).c_str(),  0, aoi.first_photon, aoi.num_photons)
{
    /* Join Hardcoded Reads */
    sc_orient.join(df->readTimeoutMs, true);
    class_ph.join(df->readTimeoutMs, true);
    confidence.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    if(!compact) ellipse_h.join(df->readTimeoutMs, true);
    if(!compact) invalid_kd.join(df->readTimeoutMs, true);
    if(!compact) invalid_wind_speed.join(df->readTimeoutMs, true);
    if(!compact) low_confidence_flag.join(df->readTimeoutMs, true);
    if(!compact) night_flag.join(df->readTimeoutMs, true);
    ortho_h.join(df->readTimeoutMs, true);
    if(!compact) sensor_depth_exceeded.join(df->readTimeoutMs, true);
    if(!compact) sigma_thu.join(df->readTimeoutMs, true);
    if(!compact) sigma_tvu.join(df->readTimeoutMs, true);
    surface_h.join(df->readTimeoutMs, true);
    x_atc.join(df->readTimeoutMs, true);
    y_atc.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Atl24DataFrame::subsettingThread (void* parm)
{
    /* Get Thread Info */
    Atl24DataFrame* df = static_cast<Atl24DataFrame*>(parm);
    const Icesat2Fields& parms = *df->parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "atl03_subsetter", "{\"context\":\"%s\", \"beam\":%s}", df->hdf24->name, df->beam);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to AreaOfInterest of Interest */
        const AreaOfInterest aoi(df);

        /* Read ATL24 Datasets */
        const Atl24Data atl24(df, aoi);

        /* Set MetaData */
        df->spot = Icesat2Fields::getSpotNumber((Icesat2Fields::sc_orient_t)atl24.sc_orient[0], df->beam);
        df->gt = Icesat2Fields::getGroundTrack(df->beam);

        /* Traverse All Photons In Dataset */
        int32_t current_photon = -1;
        while(df->active && (++current_photon < atl24.class_ph.size))
        {
            /* Check AreaOfInterest Mask */
            if(aoi.inclusion_ptr)
            {
                if(!aoi.inclusion_ptr[current_photon])
                {
                    continue;
                }
            }

            /* Check Class */
            const Atl24Fields::class_t class_ph = static_cast<Atl24Fields::class_t>(atl24.class_ph[current_photon]);
            if(!parms.atl24.class_ph[class_ph]) continue;

            /* Check Confidence */
            if(atl24.confidence[current_photon] < parms.atl24.confidence_threshold.value) continue;

            if(!parms.atl24.compact.value)
            {
                /* Check Invalid Kd */
                const Atl24Fields::flag_t invalid_kd_flag = static_cast<Atl24Fields::flag_t>(atl24.invalid_kd[current_photon]);
                if(!parms.atl24.invalid_kd[invalid_kd_flag]) continue;

                /* Check Invalid Wind Speed */
                const Atl24Fields::flag_t invalid_wind_speed_flag = static_cast<Atl24Fields::flag_t>(atl24.invalid_wind_speed[current_photon]);
                if(!parms.atl24.invalid_wind_speed[invalid_wind_speed_flag]) continue;

                /* Check Low Confidence */
                const Atl24Fields::flag_t low_confidence_flag = static_cast<Atl24Fields::flag_t>(atl24.low_confidence_flag[current_photon]);
                if(!parms.atl24.low_confidence[low_confidence_flag]) continue;

                /* Check Night */
                const Atl24Fields::flag_t night_flag = static_cast<Atl24Fields::flag_t>(atl24.night_flag[current_photon]);
                if(!parms.atl24.night[night_flag]) continue;

                /* Check Night */
                const Atl24Fields::flag_t sensor_depth_exceeded_flag = static_cast<Atl24Fields::flag_t>(atl24.sensor_depth_exceeded[current_photon]);
                if(!parms.atl24.sensor_depth_exceeded[sensor_depth_exceeded_flag]) continue;
            }

            /* Add Photon to DataFrame */
            df->addRow();
            df->class_ph.append(atl24.class_ph[current_photon]);
            df->confidence.append(atl24.confidence[current_photon]);
            df->time_ns.append(Icesat2Fields::deltatime2timestamp(atl24.delta_time[current_photon]));
            df->lat_ph.append(aoi.lat_ph[current_photon]);
            df->lon_ph.append(aoi.lon_ph[current_photon]);
            df->ortho_h.append(atl24.ortho_h[current_photon]);
            df->surface_h.append(atl24.surface_h[current_photon]);
            df->x_atc.append(atl24.x_atc[current_photon]);
            df->y_atc.append(atl24.y_atc[current_photon]);
            if(!parms.atl24.compact)
            {
                df->ellipse_h.append(atl24.ellipse_h[current_photon]);
                df->invalid_kd.append(atl24.invalid_kd[current_photon]);
                df->invalid_wind_speed.append(atl24.invalid_wind_speed[current_photon]);
                df->low_confidence_flag.append(atl24.low_confidence_flag[current_photon]);
                df->night_flag.append(atl24.night_flag[current_photon]);
                df->sensor_depth_exceeded.append(atl24.sensor_depth_exceeded[current_photon]);
                df->sigma_thu.append(atl24.sigma_thu[current_photon]);
                df->sigma_tvu.append(atl24.sigma_tvu[current_photon]);
            }

            /* Add Ancillary Elements */
            if(atl24.anc_data.length() > 0) atl24.anc_data.addToGDF(df, current_photon);
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf24->name, df->beam, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
