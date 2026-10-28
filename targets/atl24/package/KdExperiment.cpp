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

#include "kd_experiment.h" // ATL24

#include "OsApi.h"
#include "TimeLib.h"
#include "FieldElement.h"
#include "KdExperiment.h"
#include "Icesat2Parameters.h"
#include "BathyDataFrame.h"
#include "BathyKd.h"

using namespace ATL24::kd_experiment;
using namespace ATL24::photon;

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* KdExperiment::LUA_META_NAME = "KdExperiment";
const struct luaL_Reg KdExperiment::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int KdExperiment::luaCreate (lua_State* L)
{
    Icesat2Parameters* _parms = NULL;
    BathyKd* _kd = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Parameters*>(getLuaObject(L, 1, Icesat2Parameters::OBJECT_TYPE));
        _kd = dynamic_cast<BathyKd*>(getLuaObject(L, 2, BathyKd::OBJECT_TYPE));
        const long _serialize_threshold = getLuaInteger(L, 3, true, DEFAULT_SERIALIZE_THRESHOLD);
        return createLuaObject(L, new KdExperiment(L, _parms, _kd, _serialize_threshold));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_kd) _kd->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
KdExperiment::KdExperiment (lua_State* L, Icesat2Parameters* _parms, BathyKd* _kd, long _serialize_threshold):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    viirsKd(_kd),
    serializeThreshold(_serialize_threshold)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
KdExperiment::~KdExperiment (void)
{
    if(parms) parms->releaseLuaObject();
    if(viirsKd) viirsKd->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool KdExperiment::run (GeoDataFrame* dataframe)
{
    bool status = true;

    // cast dataframe to ATL24 specific dataframe
    BathyDataFrame& df = *dynamic_cast<BathyDataFrame*>(dataframe);

    // create new columns
    FieldColumn<int>*                       class_ph    = new FieldColumn<int>;
    FieldColumn<double>*                    viirs_kd    = new FieldColumn<double>;
    FieldColumn<FieldArray<double,NUM_KD>>* kd          = new FieldColumn<FieldArray<double,NUM_KD>>;
    FieldColumn<FieldArray<double,NUM_SR>>* sr          = new FieldColumn<FieldArray<double,NUM_SR>>;

    // determine serialization
    const bool serialize = df.length() > serializeThreshold;
    mlog(INFO, "Running Kd experiment on spot %d in %s mode", df.spot.value, serialize ? "serial" : "parallel");

    try
    {
        // convert dataframe to algorithm input structure
        vector<Photon> p(df.length());
        for(size_t i = 0; i < static_cast<size_t>(df.length()); ++i)
        {
            // only the below members of the structure are used
            p[i].gps_seconds    = TimeLib::sysex2gpstime(df.time_ns[i]);
            p[i].lat_ph         = df.lat_ph[i];
            p[i].lon_ph         = df.lon_ph[i];
            p[i].x_atc          = df.x_atc[i];
            p[i].h_ph           = df.ellipse_h[i];
            p[i].geoid          = df.ellipse_h[i] - df.geoid_corr_h[i];
            p[i].quality_ph     = df.quality_ph[i];
            p[i].spot           = df.spot.value;
        }

        // execute Kd Experiment
        if(serialize) experiment.lock();
        FString model_filename("%s/atl24.tgz", CONFDIR);
        vector<Kd_experiment_Photon> results = run_experiment(p, model_filename.c_str());
        if(serialize) experiment.unlock();
        for(const Kd_experiment_Photon& kd_photon: results)
        {
            // add class_ph
            class_ph->append(kd_photon.class_ph);

            // add kd
            FieldArray<double,NUM_KD> kd_row;
            kd_row[0] = kd_photon.kd1;
            kd_row[1] = kd_photon.kd2;
            kd_row[2] = kd_photon.kd3;
            kd_row[3] = kd_photon.kd4;
            kd_row[4] = kd_photon.kd5;
            kd_row[5] = kd_photon.kd6;
            kd_row[6] = kd_photon.kd7;
            kd_row[7] = kd_photon.kd8;
            kd_row[8] = kd_photon.kd9;
            kd_row[9] = kd_photon.kd10;
            kd_row[10] = kd_photon.kd11;
            kd_row[11] = kd_photon.kd12;
            kd_row[12] = kd_photon.kd13;
            kd_row[13] = kd_photon.kd14;
            kd_row[14] = kd_photon.kd15;
            kd->append(kd_row);

            // add sr
            FieldArray<double,NUM_SR> sr_row;
            sr_row[0] = kd_photon.sr1;
            sr_row[1] = kd_photon.sr2;
            sr_row[2] = kd_photon.sr3;
            sr_row[3] = kd_photon.sr4;
            sr->append(sr_row);
        }
    }
    catch(const std::exception& e)
    {
        status = false;
        mlog(CRITICAL, "Failed to run kd experiement on %s spot %d: %s", df.granule.value.c_str(), df.spot.value, e.what());
    }

    // add viirs kd
    viirsKd->join(parms->readTimeout.value * 1000);
    for(size_t i = 0; i < static_cast<size_t>(df.length()); i++)
    {
        viirs_kd->append(viirsKd->getKd(df.lon_ph[i], df.lat_ph[i]));
    }

    // add columns to dataframe
    df.addExistingColumn("class_ph",    class_ph,   "photon classification");
    df.addExistingColumn("viirs_kd",    viirs_kd,   "kd490 from VIIRS");
    df.addExistingColumn("kd",          kd,         "turbidity");
    df.addExistingColumn("sr",          sr,         "sr");

    // return success
    return status;
}
