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
 * DATA
 ******************************************************************************/

const char* BathyOpenOceansPPClassifier::CLASSIFIER_NAME = "openoceanspp";
const char* BathyOpenOceansPPClassifier::OPENOCEANSPP_PARMS = "openoceanspp";

static const char* OPENOCEANSPP_PARM_SET_CLASS = "set_class";
static const char* OPENOCEANSPP_PARM_SET_SURFACE = "set_surface";
static const char* OPENOCEANSPP_PARM_USE_PREDICTIONS = "use_predictions";
static const char* OPENOCEANSPP_PARM_VERBOSE = "verbose";
static const char* OPENOCEANSPP_PARM_X_RESOLUTION = "x_resolution";
static const char* OPENOCEANSPP_PARM_Z_RESOLUTION = "z_resolution";
static const char* OPENOCEANSPP_PARM_Z_MIN = "z_min";
static const char* OPENOCEANSPP_PARM_Z_MAX = "z_max";
static const char* OPENOCEANSPP_PARM_SURFACE_Z_MIN = "surface_z_min";
static const char* OPENOCEANSPP_PARM_SURFACE_Z_MAX = "surface_z_max";
static const char* OPENOCEANSPP_PARM_BATHY_MIN_DEPTH = "bathy_min_depth";
static const char* OPENOCEANSPP_PARM_VERTICAL_SMOOTHING_SIGMA = "vertical_smoothing_sigma";
static const char* OPENOCEANSPP_PARM_SURFACE_SMOOTHING_SIGMA = "surface_smoothing_sigma";
static const char* OPENOCEANSPP_PARM_BATHY_SMOOTHING_SIGMA = "bathy_smoothing_sigma";
static const char* OPENOCEANSPP_PARM_MIN_PEAK_PROMINENCE = "min_peak_prominence";
static const char* OPENOCEANSPP_PARM_MIN_PEAK_DISTANCE = "min_peak_distance";
static const char* OPENOCEANSPP_PARM_MIN_SURFACE_PHOTONS_PER_WINDOW = "min_surface_photons_per_window";
static const char* OPENOCEANSPP_PARM_MIN_BATHY_PHOTONS_PER_WINDOW = "min_bathy_photons_per_window";

const char* BathyOpenOceansPPClassifier::LUA_META_NAME = "BathyOpenOceansPPClassifier";
const struct luaL_Reg BathyOpenOceansPPClassifier::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(parms)
 *----------------------------------------------------------------------------*/
int BathyOpenOceansPPClassifier::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new BathyOpenOceansPPClassifier(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyOpenOceansPPClassifier: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyOpenOceansPPClassifier::BathyOpenOceansPPClassifier (lua_State* L, int index):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE)
{
    /* Build Parameters */
    if(lua_istable(L, index))
    {
        /* set class */
        lua_getfield(L, index, OPENOCEANSPP_PARM_SET_CLASS);
        parms.set_class = LuaObject::getLuaBoolean(L, -1, true, parms.set_class);
        lua_pop(L, 1);

        /* set surface */
        lua_getfield(L, index, OPENOCEANSPP_PARM_SET_SURFACE);
        parms.set_surface = LuaObject::getLuaBoolean(L, -1, true, parms.set_surface);
        lua_pop(L, 1);

        /* use predictions */
        lua_getfield(L, index, OPENOCEANSPP_PARM_USE_PREDICTIONS);
        parms.use_predictions = LuaObject::getLuaBoolean(L, -1, true, parms.use_predictions);
        lua_pop(L, 1);

        /* verbose */
        lua_getfield(L, index, OPENOCEANSPP_PARM_VERBOSE);
        parms.verbose = LuaObject::getLuaBoolean(L, -1, true, parms.verbose);
        lua_pop(L, 1);

        /* x_resolution */
        lua_getfield(L, index, OPENOCEANSPP_PARM_X_RESOLUTION);
        parms.x_resolution = LuaObject::getLuaFloat(L, -1, true, parms.x_resolution);
        lua_pop(L, 1);

        /* y_resolution */
        lua_getfield(L, index, OPENOCEANSPP_PARM_Z_RESOLUTION);
        parms.z_resolution = LuaObject::getLuaFloat(L, -1, true, parms.z_resolution);
        lua_pop(L, 1);

        /* z_min */
        lua_getfield(L, index, OPENOCEANSPP_PARM_Z_MIN);
        parms.z_min = LuaObject::getLuaFloat(L, -1, true, parms.z_min);
        lua_pop(L, 1);

        /* z_max */
        lua_getfield(L, index, OPENOCEANSPP_PARM_Z_MAX);
        parms.z_max = LuaObject::getLuaFloat(L, -1, true, parms.z_max);
        lua_pop(L, 1);

        /* surface_z_min */
        lua_getfield(L, index, OPENOCEANSPP_PARM_SURFACE_Z_MIN);
        parms.surface_z_min = LuaObject::getLuaFloat(L, -1, true, parms.surface_z_min);
        lua_pop(L, 1);

        /* surface_z_max */
        lua_getfield(L, index, OPENOCEANSPP_PARM_SURFACE_Z_MAX);
        parms.surface_z_max = LuaObject::getLuaFloat(L, -1, true, parms.surface_z_max);
        lua_pop(L, 1);

        /* bathy_min_depth */
        lua_getfield(L, index, OPENOCEANSPP_PARM_BATHY_MIN_DEPTH);
        parms.bathy_min_depth = LuaObject::getLuaFloat(L, -1, true, parms.bathy_min_depth);
        lua_pop(L, 1);

        /* vertical_smoothing_sigma */
        lua_getfield(L, index, OPENOCEANSPP_PARM_VERTICAL_SMOOTHING_SIGMA);
        parms.vertical_smoothing_sigma = LuaObject::getLuaFloat(L, -1, true, parms.vertical_smoothing_sigma);
        lua_pop(L, 1);

        /* surface_smoothing_sigma */
        lua_getfield(L, index, OPENOCEANSPP_PARM_SURFACE_SMOOTHING_SIGMA);
        parms.surface_smoothing_sigma = LuaObject::getLuaFloat(L, -1, true, parms.surface_smoothing_sigma);
        lua_pop(L, 1);

        /* surface_smoothing_sigma */
        lua_getfield(L, index, OPENOCEANSPP_PARM_BATHY_SMOOTHING_SIGMA);
        parms.bathy_smoothing_sigma = LuaObject::getLuaFloat(L, -1, true, parms.bathy_smoothing_sigma);
        lua_pop(L, 1);

        /* min_peak_prominence */
        lua_getfield(L, index, OPENOCEANSPP_PARM_MIN_PEAK_PROMINENCE);
        parms.min_peak_prominence = LuaObject::getLuaFloat(L, -1, true, parms.min_peak_prominence);
        lua_pop(L, 1);

        /* min_peak_distance */
        lua_getfield(L, index, OPENOCEANSPP_PARM_MIN_PEAK_DISTANCE);
        parms.min_peak_distance = LuaObject::getLuaInteger(L, -1, true, parms.min_peak_distance);
        lua_pop(L, 1);

        /* min_surface_photons_per_window */
        lua_getfield(L, index, OPENOCEANSPP_PARM_MIN_SURFACE_PHOTONS_PER_WINDOW);
        parms.min_surface_photons_per_window = LuaObject::getLuaInteger(L, -1, true, parms.min_surface_photons_per_window);
        lua_pop(L, 1);

        /* min_bathy_photons_per_window */
        lua_getfield(L, index, OPENOCEANSPP_PARM_MIN_BATHY_PHOTONS_PER_WINDOW);
        parms.min_bathy_photons_per_window = LuaObject::getLuaInteger(L, -1, true, parms.min_bathy_photons_per_window);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyOpenOceansPPClassifier::run (GeoDataFrame* dataframe)
{
    try
    {
        const FieldColumn<double>& x_atc = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("x_atc"));
        const FieldColumn<double>& ortho_h = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("ortho_h"));
        FieldColumn<int8_t>& class_ph = *dynamic_cast<FieldColumn<int8_t>*>(dataframe->getColumnData("class_ph"));
        FieldColumn<float>& surface_h = *dynamic_cast<FieldColumn<float>*>(dataframe->getColumnData("surface_h"));
        FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>& predictions = *dynamic_cast<FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>*>(dataframe->getColumnData("predictions"));

        // Preallocate samples vector
        const size_t number_of_samples = dataframe->length();
        std::vector<oopp::photon> samples;
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
            if(parms.set_class)
            {
                class_ph[i] = BathyFields::UNCLASSIFIED;
            }
        }

        // Initialize Parameters
        oopp::params params = {
            .x_resolution = parms.x_resolution,
            .z_resolution = parms.z_resolution,
            .z_min = parms.z_min,
            .z_max = parms.z_max,
            .surface_z_min = parms.surface_z_min,
            .surface_z_max = parms.surface_z_max,
            .bathy_min_depth = parms.bathy_min_depth,
            .vertical_smoothing_sigma = parms.vertical_smoothing_sigma,
            .surface_smoothing_sigma = parms.surface_smoothing_sigma,
            .bathy_smoothing_sigma = parms.bathy_smoothing_sigma,
            .min_peak_prominence = parms.min_peak_prominence,
            .min_peak_distance = parms.min_peak_distance,
            .min_surface_photons_per_window = parms.min_surface_photons_per_window,
            .min_bathy_photons_per_window = parms.min_bathy_photons_per_window,
            .surface_n_stddev = 3.0,
            .bathy_n_stddev = 3.0
        }; 

        // Run classification
        samples = classify (samples, params, parms.use_predictions);

        // Update extents
        for(size_t i = 0; i < number_of_samples; i++)
        {
            if(parms.set_surface) surface_h[i] = samples[i].surface_elevation;
            if(parms.set_class) class_ph[i] = samples[i].prediction;
            predictions[i][BathyFields::OPENOCEANSPP] = samples[i].prediction;
        }
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to run openoceanspp classifier: %s", e.what());
        return false;
    }

    return true;
}

// NOLINTEND