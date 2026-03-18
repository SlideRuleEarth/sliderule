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


#include "OsApi.h"
#include "H5CoroLib.h"
#include "H5VarSet.h"
#include "H5Object.h"
#include "H5Array.h"
#include "Atl09Sampler.h"
#include "Icesat2Fields.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* Atl09Sampler::LUA_META_NAME = "Atl09Sampler";
const struct luaL_Reg Atl09Sampler::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * TYPES
 ******************************************************************************/

typedef struct {
    long startrow;
    long numrows;
} range_to_sample_t;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

static range_to_sample_t get_time_range(H5Array<double>& delta_time, const time8_t& min_time, const time8_t& max_time, int timeout_ms)
{
    range_to_sample_t range_to_sample = {
        .startrow = 0,
        .numrows = 1
    };

    if(delta_time.h5f)
    {
        delta_time.join(timeout_ms, true);
        for(long i = 0; i < delta_time.size; i++)
        {
            time8_t t = Icesat2Fields::deltatime2timestamp(delta_time[i]);
            if(t.nanoseconds < min_time.nanoseconds) range_to_sample.startrow = i;
            else if(t.nanoseconds <= max_time.nanoseconds) range_to_sample.numrows++;
        }
    }

    return range_to_sample;
}

static void sample_data(GeoDataFrame* dataframe, H5Array<double>& delta_time, H5VarSet& data, FieldColumn<time8_t>& time_column, const range_to_sample_t& range_to_sample, int timeout_ms)
{
    if(delta_time.h5f)
    {
        // wait for data to finish being read
        data.joinToGDF(dataframe, timeout_ms, true);

        // find and append best sample for each variable
        long group_index = range_to_sample.startrow;
        for(long dataframe_index = 0; dataframe_index < time_column.length(); dataframe_index++)
        {
            time8_t dataframe_time = time_column[dataframe_index];
            while((group_index < delta_time.size) &&
                    (Icesat2Fields::deltatime2timestamp(delta_time[group_index]).nanoseconds < dataframe_time.nanoseconds))
                group_index++;
            data.addToGDF(dataframe, group_index - range_to_sample.startrow);
        }
    }
}

 /******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int Atl09Sampler::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;
    H5Object* _hdf09 = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 1, Icesat2Fields::OBJECT_TYPE));
        _hdf09 = dynamic_cast<H5Object*>(getLuaObject(L, 2, H5Object::OBJECT_TYPE));
        return createLuaObject(L, new Atl09Sampler(L, _parms, _hdf09));
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
Atl09Sampler::Atl09Sampler (lua_State* L, Icesat2Fields* _parms, H5Object* _hdf09):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    hdf09(_hdf09)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl09Sampler::~Atl09Sampler (void)
{
    if(parms) parms->releaseLuaObject();
    if(hdf09) hdf09->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool Atl09Sampler::run (GeoDataFrame* dataframe)
{
    try
    {
        // get spot metadata
        FieldElement<uint8_t>* spot = reinterpret_cast<FieldElement<uint8_t>*>(dataframe->getMetaData("spot"));
        int profile_num = ((spot->value - 1) / 2) + 1;
        string profile = FString("profile_%d", profile_num).c_str();

        // get time column
        FieldColumn<time8_t>* time_column = reinterpret_cast<FieldColumn<time8_t>*>(dataframe->getColumn("time_ns"));
        if(!time_column)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to find time column");
        }
        else if(time_column->length() <= 0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "time column empty");
        }

        // get minimum and maximum times
        time8_t min_time = (*time_column)[0];
        time8_t max_time = (*time_column)[0];
        for(long i = 1; i < time_column->length(); i++)
        {
            time8_t row_time = (*time_column)[i];
            if(row_time.nanoseconds < min_time.nanoseconds) min_time = row_time;
            else if(row_time.nanoseconds > max_time.nanoseconds) max_time = row_time;
        }

        // separate fields lists into groups
        FieldList<string> bckgrd_atlas_fields;
        FieldList<string> high_rate_fields;
        FieldList<string> low_rate_fields;
        for(int i = 0; i < parms->atl09Fields.length(); i++)
        {
            const string& field = parms->atl09Fields[i];
            if(field.find("bckgrd_atlas") != std::string::npos) bckgrd_atlas_fields.append(field);
            else if(field.find("high_rate") != std::string::npos) high_rate_fields.append(field);
            else if(field.find("low_rate") != std::string::npos) low_rate_fields.append(field);
        }

        // get time variables for each group
        H5Array<double> bckgrd_atlas_delta_time(bckgrd_atlas_fields.length() > 0 ? hdf09 : NULL, FString("%s/bckgrd_atlas/delta_time", profile.c_str()).c_str(), 0, 0, H5Coro::ALL_ROWS);
        H5Array<double> high_rate_delta_time(high_rate_fields.length() > 0 ? hdf09 : NULL, FString("%s/high_rate/delta_time", profile.c_str()).c_str(), 0, 0, H5Coro::ALL_ROWS);
        H5Array<double> low_rate_delta_time(low_rate_fields.length() > 0 ? hdf09 : NULL, FString("%s/low_rate/delta_time", profile.c_str()).c_str(), 0, 0, H5Coro::ALL_ROWS);

        // get time range for each group
        range_to_sample_t bckgrd_atlas_range_to_sample = get_time_range(bckgrd_atlas_delta_time, min_time, max_time, parms->timeout.value * 1000);
        range_to_sample_t high_rate_range_to_sample = get_time_range(high_rate_delta_time, min_time, max_time, parms->timeout.value * 1000);
        range_to_sample_t low_rate_range_to_sample = get_time_range(low_rate_delta_time, min_time, max_time, parms->timeout.value * 1000);

        // get request data variables for each group
        H5VarSet bckgrd_atlas_data(bckgrd_atlas_fields, hdf09, profile.c_str(), 0, bckgrd_atlas_range_to_sample.startrow, bckgrd_atlas_range_to_sample.numrows);
        H5VarSet high_rate_data(high_rate_fields, hdf09, profile.c_str(), 0, high_rate_range_to_sample.startrow, high_rate_range_to_sample.numrows);
        H5VarSet low_rate_data(low_rate_fields, hdf09, profile.c_str(), 0, low_rate_range_to_sample.startrow, low_rate_range_to_sample.numrows);

        // add data to dataframe from each group
        sample_data(dataframe, bckgrd_atlas_delta_time, bckgrd_atlas_data, (*time_column), bckgrd_atlas_range_to_sample, parms->timeout.value * 1000);
        sample_data(dataframe, high_rate_delta_time, high_rate_data, (*time_column), high_rate_range_to_sample, parms->timeout.value * 1000);
        sample_data(dataframe, low_rate_delta_time, low_rate_data, (*time_column), low_rate_range_to_sample, parms->timeout.value * 1000);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to sample ATL09: %s", e.what());
        return false;
    }

    return true;
}
