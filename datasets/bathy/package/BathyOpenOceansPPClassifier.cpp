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

// NOLINTBEGIN

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include OPENOCEANSPP_INCLUDE

#include "BathyOpenOceansPPClassifier.h"
#include "BathyFields.h"
#include "GeoDataFrame.h"
#include "FieldArray.h"
#include "FieldColumn.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyOpenOceansPPClassifier::LUA_META_NAME = "BathyOpenOceansPPClassifier";
const struct luaL_Reg BathyOpenOceansPPClassifier::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(parms)
 *----------------------------------------------------------------------------*/
int BathyOpenOceansPPClassifier::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;
    try
    {
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        return createLuaObject(L, new BathyOpenOceansPPClassifier(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyOpenOceansPPClassifier: %s", e.what());
        if(_parms) _parms->releaseLuaObject();
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyOpenOceansPPClassifier::BathyOpenOceansPPClassifier (lua_State* L, BathyFields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
    assert(parms);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyOpenOceansPPClassifier::~BathyOpenOceansPPClassifier (void)
{
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyOpenOceansPPClassifier::run (GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();

    try
    {
        const OpenOceansPPFields& oparms = parms->openoceanspp;
        const FieldColumn<double>& x_atc = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("x_atc"));
        const FieldColumn<double>& ortho_h = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("ortho_h"));
        FieldColumn<int8_t>& class_ph = *dynamic_cast<FieldColumn<int8_t>*>(dataframe->getColumnData("class_ph"));
        FieldColumn<float>& surface_h = *dynamic_cast<FieldColumn<float>*>(dataframe->getColumnData("surface_h"));
        FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>& predictions = *dynamic_cast<FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>*>(dataframe->getColumnData("predictions"));

        // Preallocate samples vector
        const size_t number_of_samples = dataframe->length();
        vector<oopp::photon> samples;
        samples.reserve(number_of_samples);
        mlog(INFO, "Building %ld photon samples", number_of_samples);

        // Build and add samples
        for(size_t i = 0; i < number_of_samples; i++)
        {
            // Populate photon
            oopp::photon photon = {
                .h5_index = 0,
                .x = x_atc[i],
                .z = ortho_h[i],
                .cls = 0,
                .prediction = static_cast<unsigned>(class_ph[i]),
                .surface_elevation = 0.0,
                .bathy_elevation = 0.0
            };
            samples.push_back(photon);

            // Clear classification (if necessary)
            if(oparms.setClass)
            {
                class_ph[i] = BathyFields::UNCLASSIFIED;
            }
        }

        // Initialize Parameters
        oopp::params params = {
            .x_resolution = oparms.xResolution.value,
            .z_resolution = oparms.zResolution.value,
            .z_min = oparms.zMin.value,
            .z_max = oparms.zMax.value,
            .surface_z_min = oparms.surfaceZMin.value,
            .surface_z_max = oparms.surfaceZMax.value,
            .bathy_min_depth = oparms.bathyMinDepth.value,
            .vertical_smoothing_sigma = oparms.verticalSmoothingSigma.value,
            .surface_smoothing_sigma = oparms.surfaceSmoothingSigma.value,
            .bathy_smoothing_sigma = oparms.bathySmoothingSigma.value,
            .min_peak_prominence = oparms.minPeakProminence.value,
            .min_peak_distance = oparms.minPeakDistance.value,
            .min_surface_photons_per_window = oparms.minSurfacePhotonsPerWindow,
            .min_bathy_photons_per_window = oparms.minBathyPhotonsPerWindow,
            .surface_n_stddev = 3.0,
            .bathy_n_stddev = 3.0
        }; 

        // Run classification
        samples = classify (samples, params, oparms.usePredictions);

        // Update extents
        for(size_t i = 0; i < number_of_samples; i++)
        {
            if(oparms.setSurface) surface_h[i] = samples[i].surface_elevation;
            if(oparms.setClass) class_ph[i] = samples[i].prediction;
            predictions[i][BathyFields::OPENOCEANSPP] = samples[i].prediction;
        }
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to run openoceanspp classifier: %s", e.what());
        return false;
    }

    updateRunTime(TimeLib::latchtime() - start);
    return true;
}

// NOLINTEND