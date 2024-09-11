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
#include "GeoParms.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoParms::SELF                  = "samples";
const char* GeoParms::SAMPLING_ALGO         = "algorithm";
const char* GeoParms::SAMPLING_RADIUS       = "radius";
const char* GeoParms::ZONAL_STATS           = "zonal_stats";
const char* GeoParms::FLAGS_FILE            = "with_flags";
const char* GeoParms::START_TIME            = "t0";
const char* GeoParms::STOP_TIME             = "t1";
const char* GeoParms::URL_SUBSTRING         = "substr";
const char* GeoParms::CLOSEST_TIME          = "closest_time";
const char* GeoParms::USE_POI_TIME          = "use_poi_time";
const char* GeoParms::DOY_RANGE             = "doy_range";
const char* GeoParms::PROJ_PIPELINE         = "proj_pipeline";
const char* GeoParms::AOI_BBOX              = "aoi_bbox";
const char* GeoParms::CATALOG               = "catalog";
const char* GeoParms::BANDS                 = "bands";
const char* GeoParms::ASSET                 = "asset";
const char* GeoParms::KEY_SPACE             = "key_space";

const char* GeoParms::NEARESTNEIGHBOUR_ALGO = "NearestNeighbour";
const char* GeoParms::BILINEAR_ALGO         = "Bilinear";
const char* GeoParms::CUBIC_ALGO            = "Cubic";
const char* GeoParms::CUBICSPLINE_ALGO      = "CubicSpline";
const char* GeoParms::LANCZOS_ALGO          = "Lanczos";
const char* GeoParms::AVERAGE_ALGO          = "Average";
const char* GeoParms::MODE_ALGO             = "Mode";
const char* GeoParms::GAUSS_ALGO            = "Gauss";

const char* GeoParms::OBJECT_TYPE           = "GeoParms";
const char* GeoParms::LUA_META_NAME         = "GeoParms";
const struct luaL_Reg GeoParms::LUA_META_TABLE[] = {
    {"name",        luaAssetName},
    {"region",      luaAssetRegion},
    {"keyspace",    luaSetKeySpace},
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
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoParms::GeoParms (lua_State* L, int index, bool asset_required):
    LuaObject           (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    sampling_algo       (GRIORA_NearestNeighbour),
    sampling_radius     (0),
    zonal_stats         (false),
    flags_file          (false),
    filter_time         (false),
    url_substring       (NULL),
    filter_closest_time (false),
    use_poi_time        (false),
    filter_doy_range    (false),
    doy_keep_inrange    (true),
    doy_start           (0),
    doy_end             (0),
    proj_pipeline       (NULL),
    aoi_bbox            {0, 0, 0, 0},
    catalog             (NULL),
    bands_list          (8),
    bands               (NULL),
    asset_name          (NULL),
    asset               (NULL),
    key_space           (0)
{
    /* Populate Object */
    try
    {
        /* Must be a Table */
        if(L && lua_istable(L, index))
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
            sampling_radius = (int)LuaObject::getLuaInteger(L, -1, true, sampling_radius, &field_provided);
            if(sampling_radius < 0) throw RunTimeException(CRITICAL, RTE_ERROR, "invalid sampling radius: %d:", sampling_radius);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", SAMPLING_RADIUS, (int)sampling_radius);
            lua_pop(L, 1);

            /* Zonal Statistics */
            lua_getfield(L, index, ZONAL_STATS);
            zonal_stats = LuaObject::getLuaBoolean(L, -1, true, zonal_stats, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", ZONAL_STATS, (int)zonal_stats);
            lua_pop(L, 1);

            /* Flags File */
            lua_getfield(L, index, FLAGS_FILE);
            flags_file = LuaObject::getLuaBoolean(L, -1, true, flags_file, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", FLAGS_FILE, (int)flags_file);
            lua_pop(L, 1);

            /* Start Time */
            lua_getfield(L, index, START_TIME);
            const char* t0_str = LuaObject::getLuaString(L, -1, true, NULL);
            if(t0_str)
            {
                const int64_t gps = TimeLib::str2gpstime(t0_str);
                if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", t0_str);
                start_time = TimeLib::gps2gmttime(gps);
                filter_time = true;
                const TimeLib::date_t start_date = TimeLib::gmt2date(start_time);
                mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", START_TIME, start_date.year, start_date.month, start_date.day, start_time.hour, start_time.minute, start_time.second);
            }
            lua_pop(L, 1);

            /* Stop Time */
            lua_getfield(L, index, STOP_TIME);
            const char* t1_str = LuaObject::getLuaString(L, -1, true, NULL);
            if(t1_str)
            {
                const int64_t gps = TimeLib::str2gpstime(t1_str);
                if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", t0_str);
                stop_time = TimeLib::gps2gmttime(gps);
                filter_time = true;
                const TimeLib::date_t stop_date = TimeLib::gmt2date(stop_time);
                mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", STOP_TIME, stop_date.year, stop_date.month, stop_date.day, stop_time.hour, stop_time.minute, stop_time.second);
            }
            lua_pop(L, 1);

            /* Start and Stop Time Special Cases */
            if(t0_str && !t1_str) // only start time supplied
            {
                const int64_t now = TimeLib::gpstime();
                stop_time = TimeLib::gps2gmttime(now);
                const TimeLib::date_t stop_date = TimeLib::gmt2date(stop_time);
                mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", STOP_TIME, stop_date.year, stop_date.month, stop_date.day, stop_time.hour, stop_time.minute, stop_time.second);
            }
            else if(!t0_str && t1_str) // only stop time supplied
            {
                const int64_t gps_epoch = 0;
                start_time = TimeLib::gps2gmttime(gps_epoch);
                const TimeLib::date_t start_date = TimeLib::gmt2date(start_time);
                mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", START_TIME, start_date.year, start_date.month, start_date.day, start_time.hour, start_time.minute, start_time.second);
            }

            /* URL Substring Filter */
            lua_getfield(L, index, URL_SUBSTRING);
            url_substring = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL));
            if(url_substring) mlog(DEBUG, "Setting %s to %s", URL_SUBSTRING, url_substring);
            lua_pop(L, 1);

            /* Closest Time Filter */
            lua_getfield(L, index, CLOSEST_TIME);
            const char* closest_time_str = LuaObject::getLuaString(L, -1, true, NULL);
            if(closest_time_str)
            {
                const int64_t gps = TimeLib::str2gpstime(closest_time_str);
                if(gps <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse time supplied: %s", closest_time_str);
                filter_closest_time = true;
                closest_time = TimeLib::gps2gmttime(gps);
                const TimeLib::date_t closest_date = TimeLib::gmt2date(closest_time);
                mlog(DEBUG, "Setting %s to %04d-%02d-%02dT%02d:%02d:%02dZ", CLOSEST_TIME, closest_date.year, closest_date.month, closest_date.day, closest_time.hour, closest_time.minute, closest_time.second);
            }
            lua_pop(L, 1);

            /* Use Point of Interest Time */
            lua_getfield(L, index, USE_POI_TIME);
            use_poi_time = LuaObject::getLuaBoolean(L, -1, true, use_poi_time, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", USE_POI_TIME, (int)use_poi_time);
            lua_pop(L, 1);

            /* Day Of Year Range Filter */
            lua_getfield(L, index, DOY_RANGE);
            const char* doy_range_str = LuaObject::getLuaString(L, -1, true, NULL);
            if(doy_range_str)
            {
                /* Do we keep in range 'dd:dd' or remove '!dd:dd' */
                if(doy_range_str[0] == '!')
                {
                    doy_keep_inrange = false;
                    doy_range_str++;
                }
                if(!TimeLib::str2doyrange(doy_range_str, doy_start, doy_end))
                    throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse day of year range supplied: %s", doy_range_str);

                if((doy_start > doy_end) || (doy_start == doy_end) ||
                   (doy_start < 1 || doy_start > 366) ||
                   (doy_end   < 1 || doy_end   > 366))
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid day of year range: %d:%d", doy_start, doy_end);

                filter_doy_range = true;
                mlog(DEBUG, "Setting %s to %02d:%02d, doy_keep_inrange: %s", DOY_RANGE, doy_start, doy_end, doy_keep_inrange ? "true" : "false");
            }
            lua_pop(L, 1);

            /* PROJ pipeline for projection transform */
            lua_getfield(L, index, PROJ_PIPELINE);
            proj_pipeline = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL));
            if(proj_pipeline) mlog(DEBUG, "Setting %s to %s", PROJ_PIPELINE, proj_pipeline);
            lua_pop(L, 1);

            /* AOI BBOX */
            lua_getfield(L, index, AOI_BBOX);
            getAoiBbox(L, -1, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to [%.4lf, %.4lf, %.4lf, %.4lf]", AOI_BBOX, aoi_bbox.lon_min, aoi_bbox.lat_min, aoi_bbox.lon_max, aoi_bbox.lat_max);
            lua_pop(L, 1);

            /* Catalog */
            lua_getfield(L, index, CATALOG);
            const char* catalog_str = LuaObject::getLuaString(L, -1, true, NULL);
            catalog = StringLib::duplicate(catalog_str);
            if(catalog) mlog(DEBUG, "Setting %s to user provided geojson", CATALOG);
            lua_pop(L, 1);

            /* Bands */
            lua_getfield(L, index, BANDS);
            getLuaBands(L, -1, &field_provided);
            if(field_provided)
            {
                bands = new band_list_t::Iterator(bands_list);
                mlog(DEBUG, "Setting %s to user provided selection", BANDS);
            }
            lua_pop(L, 1);

            /* Asset */
            lua_getfield(L, index, ASSET);
            asset_name = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL));
            if(asset_name)
            {
                asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE));
                if(!asset && asset_required) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to find asset %s", asset_name);
                mlog(DEBUG, "Setting %s to %s", ASSET, asset_name);
            }
            lua_pop(L, 1);

            /* Key Space */
            lua_getfield(L, index, KEY_SPACE);
            key_space = (uint64_t)LuaObject::getLuaInteger(L, -1, true, key_space, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %lu", KEY_SPACE, (unsigned long)key_space);
            lua_pop(L, 1);
        }
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoParms::~GeoParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * tojson
 *----------------------------------------------------------------------------*/
const char* GeoParms::tojson(void) const
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value nullval(rapidjson::kNullType);

    doc.AddMember("sampling_algo", rapidjson::Value(algo2str(sampling_algo), allocator), allocator);
    doc.AddMember("sampling_radius", sampling_radius, allocator);
    doc.AddMember("zonal_stats", zonal_stats, allocator);
    doc.AddMember("flags_file", flags_file, allocator);
    doc.AddMember("filter_time", filter_time, allocator);

    if(url_substring) doc.AddMember("url_substring", rapidjson::Value(url_substring, allocator), allocator);
    else              doc.AddMember("url_substring", nullval, allocator);

    doc.AddMember("filter_closest_time", filter_closest_time, allocator);
    doc.AddMember("use_poi_time", use_poi_time, allocator);
    doc.AddMember("filter_doy_range", filter_doy_range, allocator);
    doc.AddMember("doy_keep_inrange", doy_keep_inrange, allocator);
    doc.AddMember("doy_start", doy_start, allocator);
    doc.AddMember("doy_end", doy_end, allocator);

    if(proj_pipeline) doc.AddMember("proj_pipeline", rapidjson::Value(proj_pipeline, allocator), allocator);
    else              doc.AddMember("proj_pipeline", nullval, allocator);

    doc.AddMember("aoi_bbox", rapidjson::Value().SetArray().PushBack(aoi_bbox.lon_min, allocator).PushBack(aoi_bbox.lat_min, allocator).PushBack(aoi_bbox.lon_max, allocator).PushBack(aoi_bbox.lat_max, allocator), allocator);

    if(catalog) doc.AddMember("catalog", rapidjson::Value(catalog, allocator), allocator);
    else        doc.AddMember("catalog", nullval, allocator);

    if(bands)
    {
        rapidjson::Value _bands_list(rapidjson::kArrayType);
        for(int i = 0; i < bands->length; i++)
        {
            const char* name = (*bands)[i].c_str();
            if(name) _bands_list.PushBack(rapidjson::Value(name, allocator), allocator);
        }
        doc.AddMember("bands_list", _bands_list, allocator);
    }
    else doc.AddMember("bands_list", rapidjson::Value("[]"), allocator);

    if(asset_name) doc.AddMember("asset_name", rapidjson::Value(asset_name, allocator), allocator);
    else           doc.AddMember("asset_name", nullval, allocator);

    if(asset)
    {
        const char* asset_json = asset->tojson();
        rapidjson::Document asset_doc;
        asset_doc.Parse(asset_json);
        rapidjson::Value asset_val(rapidjson::kObjectType);
        asset_val.CopyFrom(asset_doc, allocator);    // Deep copy the parsed document into a value
        doc.AddMember("asset", asset_val, allocator);
        delete [] asset_json;
    }
    else doc.AddMember("asset", nullval, allocator);

    doc.AddMember("key_space", key_space, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return StringLib::duplicate(buffer.GetString());
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void GeoParms::cleanup (void)
{
    if(url_substring)
    {
        delete [] url_substring;
        url_substring = NULL;
    }

    if(catalog)
    {
        delete [] catalog;
        catalog = NULL;
    }

    if(asset_name)
    {
        delete [] asset_name;
        asset_name = NULL;
    }

    if(asset)
    {
        asset->releaseLuaObject();
        asset = NULL;
    }

    if(proj_pipeline)
    {
        delete [] proj_pipeline;
        proj_pipeline = NULL;
    }

    if(bands)
    {
        delete bands;
        bands = NULL;
    }
}

/*----------------------------------------------------------------------------
 * str2algo
 *----------------------------------------------------------------------------*/
int GeoParms::str2algo (const char* str)
{
    if(!str)                                         return static_cast<int>(GRIORA_NearestNeighbour);
    if(StringLib::match(str, NEARESTNEIGHBOUR_ALGO)) return static_cast<int>(GRIORA_NearestNeighbour);
    if(StringLib::match(str, BILINEAR_ALGO))         return static_cast<int>(GRIORA_Bilinear);
    if(StringLib::match(str, CUBIC_ALGO))            return static_cast<int>(GRIORA_Cubic);
    if(StringLib::match(str, CUBICSPLINE_ALGO))      return static_cast<int>(GRIORA_CubicSpline);
    if(StringLib::match(str, LANCZOS_ALGO))          return static_cast<int>(GRIORA_Lanczos);
    if(StringLib::match(str, AVERAGE_ALGO))          return static_cast<int>(GRIORA_Average);
    if(StringLib::match(str, MODE_ALGO))             return static_cast<int>(GRIORA_Mode);
    if(StringLib::match(str, GAUSS_ALGO))            return static_cast<int>(GRIORA_Gauss);
    throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling algorithm: %s:", str);
}

/*----------------------------------------------------------------------------
 * algo2str
 *----------------------------------------------------------------------------*/
const char* GeoParms::algo2str(int algo)
{
    switch(algo)
    {
        case GRIORA_NearestNeighbour: return NEARESTNEIGHBOUR_ALGO;
        case GRIORA_Bilinear:         return BILINEAR_ALGO;
        case GRIORA_Cubic:            return CUBIC_ALGO;
        case GRIORA_CubicSpline:      return CUBICSPLINE_ALGO;
        case GRIORA_Lanczos:          return LANCZOS_ALGO;
        case GRIORA_Average:          return AVERAGE_ALGO;
        case GRIORA_Mode:             return MODE_ALGO;
        case GRIORA_Gauss:            return GAUSS_ALGO;
        default:                      return "Unknown";
    }
}

/*----------------------------------------------------------------------------
 * getLuaBands
 *----------------------------------------------------------------------------*/
void GeoParms::getLuaBands (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    if(provided) *provided = false;

    if(lua_istable(L, index))
    {
        /* Get number of bands in table */
        const int num_bands = lua_rawlen(L, index);
        if(num_bands > 0 && provided) *provided = true;

        /* Iterate through each band in table */
        for(int i = 0; i < num_bands; i++)
        {
            /* Add band */
            lua_rawgeti(L, index, i+1);
            const string band_str(LuaObject::getLuaString(L, -1));
            bands_list.add(band_str);
            lua_pop(L, 1);
        }
    }
    else if(lua_isstring(L, index))
    {
        if(provided) *provided = true;

        /* Add band */
        const string band_str(LuaObject::getLuaString(L, -1));
        bands_list.add(band_str);
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "Bands must be provided as a table or string");
    }
}

/*----------------------------------------------------------------------------
 * getAoiBbox - [min_lon, min_lat, max_lon, max_lat]
 *----------------------------------------------------------------------------*/
void GeoParms::getAoiBbox (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of coordinates */
    if(!lua_istable(L, index))
    {
        mlog(DEBUG, "bounding box must be supplied as a table");
        return;
    }

    /* Get Number of Points in BBOX */
    const int num_points = lua_rawlen(L, index);
    if(num_points != 4)
    {
        mlog(ERROR, "bounding box must be supplied as four points");
        return;
    }

    /* Get Minimum Longitude */
    lua_rawgeti(L, index, 1);
    aoi_bbox.lon_min = LuaObject::getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Get Minimum Latitude */
    lua_rawgeti(L, index, 2);
    aoi_bbox.lat_min = LuaObject::getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Get Maximum Longitude */
    lua_rawgeti(L, index, 3);
    aoi_bbox.lon_max = LuaObject::getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Get Maximum Latitude */
    lua_rawgeti(L, index, 4);
    aoi_bbox.lat_max = LuaObject::getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Set Provided */
    *provided = true;
}

/*----------------------------------------------------------------------------
 * luaAssetName
 *----------------------------------------------------------------------------*/
int GeoParms::luaAssetName (lua_State* L)
{
    try
    {
        const GeoParms* lua_obj = dynamic_cast<GeoParms*>(getLuaSelf(L, 1));
        if(lua_obj->asset_name) lua_pushstring(L, lua_obj->asset_name);
        else lua_pushnil(L);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaAssetRegion
 *----------------------------------------------------------------------------*/
int GeoParms::luaAssetRegion (lua_State* L)
{
    try
    {
        const GeoParms* lua_obj = dynamic_cast<GeoParms*>(getLuaSelf(L, 1));
        if(lua_obj->asset) lua_pushstring(L, lua_obj->asset->getRegion());
        else lua_pushnil(L);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaSetKeySpace
 *----------------------------------------------------------------------------*/
int GeoParms::luaSetKeySpace (lua_State* L)
{
    try
    {
        GeoParms* lua_obj = dynamic_cast<GeoParms*>(getLuaSelf(L, 1));
        const uint64_t key_space = (uint64_t)getLuaInteger(L, 2);

        lua_obj->key_space = key_space;

        return returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}
