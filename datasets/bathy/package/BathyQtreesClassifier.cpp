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

#include QTREES_INCLUDE

#include "BathyQtreesClassifier.h"
#include "BathyFields.h"
#include "GeoDataFrame.h"
#include "FieldColumn.h"
#include "FieldArray.h"

using namespace std;
using namespace ATL24_qtrees;

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* BathyQtreesClassifier::CLASSIFIER_NAME = "qtrees";
const char* BathyQtreesClassifier::QTREES_PARMS = "qtrees";
const char* BathyQtreesClassifier::DEFAULT_QTREES_MODEL = "/data/qtrees_model-20240607.json";

static const char* QTREES_PARM_MODEL = "model";
static const char* QTREES_PARM_SET_CLASS = "set_class";
static const char* QTREES_PARM_SET_SURFACE = "set_surface";
static const char* QTREES_PARM_VERBOSE = "verbose";

const char* BathyQtreesClassifier::LUA_META_NAME = "BathyQtreesClassifier";
const struct luaL_Reg BathyQtreesClassifier::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(parms)
 *----------------------------------------------------------------------------*/
int BathyQtreesClassifier::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new BathyQtreesClassifier(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyQtreesClassifier: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyQtreesClassifier::BathyQtreesClassifier (lua_State* L, int index):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE)
{
    /* Build Parameters */
    if(lua_istable(L, index))
    {
        /* model */
        lua_getfield(L, index, QTREES_PARM_MODEL);
        parms.model = LuaObject::getLuaString(L, -1, true, parms.model.c_str());
        lua_pop(L, 1);

        /* set class */
        lua_getfield(L, index, QTREES_PARM_SET_CLASS);
        parms.set_class = LuaObject::getLuaBoolean(L, -1, true, parms.set_class);
        lua_pop(L, 1);

        /* set surface */
        lua_getfield(L, index, QTREES_PARM_SET_SURFACE);
        parms.set_surface = LuaObject::getLuaBoolean(L, -1, true, parms.set_surface);
        lua_pop(L, 1);

        /* verbose */
        lua_getfield(L, index, QTREES_PARM_VERBOSE);
        parms.verbose = LuaObject::getLuaBoolean(L, -1, true, parms.verbose);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyQtreesClassifier::run (GeoDataFrame* dataframe)
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
        std::vector<utils::sample> samples;
        samples.reserve(number_of_samples);
        mlog(INFO, "Building %ld photon samples", number_of_samples);

        // Build and add samples
        for(size_t i = 0; i < number_of_samples; i++)
        {
            // Populate sample
            utils::sample s = {
                .dataset_id = 0,
                .h5_index = 0,
                .x = x_atc[i],
                .z = ortho_h[i],
                .cls = 0,
                .prediction = BathyFields::UNCLASSIFIED,
                .surface_elevation = 0.0,
                .bathy_elevation = 0.0
            };
            samples.push_back(s);

            // Clear classification (if necessary)
            if(parms.set_class)
            {
                class_ph[i] = BathyFields::UNCLASSIFIED;
            }
        }

        // Build arguments
        struct cmd::args args;
        args.verbose = parms.verbose;
        args.model_filename = parms.model;

        // Run classification
        classify(args, samples);

        // Update extents
        for(size_t i = 0; i < number_of_samples; i++)
        {
            if(parms.set_surface) surface_h[i] = samples[i].surface_elevation;
            if(parms.set_class) class_ph[i] = samples[i].prediction;
            predictions[i][BathyFields::QTREES] = samples[i].prediction;
        }
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to run qtrees classifier: %s", e.what());
        return false;
    }

    return true;
}

// NOLINTEND