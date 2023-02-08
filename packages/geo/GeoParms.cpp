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

#include <lua.h>
#include <gdal.h>

#include "core.h"
#include "GeoParms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoParms::SELF                  = "samples";
const char* GeoParms::SAMPLING_ALGO         = "algo";
const char* GeoParms::SAMPLING_RADIUS       = "radius";
const char* GeoParms::ZONAL_STATS           = "zonal";
const char* GeoParms::AUXILIARY_FILES       = "aux";
const char* GeoParms::START_TIME            = "t0";
const char* GeoParms::STOP_TIME             = "t1";
const char* GeoParms::URL_SUBSTRING         = "substr";
const char* GeoParms::ASSET                 = "asset";

const char* GeoParms::NEARESTNEIGHBOUR_ALGO = "NearestNeighbour";
const char* GeoParms::BILINEAR_ALGO         = "Bilinear";
const char* GeoParms::CUBIC_ALGO            = "Cubic";
const char* GeoParms::CUBICSPLINE_ALGO      = "CubicSpline";
const char* GeoParms::LANCZOS_ALGO          = "Lanczos";
const char* GeoParms::AVERAGE_ALGO          = "Average";
const char* GeoParms::MODE_ALGO             = "Mode";
const char* GeoParms::GAUSS_ALGO            = "Gauss";

const char* GeoParms::OBJECT_TYPE           = "GeoParms";
const char* GeoParms::LuaMetaName           = "GeoParms";
const struct luaL_Reg GeoParms::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int GeoParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Geo parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new GeoParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoParms::GeoParms (lua_State* L, int index):
    LuaObject           (L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    sampling_algo       (GRIORA_NearestNeighbour),
    sampling_radius     (0),
    zonal_stats         (false),
    auxiliary_files     (false),
    filter_time         (false),
    url_substring       (NULL),
    asset               (NULL)
{
    fromLua(L, index);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoParms::~GeoParms (void)
{
    if(url_substring) delete [] url_substring;
    if(asset) asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void GeoParms::fromLua (lua_State* L, int index)
{
    /* Must be a Table */
    if(lua_istable(L, index))
    {
        bool field_provided = false;

        /* Sampling Algorithm */
        lua_getfield(L, index, SAMPLING_ALGO);
        const char* algo_str = LuaObject::getLuaString(L, -1, true, NULL);
        if(algo_str)
        {
            sampling_algo = str2algo(algo_str);
            mlog(DEBUG, "Setting %s to %d", SAMPLING_ALGO, sampling_algo);
        }
        lua_pop(L, 1);

        /* Sampling Radius */
        lua_getfield(L, index, SAMPLING_RADIUS);
        sampling_radius = (int)LuaObject::getLuaInteger(L, -1, true, sampling_radius, &field_provided));
        if(sampling_radius < 0) throw RunTimeException(CRITICAL, RTE_ERROR, "invalid sampling radius: %d:", sampling_radius);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", SAMPLING_RADIUS, (int)sampling_radius);
        lua_pop(L, 1);

        /* Zonal Statistics */
        lua_getfield(L, index, ZONAL_STATS);
        zonal_stats = LuaObject::getLuaBoolean(L, -1, true, zonal_stats, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", ZONAL_STATS, (int)zonal_stats);
        lua_pop(L, 1);

        /* Auxiliary Files */
        lua_getfield(L, index, AUXILIARY_FILES);
        auxiliary_files = LuaObject::getLuaBoolean(L, -1, true, auxiliary_files, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", AUXILIARY_FILES, (int)auxiliary_files);
        lua_pop(L, 1);

        /* Start Time */
        lua_getfield(L, index, START_TIME);
        const char* t0_str = LuaObject::getLuaString(L, -1, true, NULL);
        if(t0_str)
        {
            int64_t gps = TimeLib::str2gpstime(t0_str);
            if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", t0_str);
            start_time = TimeLib::gps2gmttime(gps);
            filter_time = true;
            TimeLib::date_t start_date = TimeLib::gmt2date(gmt);
            mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", START_TIME, start_date.year, start_date.month, start_date.day, start_time.hour, start_time.minute, start_time.second);
        }
        lua_pop(L, 1);

        /* Stop Time */
        lua_getfield(L, index, STOP_TIME);
        const char* t1_str = LuaObject::getLuaString(L, -1, true, NULL);
        if(t1_str)
        {
            int64_t gps = TimeLib::str2gpstime(t1_str);
            if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", t0_str);
            stop_time = TimeLib::gps2gmttime(gps);
            filter_time = true;
            TimeLib::date_t stop_date = TimeLib::gmt2date(gmt);
            mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", STOP_TIME, stop_date.year, stop_date.month, stop_date.day, stop_time.hour, stop_time.minute, stop_time.second);
        }
        lua_pop(L, 1);

        /* Start and Stop Time Special Cases */
        if(t0_str && !t1_str) // only start time supplied
        {
            int64_t now = TimeLib::gettimems();
            stop_time = TimeLib::gps2gmttime(now);
            TimeLib::date_t stop_date = TimeLib::gmt2date(gmt);
            mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", STOP_TIME, stop_date.year, stop_date.month, stop_date.day, stop_time.hour, stop_time.minute, stop_time.second);
        }
        else if(!t0_str && t1_str) // only stop time supplied
        {
            int64_t gps_epoch = 0;
            start_time = TimeLib::gps2gmttime(gps_epoch);
            TimeLib::date_t start_date = TimeLib::gmt2date(gmt);
            mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", START_TIME, start_date.year, start_date.month, start_date.day, start_time.hour, start_time.minute, start_time.second);
        }

        /* URL Substring Filter */
        lua_getfield(L, index, URL_SUBSTRING);
        url_substring = LuaObject::getLuaString(L, -1, true, NULL);
        if(url_substring) mlog(DEBUG, "Setting %s to %s", URL_SUBSTRING, url_substring);
        lua_pop(L, 1);

        /* Asset */
        lua_getfield(L, index, ASSET);
        const char* asset_name = LuaObject::getLuaString(L, -1, true, NULL);
        if(asset_name)
        {
            asset = LuaObject::getLuaObjectByName(name, Asset::OBJECT_TYPE);
            mlog(DEBUG, "Setting %s to %s", ASSET, asset_name);
        }
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * str2algo
 *----------------------------------------------------------------------------*/
GDALRIOResampleAlg GeoParms::str2algo (const char* str)
{
         if (!str)                                          return GRIORA_NearestNeighbour;
    else if (StringLib::match(str, NEARESTNEIGHBOUR_ALGO))  return GRIORA_NearestNeighbour;
    else if (StringLib::match(str, BILINEAR_ALGO))          return GRIORA_Bilinear;
    else if (StringLib::match(str, CUBIC_ALGO))             return GRIORA_Cubic;
    else if (StringLib::match(str, CUBICSPLINE_ALGO))       return GRIORA_CubicSpline;
    else if (StringLib::match(str, LANCZOS_ALGO))           return GRIORA_Lanczos;
    else if (StringLib::match(str, AVERAGE_ALGO))           return GRIORA_Average;
    else if (StringLib::match(str, MODE_ALGO))              return GRIORA_Mode;
    else if (StringLib::match(str, GAUSS_ALGO))             return GRIORA_Gauss;
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling algorithm: %s:", str);
}
