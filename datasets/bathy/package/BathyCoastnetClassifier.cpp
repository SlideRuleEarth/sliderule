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

#include COASTNET_INCLUDE

#include "FieldColumn.h"
#include "FieldArray.h"
#include "BathyCoastnetClassifier.h"
#include "BathyFields.h"

using namespace std;
using namespace ATL24_coastnet;

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* BathyCoastnetClassifier::CLASSIFIER_NAME = "coastnet";
const char* BathyCoastnetClassifier::COASTNET_PARMS = "coastnet";
const char* BathyCoastnetClassifier::DEFAULT_COASTNET_MODEL = "/data/coastnet_model-20240628.json";

static const char* COASTNET_PARM_MODEL = "model";
static const char* COASTNET_PARM_SET_CLASS = "set_class";
static const char* COASTNET_PARM_USE_PREDICTIONS = "use_predictions";
static const char* COASTNET_PARM_VERBOSE = "verbose";

const char* BathyCoastnetClassifier::LUA_META_NAME = "BathyCoastnetClassifier";
const struct luaL_Reg BathyCoastnetClassifier::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(parms)
 *----------------------------------------------------------------------------*/
int BathyCoastnetClassifier::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new BathyCoastnetClassifier(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyCoastnetClassifier: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyCoastnetClassifier::BathyCoastnetClassifier (lua_State* L, int index):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE)
{
    /* Build Parameters */
    if(lua_istable(L, index))
    {
        /* model */
        lua_getfield(L, index, COASTNET_PARM_MODEL);
        parms.model = LuaObject::getLuaString(L, -1, true, parms.model.c_str());
        lua_pop(L, 1);

        /* set class */
        lua_getfield(L, index, COASTNET_PARM_SET_CLASS);
        parms.set_class = LuaObject::getLuaBoolean(L, -1, true, parms.set_class);
        lua_pop(L, 1);

        /* use predictions */
        lua_getfield(L, index, COASTNET_PARM_USE_PREDICTIONS);
        parms.use_predictions = LuaObject::getLuaBoolean(L, -1, true, parms.use_predictions);
        lua_pop(L, 1);

        /* verbose */
        lua_getfield(L, index, COASTNET_PARM_VERBOSE);
        parms.verbose = LuaObject::getLuaBoolean(L, -1, true, parms.verbose);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyCoastnetClassifier::run (GeoDataFrame* dataframe)
{
    try
    {
        const FieldColumn<double>& x_atc = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("x_atc"));
        const FieldColumn<double>& ortho_h = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("ortho_h"));
        FieldColumn<int8_t>& class_ph = *dynamic_cast<FieldColumn<int8_t>*>(dataframe->getColumnData("class_ph"));
        FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>& predictions = *dynamic_cast<FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>*>(dataframe->getColumnData("predictions"));

        // Preallocate samples and predictions vector
        const size_t number_of_samples = dataframe->length();
        std::vector<ATL24_coastnet::classified_point2d> samples;
        samples.reserve(number_of_samples);
        mlog(INFO, "Building %ld photon samples", number_of_samples);

        // Build and add samples
        for(size_t i = 0; i < number_of_samples; i++)
        {
            // Populate sample
            const ATL24_coastnet::classified_point2d p = {
                .h5_index = 0,
                .x = x_atc[i],
                .z = ortho_h[i],
                .cls = static_cast<size_t>(class_ph[i]),
                .prediction = BathyFields::UNCLASSIFIED,
                .surface_elevation = 0.0,
                .bathy_elevation = 0.0
            };
            samples.push_back(p);

            // Clear classification (if necessary)
            if(parms.set_class)
            {
                class_ph[i] = BathyFields::UNCLASSIFIED;
            }
        }

        // Build arguments
        struct cmd::args args;
        args.verbose = parms.verbose;
        args.use_predictions = parms.use_predictions;
        args.model_filename = parms.model;

        // Run classification
        const auto results = classify (samples, args);

        // Update extents
        for(size_t i = 0; i < number_of_samples; i++)
        {
            if(parms.set_class) class_ph[i] = results[i].prediction;
            predictions[i][BathyFields::COASTNET] = results[i].prediction;
        }
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to run coastnet classifier: %s", e.what());
        return false;
    }

    return true;
}

// NOLINTEND
