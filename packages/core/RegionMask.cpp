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
#include "RegionMask.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

RegionMask::burn_func_t RegionMask::burnMask = NULL;

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * registerRasterizer
 *----------------------------------------------------------------------------*/
void RegionMask::registerRasterizer (burn_func_t func)
{
    burnMask = func;
}

/*----------------------------------------------------------------------------
 * Constructor - RegionMask
 *----------------------------------------------------------------------------*/
RegionMask::RegionMask(void):
    FieldDictionary ({  {"geojson",     &geojson},
                        {"cellsize",    &cellSize},
                        {"cols",        &cols},
                        {"rows",        &rows},
                        {"lonmin",      &lonMin},
                        {"latmin",      &latMin},
                        {"lonmax",      &lonMax},
                        {"latmax",      &latMax}  })
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RegionMask::~RegionMask(void)
{
    delete [] data;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
string RegionMask::toJson (void) const
{
    return FieldDictionary::toJson();
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
int RegionMask::toLua (lua_State* L) const
{
    return FieldDictionary::toLua(L);
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void RegionMask::fromLua (lua_State* L, int index)
{
    FieldDictionary::fromLua(L, index);    
    if(cellSize.value > 0.0 && !geojson.value.empty())
    {
        if(RegionMask::burnMask) 
        {
            RegionMask::burnMask(*this);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "unable to rasterize geojson - function unregistered");
        }
    }
}

/*----------------------------------------------------------------------------
 * includes
 *----------------------------------------------------------------------------*/
bool RegionMask::includes(double lon, double lat) const
{
    if((lonMin.value <= lon) && (lonMax.value >= lon) &&
        (latMin.value <= lat) && (latMax.value >= lat))
    {
        const uint32_t row = (latMax.value - lat) / cellSize.value;
        const uint32_t col = (lon - lonMin.value) / cellSize.value;
        if((row < rows.value) && (col < cols.value))
        {
            return static_cast<int>(data[(row * cols.value) + col]) == PIXEL_ON;
        }
    }
    return false;
}
