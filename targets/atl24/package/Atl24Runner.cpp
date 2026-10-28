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

#include "atl24.h"
#include "ensemble.h"
#include "elevations.h"
#include "estimate_kd.h"
#include "estimate_surface_roughness.h"
#include "xgboost.h"
#include "photon.h"

#include "OsApi.h"
#include "TimeLib.h"
#include "FieldElement.h"
#include "Icesat2Parameters.h"
#include "BathyDataFrame.h"
#include "Atl24Runner.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* Atl24Runner::LUA_META_NAME = "Atl24Runner";
const struct luaL_Reg Atl24Runner::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int Atl24Runner::luaCreate (lua_State* L)
{
    Icesat2Parameters* _parms = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Parameters*>(getLuaObject(L, 1, Icesat2Parameters::OBJECT_TYPE));
        const long _serialize_threshold = getLuaInteger(L, 2, true, DEFAULT_SERIALIZE_THRESHOLD);
        return createLuaObject(L, new Atl24Runner(L, _parms, _serialize_threshold));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl24Runner::Atl24Runner (lua_State* L, Icesat2Parameters* _parms, long _serialize_threshold):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    serializeThreshold(_serialize_threshold)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl24Runner::~Atl24Runner (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool Atl24Runner::run (GeoDataFrame* dataframe)
{
    bool status = true;
    const ATL24::ensemble::Params ensemble_params;
    const ATL24::elevations::ElevationsParams elevations_params;
    const ATL24::estimate_kd::Params estimate_kd_params;
    const ATL24::estimate_surface_roughness::Params estimate_surface_roughness_params;
    FString model_filename("%s/atl24.tgz", CONFDIR);

    // cast dataframe to ATL24 specific dataframe
    BathyDataFrame& df = *dynamic_cast<BathyDataFrame*>(dataframe);
    size_t num_rows = static_cast<size_t>(df.length());

    // create new columns
    FieldColumn<int>* class_ph = new FieldColumn<int>;
    FieldColumn<float>* confidence = new FieldColumn<float>;
    FieldColumn<float>* surface_h = new FieldColumn<float>;
    FieldColumn<float>* kd = new FieldColumn<float>;
    FieldColumn<float>* surface_roughness = new FieldColumn<float>;

    // determine serialization
    const bool serialize = df.length() > serializeThreshold;
    mlog(INFO, "Running classifier on spot %d in %s mode", df.spot.value, serialize ? "serial" : "parallel");

    try
    {
        // convert dataframe to algorithm input structure
        vector<ATL24::photon::Photon> p(df.length());
        for(size_t i = 0; i < static_cast<size_t>(df.length()); ++i)
        {
            p[i].gps_seconds    = TimeLib::sysex2gpstime(df.time_ns[i]);
            p[i].lat_ph         = df.lat_ph[i];
            p[i].lon_ph         = df.lon_ph[i];
            p[i].x_atc          = df.x_atc[i];
            p[i].h_ph           = df.ellipse_h[i];
            p[i].geoid          = df.ellipse_h[i] - df.geoid_corr_h[i];
            p[i].quality_ph     = df.quality_ph[i];
            p[i].spot           = df.spot.value;
        }

        // ENTER conditional serialized execution
        if(serialize) experiment.lock();

        // classify photons
        const ATL24::xgboost::ClassificationResult classification = ATL24::main_pipeline::classify (p, model_filename.c_str(), true, ensemble_params);
        if(classification.labels.size() != num_rows) throw RunTimeException(CRITICAL, RTE_FAILURE, "size mismatch in returned labels: %lu != %lu", classification.labels.size(), num_rows);
        for(size_t i = 0; i < classification.labels.size(); i++) // class_ph and labels needs to be populated for kd and surface roughness algorithms
        {
            p[i].class_ph = classification.labels[i];
            p[i].label    = static_cast<ATL24::photon::Label>(classification.labels[i]);
        }

        // generate sea surface elevation
        const vector<ATL24::elevations::Elevations> elevations = ATL24::elevations::get_elevations (p, elevations_params);
        if(elevations.size() != num_rows) throw RunTimeException(CRITICAL, RTE_FAILURE, "size mismatch in returned elevations: %lu != %lu", elevations.size(), num_rows);

        // estimate kd
        const vector<double> kd_estimates = ATL24::estimate_kd::classify (p, estimate_kd_params);
        if(kd_estimates.size() != num_rows) throw RunTimeException(CRITICAL, RTE_FAILURE, "size mismatch in returned kd estimates: %lu != %lu", kd_estimates.size(), num_rows);

        // estimate surface roughness
        const vector<double> estimated_surface_roughness = ATL24::estimate_surface_roughness::classify (p, estimate_surface_roughness_params);
        if(estimated_surface_roughness.size() != num_rows) throw RunTimeException(CRITICAL, RTE_FAILURE, "size mismatch in returned estimated surface roughness: %lu != %lu", estimated_surface_roughness.size(), num_rows);

        // EXIT conditional serialized execution
        if(serialize) experiment.unlock();

        // update new dataframe columns
        for(size_t i=0; i < num_rows; i++)
        {
            class_ph->append(classification.labels[i]);
            confidence->append(classification.probabilities[i][ATL24::labeling::label_map.at(static_cast<int>(ATL24::photon::Label::bathy))]);
            surface_h->append(static_cast<float>(elevations[i].sea_surface_elevation));
            kd->append(static_cast<float>(kd_estimates[i]));
            surface_roughness->append(static_cast<float>(estimated_surface_roughness[i]));
        }

        // status of completion
        mlog(INFO, "Finished classifier on spot %d with %lu rows", df.spot.value, num_rows);

    }
    catch(const std::exception& e)
    {
        status = false;
        mlog(CRITICAL, "Failed to run classifier on %s spot %d: %s", df.granule.value.c_str(), df.spot.value, e.what());
    }

    // add columns to dataframe
    df.addExistingColumn("class_ph",            class_ph,           "photon classification");
    df.addExistingColumn("confidence",          confidence,         "bathymetry classification probability");
    df.addExistingColumn("surface_h",           surface_h,          "surface elevation");
    df.addExistingColumn("kd",                  kd,                 "turbidity");
    df.addExistingColumn("surface_roughness",   surface_roughness,  "surface roughness");

    // return success
    return status;
}
