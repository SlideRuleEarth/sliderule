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

#include "ContainerRunner.h"
#include "FieldColumn.h"
#include "FieldArray.h"
#include "BathyCoastnetClassifier.h"
#include "BathyFields.h"

using namespace std;
using namespace ATL24_coastnet;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyCoastnetClassifier::LUA_META_NAME = "BathyCoastnetClassifier";
const struct luaL_Reg BathyCoastnetClassifier::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(parms)
 *----------------------------------------------------------------------------*/
int BathyCoastnetClassifier::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;
    try
    {
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        return createLuaObject(L, new BathyCoastnetClassifier(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyCoastnetClassifier: %s", e.what());
        if(_parms) _parms->releaseLuaObject();
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyCoastnetClassifier::BathyCoastnetClassifier (lua_State* L, BathyFields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
    assert(parms);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyCoastnetClassifier::~BathyCoastnetClassifier (void)
{
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyCoastnetClassifier::run (GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();

    try
    {
        const CoastnetFields& cparms = parms->coastnet;
        const FieldColumn<double>& x_atc = *dynamic_cast<FieldColumn<double>*>(dataframe->getColumnData("x_atc"));
        const FieldColumn<float>& geoid_corr_h = *dynamic_cast<FieldColumn<float>*>(dataframe->getColumnData("geoid_corr_h"));
        FieldColumn<int8_t>& class_ph = *dynamic_cast<FieldColumn<int8_t>*>(dataframe->getColumnData("class_ph"));
        FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>& predictions = *dynamic_cast<FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>>*>(dataframe->getColumnData("predictions"));
        FieldColumn<uint32_t>& processing_flags = *dynamic_cast<FieldColumn<uint32_t>*>(dataframe->getColumnData("processing_flags"));

        // Preallocate samples and predictions vector
        const size_t number_of_samples = dataframe->length();
        vector<ATL24_coastnet::classified_point2d> samples;
        samples.reserve(number_of_samples);
        mlog(INFO, "Building %ld photon samples", number_of_samples);

        // Build and add samples
        for(size_t i = 0; i < number_of_samples; i++)
        {
            // Populate sample
            const ATL24_coastnet::classified_point2d p = {
                .h5_index = 0,
                .x = x_atc[i],
                .z = geoid_corr_h[i],
                .cls = static_cast<size_t>(class_ph[i]),
                .prediction = BathyFields::UNCLASSIFIED,
                .surface_elevation = 0.0,
                .bathy_elevation = 0.0
            };
            samples.push_back(p);

            // Clear classification (if necessary)
            if(cparms.setClass)
            {
                class_ph[i] = BathyFields::UNCLASSIFIED;
            }
        }

        // Build arguments
        struct cmd::args args;
        args.verbose = cparms.verbose.value;
        args.use_predictions = cparms.usePredictions.value;
        args.model_filename = FString("%s/%s", ContainerRunner::HOST_DIRECTORY, cparms.model.value.c_str()).c_str();

        // Run classification
        const auto results = classify (samples, args);

        // Update extents
        for(size_t i = 0; i < number_of_samples; i++)
        {
            if(cparms.setClass) class_ph[i] = results[i].prediction;
            predictions[i][BathyFields::COASTNET] = results[i].prediction;
            if(results[i].prediction == BathyFields::BATHYMETRY)
            {
                processing_flags[i] = processing_flags[i] | BathyFields::BATHY_COASTNET;
            }
        }
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to run coastnet classifier: %s", e.what());
        return false;
    }

    updateRunTime(TimeLib::latchtime() - start);
    return true;
}

// NOLINTEND
