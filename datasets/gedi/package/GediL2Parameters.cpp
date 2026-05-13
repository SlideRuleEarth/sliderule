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
 * INCLUDE
 ******************************************************************************/

#include "OsApi.h"
#include "FieldEnumeration.h"
#include "GediL2Parameters.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GediL2Parameters::OBJECT_TYPE = "GediL2Parameters";

 /******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GediL2Parameters::GediL2Parameters(lua_State* L , uint64_t key_space, const char* asset_name, const char* _resource, const char* object_type):
    GediParameters (L, key_space, asset_name, _resource, object_type)
{
    addParameter("l2_quality_filter", &l2_quality_filter, "Filter for level 2 low quality data; when enabled, low quality returns are not included in the response");
    addParameter("surface_filter",    &surface_filter,    "Filter for surface data; when enabled, only surface returns are included in the response");

    // backwards compatibility
    addParameter("l2_quality_flag",   &l2_quality_flag,   "Flag for level 2 low quality data (only source data that matches flag value is included); deprecated, use 'l2_quality_filter'");
    addParameter("surface_flag",      &surface_flag,      "Flag for surface data (only source data that matches flag value is included); deprecated, use 'surface_filter'");
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void GediL2Parameters::fromLua (lua_State* L, int index)
{
    GediParameters::fromLua(L, index);

    // set filters
    if(l2_quality_flag == 1) l2_quality_filter = true;
    if(surface_flag == 1) surface_filter = true;
}