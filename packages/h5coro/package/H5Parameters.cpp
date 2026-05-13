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
#include "GeoDataFrame.h"
#include "H5Parameters.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * defaultCRS
 *----------------------------------------------------------------------------*/
const char* H5Parameters::defaultCRS (void)
{
    /* Load and cache the CRS once; returned value is compact PROJJSON. */
    const static string crs = GeoDataFrame::loadCRSFile("EPSG7912.projjson");
    return crs.c_str();
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Parameters::H5Parameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const std::initializer_list<init_entry_t>& init_list):
    RequestParameters (L, key_space, asset_name, _resource, {
        {"col",         &col,           "The column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned; if the variable has more than one column, then by default the first column is read, if all columns are wanted, then set col=-1 and the result will be a flattened array of all of the data"},
        {"startrow",    &startRow,      "The first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)"},
        {"numrows",     &numRows,       "The number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if ALL_ROWS selected, it will read from the startrow to the end of the dataset"},
        {"crs",         &crs,           "Coordinate reference system to attach to the resulting dataframe when 'h5x' is used to build a dataframe from an HDF5 granule"},
        {"index",       &index_column,  "The 'h5x' dataframe column to identify as the index"},
        {"time",        &time_column,   "The 'h5x' dataframe column to identify as the time"},
        {"x",           &x_column,      "The 'h5x' dataframe column to identify as the x coordinate"},
        {"y",           &y_column,      "The 'h5x' dataframe column to identify as the y coordinate"},
        {"z",           &z_column,      "The 'h5x' dataframe column to identify as the z coordinate"},
        {"groups",      &groups,        "The groups to pull from when 'h5x' is reading variables to build the dataframe"},
        {"variables",   &variables,     "The variables to read when 'h5x' is building the dataframe; each variable becomes a column in the dataframe"}})
{
    crs.value = defaultCRS(); // initialize
    for(const init_entry_t elem: init_list)
    {
        const entry_t entry = {elem.field, elem.description, false};
        fields.add(elem.name, entry);
    }
}
