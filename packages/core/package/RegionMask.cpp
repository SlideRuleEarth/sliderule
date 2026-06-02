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

Dictionary<RegionMask::rasterizer_t> RegionMask::rasterizers(16);
const char* RegionMask::GEOJSON_FORMAT = "geojson";
const char* RegionMask::B16MASK_FORMAT = "b16mask";

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * registerRasterizer - NOT THREAD SAFE
 *----------------------------------------------------------------------------*/
void RegionMask::registerRasterizer (const char* format, burn_func_t func)
{
    rasterizer_t rasterizer = {
        .format = format,
        .burn_func = func
    };
    rasterizers.add(format, rasterizer);
}

/*----------------------------------------------------------------------------
 * decodeB16mask
 *----------------------------------------------------------------------------*/
void RegionMask::decodeB16mask(RegionMask& image)
{
    // get and check rows of raster
    uint32_t rows = static_cast<uint32_t>((image.latMax.value - image.latMin.value) / image.cellSize.value);
    if(rows > MAX_ROWS)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Number of rows exceeds maximum allowed: %u", rows);
    }

    // get and check columns of raster
    uint32_t cols = static_cast<uint32_t>((image.lonMax.value - image.lonMin.value) / image.cellSize.value);
    if(cols > MAX_COLS)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Number of columns exceeds maximum allowed: %u", cols);
    }

    // get and check size of raster
    uint32_t data_size = rows * cols;
    size_t expected_b16_size = data_size / 8 * 2; // 8 bits per byte, 2 ASCII bytes per byte
    if(image.b16mask.value.length() != expected_b16_size)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid b16 size: %lu (expected %lu)", image.b16mask.value.length(), expected_b16_size);
    }

    // decode raster
    size_t bit_array_length = image.b16mask.value.length() / 2;
    uint8_t* bit_array = new uint8_t [bit_array_length];
    uint32_t bytes_decoded = StringLib::b16decode(image.b16mask.value.c_str(), image.b16mask.value.length(), true, bit_array);
    if(bytes_decoded != bit_array_length)
    {
        delete [] bit_array;
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to fully decode region mask: %d (expected %lu)", bytes_decoded, bit_array_length);
    }

    // allocate raster
    image.data = new uint8_t [data_size];
    image.rows = rows;
    image.cols = cols;

    // populate raster
    uint32_t k = 0;
    for(uint32_t i = 0; i < bytes_decoded; i++)
    {
        image.data[k++] = bit_array[i] & 0x80;
        image.data[k++] = bit_array[i] & 0x40;
        image.data[k++] = bit_array[i] & 0x20;
        image.data[k++] = bit_array[i] & 0x10;
        image.data[k++] = bit_array[i] & 0x08;
        image.data[k++] = bit_array[i] & 0x04;
        image.data[k++] = bit_array[i] & 0x02;
        image.data[k++] = bit_array[i] & 0x01;
    }

    // clean up
    delete [] bit_array;
}

/*----------------------------------------------------------------------------
 * Constructor - RegionMask
 *----------------------------------------------------------------------------*/
RegionMask::RegionMask(void):
    FieldMap<Field> ({  {GEOJSON_FORMAT,    &geojson,   "GeoJSON string defining area of interest to be rasterized"},
                        {B16MASK_FORMAT,    &b16mask,   "Base16 encoded bit-mask defining area of interest to be rasterized"},
                        {"cellsize",        &cellSize,  "Pixel size of rasterized area of interest"},
                        {"cols",            &cols,      "Number of columns in the rasterized area of interest"},
                        {"rows",            &rows,      "Number of rows in the rasterized area of interest"},
                        {"lonmin",          &lonMin,    "Minimum longitude in the area of interest"},
                        {"latmin",          &latMin,    "Minumum latitude in the area of interest"},
                        {"lonmax",          &lonMax,    "Maximum longitude in the area of interest"},
                        {"latmax",          &latMax,    "Maximum latitude in the area of interest"}  })
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
 * valid
 *----------------------------------------------------------------------------*/
bool RegionMask::valid (void) const
{
    return cellSize.value > 0.0;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void RegionMask::fromLua (lua_State* L, int index)
{
    FieldMap<Field>::fromLua(L, index);
    if(cellSize.value > 0.0)
    {
        burn_func_t burn_func = NULL;
        if(!geojson.value.empty())
        {
            burn_func = RegionMask::rasterizers[GEOJSON_FORMAT].burn_func; // will throw if not found
        }
        else if(!b16mask.value.empty())
        {
            burn_func = RegionMask::rasterizers[B16MASK_FORMAT].burn_func; // will throw if not found
        }

        if(burn_func)
        {
            burn_func(*this);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to rasterize region of interest - no data supplied");
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
