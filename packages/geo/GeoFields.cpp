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

#include <gdal.h>

#include "OsApi.h"
#include "RequestFields.h"
#include "GeoFields.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoFields::DEFAULT_KEY = "default";

const char* GeoFields::NEARESTNEIGHBOUR_ALGO_STR = "NearestNeighbour";
const char* GeoFields::BILINEAR_ALGO_STR         = "Bilinear";
const char* GeoFields::CUBIC_ALGO_STR            = "Cubic";
const char* GeoFields::CUBICSPLINE_ALGO_STR      = "CubicSpline";
const char* GeoFields::LANCZOS_ALGO_STR          = "Lanczos";
const char* GeoFields::AVERAGE_ALGO_STR          = "Average";
const char* GeoFields::MODE_ALGO_STR             = "Mode";
const char* GeoFields::GAUSS_ALGO_STR            = "Gauss";

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int GeoFields::luaCreate (lua_State* L)
{
    RequestFields* request_fields = NULL;
    try
    {
        request_fields = new RequestFields(L, 0, {});
        request_fields->samplers.values.emplace(GeoFields::DEFAULT_KEY, GeoFields());
        request_fields->samplers.values[GeoFields::DEFAULT_KEY].fromLua(L, 1);
        return LuaObject::createLuaObject(L, request_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating request parameters with default geo fields: %s", e.what());
        delete request_fields;
        lua_pushnil(L);
        return 1;
    }
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void GeoFields::fromLua (lua_State* L, int index)
{
    FieldDictionary::fromLua(L, index);

    /* Sampling Radius */
    if(sampling_radius.value < 0)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid sampling radius: %d:", sampling_radius.value);
    }

    /* Start Time */
    if(!t0.value.empty())
    {
        const int64_t gps = TimeLib::str2gpstime(t0.value.c_str());
        if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", t0.value.c_str());
        start_time = TimeLib::gps2gmttime(gps);
        filter_time = true;
        const TimeLib::date_t start_date = TimeLib::gmt2date(start_time);
        mlog(DEBUG, "Setting t0 to %04d-%02d-%02dT%02d:%02d:%02dZ", start_date.year, start_date.month, start_date.day, start_time.hour, start_time.minute, start_time.second);
    }

    /* Stop Time */
    if(!t1.value.empty())
    {
        const int64_t gps = TimeLib::str2gpstime(t1.value.c_str());
        if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", t1.value.c_str());
        stop_time = TimeLib::gps2gmttime(gps);
        filter_time = true;
        const TimeLib::date_t stop_date = TimeLib::gmt2date(stop_time);
        mlog(DEBUG, "Setting t1 to %04d-%02d-%02dT%02d:%02d:%02dZ", stop_date.year, stop_date.month, stop_date.day, stop_time.hour, stop_time.minute, stop_time.second);
    }
    lua_pop(L, 1);

    /* Start and Stop Time Special Cases */
    if(!t0.value.empty() && t1.value.empty()) // only start time supplied
    {
        const int64_t now = TimeLib::gpstime();
        stop_time = TimeLib::gps2gmttime(now);
        const TimeLib::date_t stop_date = TimeLib::gmt2date(stop_time);
        mlog(DEBUG, "Setting t1 to %04d-%02d-%02dT%02d:%02d:%02dZ", stop_date.year, stop_date.month, stop_date.day, stop_time.hour, stop_time.minute, stop_time.second);
    }
    else if(t0.value.empty() && !t1.value.empty()) // only stop time supplied
    {
        const int64_t gps_epoch = 0;
        start_time = TimeLib::gps2gmttime(gps_epoch);
        const TimeLib::date_t start_date = TimeLib::gmt2date(start_time);
        mlog(DEBUG, "Setting t0 to %04d-%02d-%02dT%02d:%02d:%02dZ", start_date.year, start_date.month, start_date.day, start_time.hour, start_time.minute, start_time.second);
    }

    /* Closest Time Filter */
    if(!tc.value.empty())
    {
        const int64_t gps = TimeLib::str2gpstime(tc.value.c_str());
        if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", tc.value.c_str());
        filter_closest_time = true;
        closest_time = TimeLib::gps2gmttime(gps);
        const TimeLib::date_t closest_date = TimeLib::gmt2date(closest_time);
        mlog(DEBUG, "Setting closest time to %04d-%02d-%02dT%02d:%02d:%02dZ", closest_date.year, closest_date.month, closest_date.day, closest_time.hour, closest_time.minute, closest_time.second);
    }
    lua_pop(L, 1);

    /* Day Of Year Range Filter */
    if(!doy_range.value.empty())
    {
        int start_pos = 0;

        /* Do we keep in range 'dd:dd' or remove '!dd:dd' */
        if(doy_range.value[0] == '!')
        {
            doy_keep_inrange = false;
            start_pos++;
        }
        if(!TimeLib::str2doyrange(doy_range.value.substr(start_pos).c_str(), doy_start, doy_end))
            throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse day of year range supplied: %s", doy_range.value.substr(start_pos).c_str());

        if((doy_start > doy_end) || (doy_start == doy_end) ||
            (doy_start < 1 || doy_start > 366) ||
            (doy_end   < 1 || doy_end   > 366))
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid day of year range: %d:%d", doy_start, doy_end);

        filter_doy_range = true;
        mlog(DEBUG, "Setting day of year to %02d:%02d, doy_keep_inrange: %s", doy_start, doy_end, doy_keep_inrange ? "true" : "false");
    }
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoFields::GeoFields (void):
    FieldDictionary ({
        {"algorithm",       &sampling_algo},
        {"radius",          &sampling_radius},
        {"t0",              &t0},
        {"t1",              &t1},
        {"closest_time",    &tc},
        {"zonal_stats",     &zonal_stats},
        {"with_flags",      &flags_file},
        {"substr",          &url_substring},
        {"use_poi_time",    &use_poi_time},
        {"doy_range",       &doy_range},
        {"sort_by_index",   &sort_by_index},
        {"proj_pipeline",   &proj_pipeline},
        {"aoi_bbox",        &aoi_bbox},
        {"catalog",         &catalog},
        {"bands",           &bands},
        {"asset",           &asset}
    }),
    filter_time(false),
    filter_doy_range(false),
    doy_keep_inrange{true},
    doy_start{0},
    doy_end{0},
    filter_closest_time(false),
    closest_time{0,0,0,0,0,0},
    start_time{0,0,0,0,0,0},
    stop_time{0,0,0,0,0,0}
{
}

/*----------------------------------------------------------------------------
 * Constructor - Copy
 *----------------------------------------------------------------------------*/
GeoFields::GeoFields (const GeoFields& other):
    FieldDictionary ({
        {"algorithm",       &sampling_algo},
        {"radius",          &sampling_radius},
        {"t0",              &t0},
        {"t1",              &t1},
        {"closest_time",    &tc},
        {"zonal_stats",     &zonal_stats},
        {"with_flags",      &flags_file},
        {"substr",          &url_substring},
        {"use_poi_time",    &use_poi_time},
        {"doy_range",       &doy_range},
        {"sort_by_index",   &sort_by_index},
        {"proj_pipeline",   &proj_pipeline},
        {"aoi_bbox",        &aoi_bbox},
        {"catalog",         &catalog},
        {"bands",           &bands},
        {"asset",           &asset}
    }),
    filter_time(other.filter_time),
    filter_doy_range(other.filter_doy_range),
    doy_keep_inrange(other.doy_keep_inrange),
    doy_start(other.doy_start),
    doy_end(other.doy_end),
    filter_closest_time(other.filter_closest_time),
    closest_time(other.closest_time),
    start_time(other.start_time),
    stop_time(other.stop_time)
{
    sampling_algo = other.sampling_algo;
    sampling_radius = other.sampling_radius;
    t0 = other.t0;
    t1 = other.t1;
    tc = other.tc;
    zonal_stats = other.zonal_stats;
    flags_file = other.flags_file;
    url_substring = other.url_substring;
    use_poi_time = other.use_poi_time;
    doy_range = other.doy_range;
    sort_by_index = other.sort_by_index;
    proj_pipeline = other.proj_pipeline;
    aoi_bbox = other.aoi_bbox;
    catalog = other.catalog;
    bands = other.bands;
    asset = other.asset;

printf("\n\n\nHERE WE ARE\n\n\n");
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
/*
GeoFields& GeoFields::operator= (const GeoFields& geo_fields)
{
    if(&geo_fields == this) return *this;
printf("\n\n\nPH BOY\n\n\n");
    fields = geo_fields.fields;
    return *this;
}
*/
/*----------------------------------------------------------------------------
 * sserror2str
 *----------------------------------------------------------------------------*/
std::string GeoFields::sserror2str(uint32_t error)
{
    std::string errorStr;

    if(error == SS_NO_ERRORS)
    {
        errorStr = "SS_NO_ERRORS";
        return errorStr;
    }

    if(error & SS_THREADS_LIMIT_ERROR)
    {
        errorStr += "SS_THREADS_LIMIT_ERROR, ";
    }
    if(error & SS_MEMPOOL_ERROR)
    {
        errorStr += "SS_MEMPOOL_ERROR, ";
    }
    if(error & SS_OUT_OF_BOUNDS_ERROR)
    {
        errorStr += "SS_OUT_OF_BOUNDS_ERROR, ";
    }
    if(error & SS_READ_ERROR)
    {
        errorStr += "SS_READ_ERROR, ";
    }
    if(error & SS_WRITE_ERROR)
    {
        errorStr += "SS_WRITE_ERROR, ";
    }
    if(error & SS_SUBRASTER_ERROR)
    {
        errorStr += "SS_SUBRASTER_ERROR, ";
    }
    if(error & SS_INDEX_FILE_ERROR)
    {
        errorStr += "SS_INDEX_FILE_ERROR, ";
    }
    if(error & SS_RESOURCE_LIMIT_ERROR)
    {
        errorStr += "SS_RESOURCE_LIMIT_ERROR, ";
    }

    /* Remove the last ", " if it exists */
    if (errorStr.size() >= 2 && errorStr[errorStr.size() - 2] == ',')
    {
        errorStr.resize(errorStr.size() - 2);
    }
    return errorStr;
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson - GeoFields::sampling_algo_t
 *----------------------------------------------------------------------------*/
string convertToJson(const GeoFields::sampling_algo_t& v)
{
    switch(v)
    {
        case GeoFields::NEARESTNEIGHBOUR_ALGO: return "NearestNeighbour";
        case GeoFields::BILINEAR_ALGO:         return "Bilinear";
        case GeoFields::CUBIC_ALGO:            return "Cubic";
        case GeoFields::CUBICSPLINE_ALGO:      return "CubicSpline";
        case GeoFields::LANCZOS_ALGO:          return "Lanczos";
        case GeoFields::AVERAGE_ALGO:          return "Average";
        case GeoFields::MODE_ALGO:             return "Mode";
        case GeoFields::GAUSS_ALGO:            return "Gauss";
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "Unknown sampling algorithm: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - GeoFields::sampling_algo_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const GeoFields::sampling_algo_t& v)
{
    switch(v)
    {
        case GeoFields::NEARESTNEIGHBOUR_ALGO: lua_pushstring(L, "NearestNeighbour");   break;
        case GeoFields::BILINEAR_ALGO:         lua_pushstring(L, "Bilinear");           break;
        case GeoFields::CUBIC_ALGO:            lua_pushstring(L, "Cubic");              break;
        case GeoFields::CUBICSPLINE_ALGO:      lua_pushstring(L, "CubicSpline");        break;
        case GeoFields::LANCZOS_ALGO:          lua_pushstring(L, "Lanczos");            break;
        case GeoFields::AVERAGE_ALGO:          lua_pushstring(L, "Average");            break;
        case GeoFields::MODE_ALGO:             lua_pushstring(L, "Mode");               break;
        case GeoFields::GAUSS_ALGO:            lua_pushstring(L, "Gauss");              break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "Unknown sampling algorithm: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - GeoFields::sampling_algo_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, GeoFields::sampling_algo_t& v)
{
    if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if      (StringLib::match(str, "NearestNeighbour")) v = GeoFields::NEARESTNEIGHBOUR_ALGO;
        else if (StringLib::match(str, "Bilinear"))         v = GeoFields::BILINEAR_ALGO;
        else if (StringLib::match(str, "Cubic"))            v = GeoFields::CUBIC_ALGO;
        else if (StringLib::match(str, "CubicSpline"))      v = GeoFields::CUBICSPLINE_ALGO;
        else if (StringLib::match(str, "Lanczos"))          v = GeoFields::LANCZOS_ALGO;
        else if (StringLib::match(str, "Average"))          v = GeoFields::AVERAGE_ALGO;
        else if (StringLib::match(str, "Mode"))             v = GeoFields::MODE_ALGO;
        else if (StringLib::match(str, "Gauss"))            v = GeoFields::GAUSS_ALGO;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "Unknown sampling algorithm: %s", str);
    }
    else
    {
        const long n = LuaObject::getLuaInteger(L, index);
        switch(static_cast<GeoFields::sampling_algo_t>(n))
        {
            case GeoFields::NEARESTNEIGHBOUR_ALGO: break; // valid value
            case GeoFields::BILINEAR_ALGO:         break; // valid value
            case GeoFields::CUBIC_ALGO:            break; // valid value
            case GeoFields::CUBICSPLINE_ALGO:      break; // valid value
            case GeoFields::LANCZOS_ALGO:          break; // valid value
            case GeoFields::AVERAGE_ALGO:          break; // valid value
            case GeoFields::MODE_ALGO:             break; // valid value
            case GeoFields::GAUSS_ALGO:            break; // valid value
            default: throw RunTimeException(CRITICAL, RTE_ERROR, "Unknown sampling algorithm: %ld", n);
        }
    }
}

/*----------------------------------------------------------------------------
 * convertToJson - GeoFields::bbox_t
 *----------------------------------------------------------------------------*/
string convertToJson(const GeoFields::bbox_t& v)
{
    return FString("[%lf, %lf, %lf, %lf]", v.lon_min, v.lat_min, v.lon_max, v.lat_max).c_str();
}

/*----------------------------------------------------------------------------
 * convertToLua - GeoFields::bbox_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const GeoFields::bbox_t& v)
{
    lua_newtable(L);

    // lon_min
    lua_pushnumber(L, v.lon_min);
    lua_rawseti(L, -2, 1);

    // lat_min
    lua_pushnumber(L, v.lat_min);
    lua_rawseti(L, -2, 2);

    // lon_max
    lua_pushnumber(L, v.lon_max);
    lua_rawseti(L, -2, 2);

    // lat_max
    lua_pushnumber(L, v.lat_max);
    lua_rawseti(L, -2, 2);

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - GeoFields::bbox_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, GeoFields::bbox_t& v)
{
    if(lua_istable(L, index))
    {
        const int num_points = lua_rawlen(L, index);
        if(num_points == 4)
        {
            // lon_min
            lua_rawgeti(L, index, 1);
            v.lon_min = LuaObject::getLuaFloat(L, -1);
            lua_pop(L, 1);

            // lat_min
            lua_rawgeti(L, index, 2);
            v.lat_min = LuaObject::getLuaFloat(L, -1);
            lua_pop(L, 1);

            // lon_max
            lua_rawgeti(L, index, 3);
            v.lon_max = LuaObject::getLuaFloat(L, -1);
            lua_pop(L, 1);

            // lat_max
            lua_rawgeti(L, index, 4);
            v.lat_max = LuaObject::getLuaFloat(L, -1);
            lua_pop(L, 1);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "bounding box must be supplied as a table of four points [lon_min, lat_min, lon_max, lat_max]");
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "bounding box must be supplied as a table [lon_min, lat_min, lon_max, lat_max]");
    }
}
