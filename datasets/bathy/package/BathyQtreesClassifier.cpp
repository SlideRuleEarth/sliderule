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

#include <ATL24_qtrees/qtrees.h>

#include "BathyQtreesClassifier.h"
#include "ContainerRunner.h"
#include "BathyFields.h"
#include "GeoDataFrame.h"
#include "FieldColumn.h"
#include "FieldArray.h"

using namespace std;
using namespace ATL24_qtrees;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyQtreesClassifier::LUA_META_NAME = "BathyQtreesClassifier";
const struct luaL_Reg BathyQtreesClassifier::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(parms)
 *----------------------------------------------------------------------------*/
int BathyQtreesClassifier::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;
    try
    {
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        return createLuaObject(L, new BathyQtreesClassifier(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyQtreesClassifier: %s", e.what());
        if(_parms) _parms->releaseLuaObject();
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyQtreesClassifier::BathyQtreesClassifier (lua_State* L, BathyFields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
    assert(parms);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyQtreesClassifier::~BathyQtreesClassifier (void)
{
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyQtreesClassifier::run (GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();

    try
    {
        const QtreesFields& args = parms->qtrees;
        const FieldColumn<double>& x_atc = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumn("x_atc"));
        const FieldColumn<float>& geoid_corr_h = *dynamic_cast<FieldColumn<float>*>(dataframe->getColumn("geoid_corr_h"));
        FieldColumn<int8_t>& class_ph = *dynamic_cast<FieldColumn<int8_t>*>(dataframe->getColumn("class_ph"));
        FieldColumn<float>& surface_h = *dynamic_cast<FieldColumn<float>*>(dataframe->getColumn("surface_h"));
        FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>& predictions = *dynamic_cast<FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>*>(dataframe->getColumn("predictions"));

        // Preallocate samples vector
        const size_t number_of_samples = dataframe->length();
        vector<utils::sample> samples;
        samples.reserve(number_of_samples);
        mlog(INFO, "Building %ld photon samples", number_of_samples);

        // Build and add samples
        for(size_t i = 0; i < number_of_samples; i++)
        {
            // Populate sample
            const utils::sample s = {
                .dataset_id = 0,
                .h5_index = 0,
                .x = x_atc[i],
                .z = geoid_corr_h[i],
                .cls = 0,
                .prediction = BathyFields::UNCLASSIFIED,
                .surface_elevation = 0.0,
                .bathy_elevation = 0.0
            };
            samples.push_back(s);

            // Clear classification (if necessary)
            if(args.setClass)
            {
                class_ph[i] = BathyFields::UNCLASSIFIED;
            }
        }

        // Run classification
        const string model_filename = FString("%s/%s", ContainerRunner::HOST_DIRECTORY, args.model.value.c_str()).c_str();
        const auto results = classify(args.verbose, samples, model_filename);

        // Update extents
        for(size_t i = 0; i < number_of_samples; i++)
        {
            if(args.setSurface) surface_h[i] = results[i].surface_elevation;
            if(args.setClass) class_ph[i] = results[i].prediction;
            predictions[i][BathyFields::QTREES] = results[i].prediction;
        }
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to run qtrees classifier: %s", e.what());
        return false;
    }

    updateRunTime(TimeLib::latchtime() - start);
    return true;
}
