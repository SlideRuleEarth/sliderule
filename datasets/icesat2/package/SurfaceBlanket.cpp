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

#include "OsApi.h"
#include "GeoLib.h"
#include "SurfaceBlanket.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* SurfaceBlanket::LUA_META_NAME = "SurfaceBlanket";
const struct luaL_Reg SurfaceBlanket::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int SurfaceBlanket::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 1, Icesat2Fields::OBJECT_TYPE));
        return createLuaObject(L, new SurfaceBlanket(L, _parms));
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
SurfaceBlanket::SurfaceBlanket (lua_State* L, Icesat2Fields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SurfaceBlanket::~SurfaceBlanket (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool SurfaceBlanket::run (GeoDataFrame* dataframe)
{
    Atl03DataFrame& df = *dynamic_cast<Atl03DataFrame*>(dataframe);

    // create new dataframe columns
    FieldColumn<time8_t>*   time_ns         = new FieldColumn<time8_t>(Field::TIME_COLUMN, 0, "Segment timestamp (Unix ns)");
    FieldColumn<double>*    latitude        = new FieldColumn<double>(Field::Y_COLUMN, 0, "Latitude (degrees)");
    FieldColumn<double>*    longitude       = new FieldColumn<double>(Field::X_COLUMN, 0, "Longitude (degrees)");
    FieldColumn<int32_t>*   segment_id_beg  = new FieldColumn<int32_t>("Starting segment ID");
    FieldColumn<double>*    x_atc           = new FieldColumn<double>("Along-track distance (m)");
    FieldColumn<float>*     y_atc           = new FieldColumn<float>("Across-track distance (m)");
    FieldColumn<float>*     top_of_surface  = new FieldColumn<float>(Field::Z_COLUMN, 0, "Top of reflective surface (m)");
    FieldColumn<float>*     median_ground   = new FieldColumn<float>("Median ground elevation (m)");
    FieldColumn<uint16_t>*  pflags          = new FieldColumn<uint16_t>("Processing flags");

    // create new ancillary dataframe columns
    Dictionary<GeoDataFrame::ancillary_t>* ancillary_columns = NULL;
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03BckgrdFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03GeoFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03CorrFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03PhFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl08Fields);

    // initialize
    double start_distance = 0;
    if(df.length() > 0)
    {
        start_distance = df.x_atc[0];
    }

    // for each photon
    int32_t i0 = 0; // start row
    while(i0 < df.length())
    {
        // initialize result
        result_t result;

        // find end of extent
        int32_t i1 = i0; // end row
        while( (i1 < (df.length() - 1)) &&
               ((df.x_atc[i1 + 1] - start_distance) < parms->extentLength.value) )
        {
            i1++;
        }

        // calculate number of photons in extent
        const int32_t num_photons = i1 - i0 + 1;

        // check minimum number of photons
        if(num_photons < parms->minPhotonCount)
        {
            result.pflags |= Icesat2Fields::PFLAG_TOO_FEW_PHOTONS;
        }

        // add row to surface blanket dataframe
        if(result.pflags == 0 || parms->passInvalid)
        {
            // run algorithm
            algorithm(df, i0, num_photons, result);

            // populate surface blanket columns
            time8_t t(static_cast<int64_t>(df.time_ns.mean(i0, num_photons)));
            time_ns->append(t);
            latitude->append(df.latitude.mean(i0, num_photons));
            longitude->append(df.longitude.mean(i0, num_photons));
            segment_id_beg->append(df.segment_id[i0]);
            x_atc->append(df.x_atc.mean(i0, num_photons));
            y_atc->append(df.y_atc.mean(i0, num_photons));
            top_of_surface->append(result.top_of_surface_h);
            median_ground->append(result.median_ground_h);
            pflags->append(result.pflags);

            // populate ancillary columns
            GeoDataFrame::populateAncillaryColumns(ancillary_columns, df, i0, num_photons);
        }

        while(i0 < df.length())
        {
            // update the start distance
            start_distance += parms->extentStep.value;

            // find first index past new start distance
            while( (i0 < df.length()) && (df.x_atc[i0] < start_distance) )
            {
                i0++;
            }

            // check if this extent has any photons
            if( (i0 < df.length()) && ((df.x_atc[i0] - start_distance) < parms->extentLength.value) )
            {
                break;
            }
        }
    }

    // clear all columns from original dataframe
    dataframe->clear(); // frees memory

    // install new columns into dataframe
    dataframe->addExistingColumn("time_ns",         time_ns);
    dataframe->addExistingColumn("latitude",        latitude);
    dataframe->addExistingColumn("longitude",       longitude);
    dataframe->addExistingColumn("segment_id_beg",  segment_id_beg);
    dataframe->addExistingColumn("x_atc",           x_atc);
    dataframe->addExistingColumn("y_atc",           y_atc);
    dataframe->addExistingColumn("top_of_surface",  top_of_surface);
    dataframe->addExistingColumn("median_ground",   median_ground);
    dataframe->addExistingColumn("pflags",          pflags);

    // install ancillary columns into dataframe
    GeoDataFrame::addAncillaryColumns (ancillary_columns, dataframe);
    delete ancillary_columns;

    // finalize dataframe
    dataframe->populateGeoColumns();

    // update runtime
    return true;
}

/*----------------------------------------------------------------------------
 * algorithm
 *----------------------------------------------------------------------------*/
void SurfaceBlanket::algorithm (const Atl03DataFrame& df, uint32_t start_photon, uint32_t num_photons, result_t& result)
{
    // put photon heights into classified arrays
    vector<float> ground_heights;
    vector<float> canopy_heights;
    vector<float> topcan_heights;
    for(uint32_t p = 0; p < num_photons; p++)
    {
        const uint32_t i = start_photon + p;

        if(df.atl08_class[i] == Icesat2Fields::ATL08_GROUND)
        {
            ground_heights.push_back(df.height[i]);
        }
        else if(df.atl08_class[i] == Icesat2Fields::ATL08_CANOPY)
        {
            canopy_heights.push_back(df.height[i]);
        }
        else if(df.atl08_class[i] == Icesat2Fields::ATL08_TOP_OF_CANOPY)
        {
            canopy_heights.push_back(df.height[i]); // include top of canopy in canopy
            topcan_heights.push_back(df.height[i]);
        }
    }

    // calculate top of surface height
    result.top_of_surface_h = std::numeric_limits<float>::quiet_NaN();
    if(topcan_heights.size() >= static_cast<size_t>(parms->minPhotonCount.value))
    {
        result.top_of_surface_h = percentile(topcan_heights, parms->blanket.max_top_of_surface_percentile.value);
    }
    else if(canopy_heights.size() >= static_cast<size_t>(parms->minPhotonCount.value))
    {
        result.top_of_surface_h = percentile(canopy_heights, parms->blanket.max_top_of_surface_percentile.value);
    }
    else if(ground_heights.size() >= static_cast<size_t>(parms->minPhotonCount.value))
    {
        result.top_of_surface_h = percentile(ground_heights, parms->blanket.max_top_of_surface_percentile.value);
    }
    else
    {
        result.pflags |= Icesat2Fields::PFLAG_TOO_FEW_PHOTONS;
    }

    // calculate median ground height
    result.median_ground_h = std::numeric_limits<float>::quiet_NaN();
    if(ground_heights.size() >= static_cast<size_t>(parms->minPhotonCount.value))
    {
        result.median_ground_h = percentile(ground_heights, parms->blanket.median_ground_percentile.value);
    }
    else
    {
        result.pflags |= Icesat2Fields::PFLAG_TOO_FEW_PHOTONS;
    }
}

/*----------------------------------------------------------------------------
 * percentile
 *----------------------------------------------------------------------------*/
double SurfaceBlanket::percentile(vector<float>& data, double p)
{
    size_t n = data.size();
    double idx = p * (n - 1);

    size_t k = static_cast<size_t>(idx);
    double frac = idx - k;

    std::nth_element(data.begin(), data.begin() + k, data.end());
    double a = data[k];

    std::nth_element(data.begin(), data.begin() + k + 1, data.end());
    double b = data[k + 1];

    return a + (frac * (b - a));
}

