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
 *INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "FootprintReader.h"
#include "Gedi01bReader.h"
#include "Gedi01bDataFrame.h"
#include "Gedi02aReader.h"
#include "Gedi02aDataFrame.h"
#include "Gedi04aReader.h"
#include "Gedi04aDataFrame.h"
#include "GediRaster.h"
#include "GediFields.h"
#include "GediIODriver.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEDI_LIBNAME                            "gedi"
#define LUA_GEDI_L03_ELEVATION_RASTER_NAME          "gedil3-elevation"
#define LUA_GEDI_L03_CANOPY_RASTER_NAME             "gedil3-canopy"
#define LUA_GEDI_L03_ELEVATION_STDDEV_RASTER_NAME   "gedil3-elevation-stddev"
#define LUA_GEDI_L03_CANOPY_STDDEV_RASTER_NAME      "gedil3-canopy-stddev"
#define LUA_GEDI_L03_COUNTS_RASTER_NAME             "gedil3-counts"
#define LUA_GEDI_L04B_RASTER_NAME                   "gedil4b"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * gedi_open
 *----------------------------------------------------------------------------*/
int gedi_open (lua_State *L)
{
    static const struct luaL_Reg gedi_functions[] = {
        {"parms",               GediFields::luaCreate},
        {"gedi01b",             Gedi01bReader::luaCreate},
        {"gedi01bx",            Gedi01bDataFrame::luaCreate},
        {"gedi02a",             Gedi02aReader::luaCreate},
        {"gedi02ax",            Gedi02aDataFrame::luaCreate},
        {"gedi04a",             Gedi04aReader::luaCreate},
        {"gedi04ax",            Gedi04aDataFrame::luaCreate},
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, gedi_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initgedi (void)
{
    /* Initialize Modules */
    Gedi01bReader::init();
    Gedi02aReader::init();
    Gedi04aReader::init();

    /* Register GEDI IO Driver */
    Asset::registerDriver(GediIODriver::FORMAT, GediIODriver::create);

    /* Register Rasters */
    RasterObject::registerRaster(LUA_GEDI_L03_ELEVATION_RASTER_NAME,        GediRaster::createL3ElevationRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_CANOPY_RASTER_NAME,           GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_ELEVATION_STDDEV_RASTER_NAME, GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_CANOPY_STDDEV_RASTER_NAME,    GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_COUNTS_RASTER_NAME,           GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L04B_RASTER_NAME,                 GediRaster::createL4DataRaster);

    /* Register Schemas */
    {
        const uint32_t COL = Field::NESTED_COLUMN;
        GeoDataFrame::registerSchema("gedi01bx", "GEDI L1B geolocated waveforms", {
            {"shot_number",     COL | Field::UINT64, "GEDI shot number",               false},
            {"time_ns",         COL | Field::TIME8,  "Shot timestamp (Unix ns)",        false},
            {"latitude",        COL | Field::DOUBLE, "Latitude (degrees)",              false},
            {"longitude",       COL | Field::DOUBLE, "Longitude (degrees)",             false},
            {"elevation_start", COL | Field::FLOAT,  "Waveform start elevation (m)",    false},
            {"elevation_stop",  COL | Field::DOUBLE, "Waveform stop elevation (m)",     false},
            {"solar_elevation", COL | Field::DOUBLE, "Solar elevation angle (deg)",     false},
            {"tx_size",         COL | Field::UINT16, "Transmit waveform sample count",  false},
            {"rx_size",         COL | Field::UINT16, "Receive waveform sample count",   false},
            {"flags",           COL | Field::UINT8,  "Combined quality flags",          false},
            {"tx_waveform",     Field::NESTED_LIST | Field::FLOAT, "Transmit waveform samples", false},
            {"rx_waveform",     Field::NESTED_LIST | Field::FLOAT, "Receive waveform samples",  false},
            {"beam",            COL | Field::UINT8,  "GEDI beam number",               true},
            {"orbit",           COL | Field::UINT32, "Orbit number",                   true},
            {"track",           COL | Field::UINT16, "Track number",                   true},
            {"srcid",           COL | Field::INT32,  "Source granule ID (see metadata)", false},
        });

        GeoDataFrame::registerSchema("gedi02ax", "GEDI L2A elevation and height metrics", {
            {"shot_number",     COL | Field::UINT64, "GEDI shot number",               false},
            {"time_ns",         COL | Field::TIME8,  "Shot timestamp (Unix ns)",        false},
            {"latitude",        COL | Field::DOUBLE, "Latitude (degrees)",              false},
            {"longitude",       COL | Field::DOUBLE, "Longitude (degrees)",             false},
            {"elevation_lm",    COL | Field::FLOAT,  "Elevation of lowest mode (m)",    false},
            {"elevation_hr",    COL | Field::FLOAT,  "Elevation of highest return (m)", false},
            {"solar_elevation", COL | Field::FLOAT,  "Solar elevation angle (deg)",     false},
            {"sensitivity",     COL | Field::FLOAT,  "Beam sensitivity",               false},
            {"flags",           COL | Field::UINT8,  "Combined quality flags",          false},
            {"beam",            COL | Field::UINT8,  "GEDI beam number",               true},
            {"orbit",           COL | Field::UINT32, "Orbit number",                   true},
            {"track",           COL | Field::UINT16, "Track number",                   true},
            {"srcid",           COL | Field::INT32,  "Source granule ID (see metadata)", false},
        });

        GeoDataFrame::registerSchema("gedil4ax", "GEDI L4A above-ground biomass density", {
            {"shot_number",     COL | Field::UINT64, "GEDI shot number",               false},
            {"time_ns",         COL | Field::TIME8,  "Shot timestamp (Unix ns)",        false},
            {"latitude",        COL | Field::DOUBLE, "Latitude (degrees)",              false},
            {"longitude",       COL | Field::DOUBLE, "Longitude (degrees)",             false},
            {"agbd",            COL | Field::FLOAT,  "Above-ground biomass density (Mg/ha)", false},
            {"elevation",       COL | Field::FLOAT,  "Elevation of lowest mode (m)",    false},
            {"solar_elevation", COL | Field::FLOAT,  "Solar elevation angle (deg)",     false},
            {"sensitivity",     COL | Field::FLOAT,  "Beam sensitivity",               false},
            {"flags",           COL | Field::UINT8,  "Combined quality flags",          false},
            {"beam",            COL | Field::UINT8,  "GEDI beam number",               true},
            {"orbit",           COL | Field::UINT32, "Orbit number",                   true},
            {"track",           COL | Field::UINT16, "Track number",                   true},
            {"srcid",           COL | Field::INT32,  "Source granule ID (see metadata)", false},
        });
    }

    /* Extend Lua */
    LuaEngine::extend(LUA_GEDI_LIBNAME, gedi_open, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_GEDI_LIBNAME, LIBID);
}

void deinitgedi (void)
{
}
}
