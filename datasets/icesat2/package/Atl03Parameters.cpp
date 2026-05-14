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
#include "Atl03Parameters.h"
#include "FieldMap.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Parameters::LUA_META_NAME = "Atl03Parameters";

 /******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - FitFields
 *----------------------------------------------------------------------------*/
FitFields::FitFields():
    FieldMap<Field>({ {"maxi",          &maxIterations,         "Maximum number of iterations for the Surface Fitting algorithm to run when fitting a line to the photons in a segment"},
                      {"H_min_win",     &minWindow,             "Minimum vertical window used by the Surface Fitting algorithm when fitting a line to the photons in a segment"},
                      {"sigma_r_max",   &maxRobustDispersion,   "Maximum robust dispersion used by the Surface Fitting algorithm when fitting a line to the photons in a segment"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - FitFields
 *----------------------------------------------------------------------------*/
void FitFields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        FieldMap<Field>::fromLua(L, index);
        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * Constructor - YapcFields
 *----------------------------------------------------------------------------*/
YapcFields::YapcFields():
    FieldMap<Field>({ {"score",     &score,     "Photon density score"},
                      {"version",   &version,   "YAPC algorithm version to execute, (0: pull from ATL03 granule, 3: latest algorithm)"},
                      {"knn",       &knn,       "K-nearest neighbors; version 2 only"},
                      {"min_knn",   &min_knn,   "Minimum number of k-nearest neighbors; version 3 only"},
                      {"win_h",     &win_h,     "Window height (overrides calculated value if non-zero)"},
                      {"win_x",     &win_x,     "Window width"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - YapcFields
 *----------------------------------------------------------------------------*/
void YapcFields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        FieldMap<Field>::fromLua(L, index);
        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * Constructor - PhorealFields
 *----------------------------------------------------------------------------*/
PhorealFields::PhorealFields():
    FieldMap<Field>({ {"binsize",           &binsize,               "Vertical resolution (in meters) over which canopy statistics are calculated"},
                      {"geoloc",            &geoloc,                "Controls how the geolocation of the metrics are calculated"},
                      {"use_abs_h",         &use_abs_h,             "Use absolute height values when calculating the metrics; this is non-standard and for special cases only"},
                      {"send_waveform",     &send_waveform,         "Send the full vertical reconstructed waveform used to calculate the metrics; only supported by 'atl08p'"},
                      {"above_classifier",  &above_classifier,      "Use the ABoVE classification algorithm for canopy photons"},
                      {"te_quality_filter", &te_quality_filter,     "Filter out low quality terrain photons when calculating metrics"},
                      {"can_quality_filter",&can_quality_filter,    "Filter out low quality canopy photons when calculating metrics"} }),
    te_quality_filter_provided(false),
    can_quality_filter_provided(false),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - PhorealFields
 *----------------------------------------------------------------------------*/
void PhorealFields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        lua_getfield(L, index, "te_quality_filter");
        te_quality_filter_provided = !lua_isnil(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "can_quality_filter");
        can_quality_filter_provided = !lua_isnil(L, -1);
        lua_pop(L, 1);

        FieldMap<Field>::fromLua(L, index);

        if(binsize.value <= 0.0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid binsize: %lf", binsize.value);
        }

        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * Constructor - BlanketFields
 *----------------------------------------------------------------------------*/
BlanketFields::BlanketFields():
    FieldMap<Field>({ {"max_top_of_surface_percentile", &max_top_of_surface_percentile, "Percentile to use when calculating the top of the surface"},
                      {"median_ground_percentile",      &median_ground_percentile,      "Percentile to use when calculating the ground"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - BlanketFields
 *----------------------------------------------------------------------------*/
void BlanketFields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        FieldMap<Field>::fromLua(L, index);
        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * Constructor - Atl24Fields
 *----------------------------------------------------------------------------*/
Atl24Fields::Atl24Fields():
    FieldMap<Field>({ {"compact",               &compact,               "Boolean flag indicating that only a compact set of fields should be returned"},
                      {"class_ph",              &class_ph,              "List of ATL24 photon classifications that are to be included in the response"},
                      {"confidence_threshold",  &confidence_threshold,  "Minimum confidence value of bathymetry photons to be returned (0.0 to 1.0)"},
                      {"invalid_kd",            &invalid_kd,            "Boolean flag controlling whether photons with invalid Kd data is returned"},
                      {"invalid_wind_speed",    &invalid_wind_speed,    "Boolean flag controlling whether photons with invalid wind speed data is returned"},
                      {"low_confidence",        &low_confidence,        "Boolean flag controlling whether photons with low confidence should be returned (short cut for manually setting the confidence threshold)"},
                      {"night",                 &night,                 "Boolean flag controlling whether photons collected at night should be returned"},
                      {"sensor_depth_exceeded", &sensor_depth_exceeded, "Boolean flag controlling whether photons that exceeded the sensor depth should be returned"},
                      {"anc_fields",            &anc_fields,            "Ancillary fields from the source granules to include in the response"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - Atl24Fields
 *----------------------------------------------------------------------------*/
void Atl24Fields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        FieldMap<Field>::fromLua(L, index);

        if( invalid_kd.anyDisabled() ||
            invalid_wind_speed.anyDisabled() ||
            low_confidence.anyDisabled() ||
            night.anyDisabled() ||
            sensor_depth_exceeded.anyDisabled() )
        {
            compact = false;
        }

        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void Atl03Parameters::fromLua (lua_State* L, int index)
{
    Icesat2Parameters::fromLua(L, index);

    // handle signal confidence options
    if(atl03Cnf.providedAsSingle)
    {
        // when signal confidence is supplied as a single option
        // instead of setting only that option, treat it as a level
        // where every selection that is that option or above is set
        bool selection = false;
        for(int i = 0; i < NUM_SIGNAL_CONF; i++)
        {
            // set every element true after the first one found that is set to true
            atl03Cnf.values[i] = selection || atl03Cnf.values[i];
            selection = atl03Cnf.values[i];
        }
    }

    // handle YAPC options
    if(yapc.provided)
    {
        stages[STAGE_YAPC] = true;
    }

    // handle atl08 class options
    if(atl08Class.anyEnabled())
    {
        stages[STAGE_ATL08] = true;
    }

    // handle Surface Fitter options
    if(fit.provided)
    {
        stages[STAGE_FITTER] = true;
    }

    // handle PhoREAL options
    if(phoreal.provided)
    {
        stages[STAGE_PHOREAL] = true;
        if(!stages[STAGE_ATL08])
        {
            // if atl08 processing not enabled, then enable it
            // and default photon classes to a reasonable request
            stages[STAGE_ATL08] = true;
            atl08Class[ATL08_NOISE] = false;
            atl08Class[ATL08_GROUND] = true;
            atl08Class[ATL08_CANOPY] = true;
            atl08Class[ATL08_TOP_OF_CANOPY] = true;
            atl08Class[ATL08_UNCLASSIFIED] = false;
        }
    }

    // handle SurfaceBlanket options
    if(blanket.provided)
    {
        stages[STAGE_BLANKET] = true;
        stages[STAGE_ATL08] = true;
        atl08Class[ATL08_GROUND] = true;
        atl08Class[ATL08_CANOPY] = true;
        atl08Class[ATL08_TOP_OF_CANOPY] = true;
    }

    // handle ATL24 options
    if(atl24.provided)
    {
        stages[STAGE_ATL24] = true;
    }

    // handle ATL08 fields
    if(!atl08Fields.values.empty())
    {
        if(!stages[STAGE_ATL08])
        {
            // if atl08 processing not enabled, then enable it
            // and default all classified photons to on
            stages[STAGE_ATL08] = true;
            atl08Class[ATL08_NOISE] = true;
            atl08Class[ATL08_GROUND] = true;
            atl08Class[ATL08_CANOPY] = true;
            atl08Class[ATL08_TOP_OF_CANOPY] = true;
            atl08Class[ATL08_UNCLASSIFIED] = false;
        }
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03Parameters::Atl03Parameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name):
    Icesat2Parameters (L, key_space, asset_name, _resource, lua_meta_name)
{
    addParameter("srt",                 &surfaceType,           "Surface reference type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water");
    addParameter("pass_invalid",        &passInvalid,           "Boolean flag indicating whether or not extents that fail validation checks are still used and returned in the results");
    addParameter("dist_in_seg",         &distInSeg,             "Boolean flag indicating that the units of the len and res are in ATL03 segments (e.g. if true then len=2 is exactly two ATL03 segments which is approximately 40 meters)");
    addParameter("cnf",                 &atl03Cnf,              "Confidence level for photon selection, can be supplied as a single value (which means the confidence must be at least that), or a list (which means the confidence must be in the list); note - the confidence can be supplied as strings or as numbers {-2, -1, 0, 1, 2, 3, 4}");
    addParameter("quality_ph",          &qualityPh,             "Quality classification based on an ATL03 algorithms that attempt to identify instrumental artifacts, can be supplied as a single value (which means the classification must be exactly that), or a list (which means the classification must be in the list)");
    addParameter("atl08_class",         &atl08Class,            "List of ATL08 classifications used to select which photons are used in the processing");
    addParameter("cnt",                 &minPhotonCount,        "Minimum photon count in the variable length segment for the segment to be processed");
    addParameter("ats",                 &minAlongTrackSpread,   "Minimum along track spread of the photons (in meters) in the variable length segment for the segment to be processed");
    addParameter("len",                 &extentLength,          "Size (in meters) of the variable length segment");
    addParameter("res",                 &extentStep,            "Step (in meters) of the variable length segments; could also be thought of as the spacing of the segments or the resolution of the segments");
    addParameter("podppd",              &podppdMask,            "Pointing/geolocation degradation mask; each bit in the mask represents a pointing/geolocation solution quality assessment to be included; the bits are 0: nominal, 1: pod_degrade, 2: ppd_degrade, 3: podppd_degrade, 4: cal_nominal, 5: cal_pod_degrade, 6: cal_ppd_degrade, 7: cal_podppd_degrade");
    addParameter("fit",                 &fit,                   "Configuration structure for the 'Surface Fitting' algorithm; when provided the servers will fit a surface to the source photon cloud and return an elevation dataset similar to ATL06");
    addParameter("yapc",                &yapc,                  "Configuration structure for the 'Yet Another Photon Classifier' algorithm; when provided the servers will calculate a density score for each photon and include that score in the response data");
    addParameter("phoreal",             &phoreal,               "Configuration structure for the 'PhoREAL' algorithm; when provided the servers will calculate canopy metrics on the source photon cloud and return those metrics as a dataset similar to ATL08");
    addParameter("als",                 &blanket,               "Configuration structure for the 'Surface Blanket' algorithm; when provided the servers will calculate a canopy top and ground using the source photon cloud and return those values in the response");
    addParameter("atl24",               &atl24,                 "Configuration for classifying bathymetry photons");
    addParameter("atl03_bckgrd_fields", &atl03BckgrdFields,     "Ancillary fields in the 'bckgrd_atlas' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x");
    addParameter("atl03_geo_fields",    &atl03GeoFields,        "Ancillary fields in the 'geolocation' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x");
    addParameter("atl03_corr_fields",   &atl03CorrFields,       "Ancillary fields in the 'geophys_corr' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x");
    addParameter("atl03_ph_fields",     &atl03PhFields,         "Ancillary fields in the 'heights' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x");
    addParameter("atl08_fields",        &atl08Fields,           "Ancillary fields in the 'land_segments' group of the ATL08 granule to include in the response; supported by atl08x");
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson - phoreal_geoloc_t
 *----------------------------------------------------------------------------*/
string convertToJson(const PhorealFields::phoreal_geoloc_t& v)
{
    switch(v)
    {
        case PhorealFields::MEAN:   return "\"mean\"";
        case PhorealFields::MEDIAN: return "\"median\"";
        case PhorealFields::CENTER: return "\"center\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid PhoREAL geolocation: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - phoreal_geoloc_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const PhorealFields::phoreal_geoloc_t& v)
{
    switch(v)
    {
        case PhorealFields::MEAN:   lua_pushstring(L, "mean");          break;
        case PhorealFields::MEDIAN: lua_pushstring(L, "median");        break;
        case PhorealFields::CENTER: lua_pushstring(L, "center");        break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid PhoREAL geolocation: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - phoreal_geoloc_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, PhorealFields::phoreal_geoloc_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<PhorealFields::phoreal_geoloc_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* fmt_str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(fmt_str, "mean"))      v = PhorealFields::MEAN;
        else if(StringLib::match(fmt_str, "median"))    v = PhorealFields::MEDIAN;
        else if(StringLib::match(fmt_str, "center"))    v = PhorealFields::CENTER;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "geolocation is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "geolocation is an invalid type:%d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToJson - class_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Atl24Fields::class_t& v)
{
    switch(v)
    {
        case Atl24Fields::UNCLASSIFIED: return "\"unclassified\"";
        case Atl24Fields::BATHYMETRY:   return "\"bathymetry\"";
        case Atl24Fields::SEA_SURFACE:  return "\"sea_surface\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid bathy class: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - class_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Atl24Fields::class_t& v)
{
    switch(v)
    {
        case Atl24Fields::UNCLASSIFIED: lua_pushstring(L, "unclassified");  break;
        case Atl24Fields::BATHYMETRY:   lua_pushstring(L, "bathymetry");    break;
        case Atl24Fields::SEA_SURFACE:  lua_pushstring(L, "sea_surface");   break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl24 class: %d", static_cast<int>(v));
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - class_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Atl24Fields::class_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Atl24Fields::class_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "unclassified"))      v = Atl24Fields::UNCLASSIFIED;
        else if(StringLib::match(str, "bathymetry"))        v = Atl24Fields::BATHYMETRY;
        else if(StringLib::match(str, "sea_surface"))       v = Atl24Fields::SEA_SURFACE;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "bathy class is an invalid value: %s", str);
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "bathy class is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - class_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Atl24Fields::class_t& v)
{
    switch(v)
    {
        case Atl24Fields::UNCLASSIFIED: return 0;
        case Atl24Fields::BATHYMETRY:   return 1;
        case Atl24Fields::SEA_SURFACE:  return 2;
        default:                        return -1;
    }
}

/*----------------------------------------------------------------------------
 * convertFromIndex - class_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Atl24Fields::class_t& v)
{
    switch(index)
    {
        case 0:     v = Atl24Fields::UNCLASSIFIED;  break;
        case 1:     v = Atl24Fields::BATHYMETRY;    break;
        case 2:     v = Atl24Fields::SEA_SURFACE;   break;
        default:    v = Atl24Fields::NUM_CLASSES;   break;
    }
}

/*----------------------------------------------------------------------------
 * convertToJson - flag_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Atl24Fields::flag_t& v)
{
    switch(v)
    {
        case Atl24Fields::FLAG_OFF: return "\"off\"";
        case Atl24Fields::FLAG_ON:  return "\"on\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid filter flag: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - flag_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Atl24Fields::flag_t& v)
{
    switch(v)
    {
        case Atl24Fields::FLAG_OFF: lua_pushstring(L, "off");   break;
        case Atl24Fields::FLAG_ON:  lua_pushstring(L, "on");    break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid filter flag: %d", static_cast<int>(v));
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - flag_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Atl24Fields::flag_t& v)
{
    if(lua_isboolean(L, index))
    {
        const bool f = LuaObject::getLuaBoolean(L, index);
        if(f)   v = Atl24Fields::FLAG_ON;
        else    v = Atl24Fields::FLAG_OFF;
    }
    else if(lua_isinteger(L, index))
    {
        const long f = LuaObject::getLuaInteger(L, index);
        switch(f)
        {
            case 0:     v = Atl24Fields::FLAG_OFF;  break;
            case 1:     v = Atl24Fields::FLAG_ON;   break;
            default:    throw RunTimeException(CRITICAL, RTE_FAILURE, "flag filter is an invalid value: %ld", f);
        }
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "off"))   v = Atl24Fields::FLAG_OFF;
        else if(StringLib::match(str, "on"))    v = Atl24Fields::FLAG_ON;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "flag filter is an invalid value: %s", str);
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "flag filter is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - flag_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Atl24Fields::flag_t& v)
{
    switch(v)
    {
        case Atl24Fields::FLAG_OFF: return 0;
        case Atl24Fields::FLAG_ON:  return 1;
        default:                    return 0;
    }
}

/*----------------------------------------------------------------------------
 * convertFromIndex - flag_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Atl24Fields::flag_t& v)
{
    switch(index)
    {
        case 0:     v = Atl24Fields::FLAG_OFF;  break;
        case 1:     v = Atl24Fields::FLAG_ON;   break;
        default:    v = Atl24Fields::FLAG_OFF;  break;
    }
}
