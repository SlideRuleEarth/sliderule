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
#include "Icesat2Fields.h"
#include "FieldMap.h"
#include "LuaObject.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - Atl03GranuleFields
 *----------------------------------------------------------------------------*/
Atl03GranuleFields::Atl03GranuleFields():
    FieldMap<Field>({ {"year",      &year,      "Year of data acquisition"},
                      {"month",     &month,     "Month of data acquisition"},
                      {"day",       &day,       "Day of data acquisition"},
                      {"rgt",       &rgt,       "ICESat-2 Reference Ground Track"},
                      {"cycle",     &cycle,     "ICESat-2 Cycle Number"},
                      {"region",    &region,    "ICESat-2 Region (0 to 14, see ATL03 ATBD)"},
                      {"version",   &version,   "ICESat-2 Standard Data Product Version"} })
{
}

/*----------------------------------------------------------------------------
 * parseResource
 *
 *  ATLxx_YYYYMMDDHHMMSS_ttttccrr_vvv_ee
 *      YYYY    - year
 *      MM      - month
 *      DD      - day
 *      HH      - hour
 *      MM      - minute
 *      SS      - second
 *      tttt    - reference ground track
 *      cc      - cycle
 *      rr      - region
 *      vvv     - version
 *      ee      - revision
 *----------------------------------------------------------------------------*/
void Atl03GranuleFields::parseResource (const char* resource)
{
    long val;

    /* check resource */
    const int strsize = StringLib::size(resource);
    if(strsize < 33 || resource[0] != 'A' || resource[1] !='T' || resource[2] != 'L')
    {
        return; // not an ICESat-2 standard data product
    }

    /* get year */
    char year_str[5];
    year_str[0] = resource[6];
    year_str[1] = resource[7];
    year_str[2] = resource[8];
    year_str[3] = resource[9];
    year_str[4] = '\0';
    if(StringLib::str2long(year_str, &val, 10))
    {
        year = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse year from resource %s: %s", resource, year_str);
    }

    /* get month */
    char month_str[3];
    month_str[0] = resource[10];
    month_str[1] = resource[11];
    month_str[2] = '\0';
    if(StringLib::str2long(month_str, &val, 10))
    {
        month = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse month from resource %s: %s", resource, month_str);
    }

    /* get day */
    char day_str[3];
    day_str[0] = resource[12];
    day_str[1] = resource[13];
    day_str[2] = '\0';
    if(StringLib::str2long(day_str, &val, 10))
    {
        day = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse day from resource %s: %s", resource, day_str);
    }

    /* get RGT */
    char rgt_str[5];
    rgt_str[0] = resource[21];
    rgt_str[1] = resource[22];
    rgt_str[2] = resource[23];
    rgt_str[3] = resource[24];
    rgt_str[4] = '\0';
    if(StringLib::str2long(rgt_str, &val, 10))
    {
        rgt = static_cast<uint16_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse RGT from resource %s: %s", resource, rgt_str);
    }

    /* get cycle */
    char cycle_str[3];
    cycle_str[0] = resource[25];
    cycle_str[1] = resource[26];
    cycle_str[2] = '\0';
    if(StringLib::str2long(cycle_str, &val, 10))
    {
        cycle = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse cycle from resource %s: %s", resource, cycle_str);
    }

    /* get region */
    char region_str[3];
    region_str[0] = resource[27];
    region_str[1] = resource[28];
    region_str[2] = '\0';
    if(StringLib::str2long(region_str, &val, 10))
    {
        region = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse region from resource %s: %s", resource, region_str);
    }

    /* get version */
    char version_str[4];
    version_str[0] = resource[30];
    version_str[1] = resource[31];
    version_str[2] = resource[32];
    version_str[3] = '\0';
    if(StringLib::str2long(version_str, &val, 10))
    {
        version = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse version from resource %s: %s", resource, version_str);
    }
}

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
 * Constructor - Atl13Fields
 *----------------------------------------------------------------------------*/
Atl13Fields::Atl13Fields():
    FieldMap<Field>({ {"refid",         &reference_id,      "Selects data associated only with this ATL13 Reference ID of the inland body of water"},
                      {"name",          &name,              "Selects data associated only with this inland body of water name"},
                      {"coord",         &coordinate,        "Selects data associated only with the inland body of water that contains this coordinate"},
                      {"anc_fields",    &anc_fields,        "Ancillary fields from the source granules to include in the response"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - Atl13Fields
 *----------------------------------------------------------------------------*/
void Atl13Fields::fromLua (lua_State* L, int index)
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
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int Icesat2Fields::luaCreate (lua_State* L)
{
    Icesat2Fields* icesat2_fields = NULL;

    try
    {
        const uint64_t key_space = LuaObject::getLuaInteger(L, 2, true, RequestFields::DEFAULT_KEY_SPACE);
        const char* asset_name = LuaObject::getLuaString(L, 3, true, "icesat2");
        const char* _resource = LuaObject::getLuaString(L, 4, true, NULL);

        icesat2_fields = new Icesat2Fields(L, key_space, asset_name, _resource, {});
        icesat2_fields->fromLua(L, 1);

        return createLuaObject(L, icesat2_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        delete icesat2_fields;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void Icesat2Fields::fromLua (lua_State* L, int index)
{
    RequestFields::fromLua(L, index);

    // parse resource name
    if(!resource.value.empty())
    {
        granuleFields.parseResource(resource.value.c_str());
    }

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

    // handle atl09 sampline options
    if(atl09Fields.length() > 0)
    {
        stages[STAGE_ATL09] = true;
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

    // handle track selection override of beams
    if(track.value != ALL_TRACKS)
    {
        switch(track.value)
        {
            case RPT_1:
            {
                beams[GT1L] = true;
                beams[GT1R] = true;
                beams[GT2L] = false;
                beams[GT2R] = false;
                beams[GT3L] = false;
                beams[GT3R] = false;
                break;
            }
            case RPT_2:
            {
                beams[GT1L] = false;
                beams[GT1R] = false;
                beams[GT2L] = true;
                beams[GT2R] = true;
                beams[GT3L] = false;
                beams[GT3R] = false;
                break;
            }
            case RPT_3:
            {
                beams[GT1L] = false;
                beams[GT1R] = false;
                beams[GT2L] = false;
                beams[GT2R] = false;
                beams[GT3L] = true;
                beams[GT3R] = true;
                break;
            }
            default:
            {
                beams[GT1L] = false;
                beams[GT1R] = false;
                beams[GT2L] = false;
                beams[GT2R] = false;
                beams[GT3L] = false;
                beams[GT3R] = false;
                break;
            }
        }
    }

}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Icesat2Fields::Icesat2Fields(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const std::initializer_list<FieldMap<Field>::init_entry_t>& init_list):
    RequestFields (L, key_space, asset_name, _resource, {
        {"srt",                 &surfaceType,           "Surface reference type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water"},
        {"pass_invalid",        &passInvalid,           "Boolean flag indicating whether or not extents that fail validation checks are still used and returned in the results"},
        {"dist_in_seg",         &distInSeg,             "Boolean flag indicating that the units of the len and res are in ATL03 segments (e.g. if true then len=2 is exactly two ATL03 segments which is approximately 40 meters)"},
        {"cnf",                 &atl03Cnf,              "Confidence level for photon selection, can be supplied as a single value (which means the confidence must be at least that), or a list (which means the confidence must be in the list); note - the confidence can be supplied as strings or as numbers {-2, -1, 0, 1, 2, 3, 4}"},
        {"quality_ph",          &qualityPh,             "Quality classification based on an ATL03 algorithms that attempt to identify instrumental artifacts, can be supplied as a single value (which means the classification must be exactly that), or a list (which means the classification must be in the list)"},
        {"atl08_class",         &atl08Class,            "List of ATL08 classifications used to select which photons are used in the processing"},
        {"spots",               &spots,                 "List of spots (1, 2, 3, 4, 5, 6) to process; this is only supported by the atl03x endpoint"},
        {"beams",               &beams,                 "List of beams (gt1l, gt1r, gt2l, gt2r, gt3l, gt3r; defaults to all) to process"},
        {"track",               &track,                 "Reference pair track number (1, 2, 3, or 0 to include for all three; defaults to 0) to process; note that when provided, this is combined with the beam selection as a union of the two"},
        {"cnt",                 &minPhotonCount,        "Minimum photon count in the variable length segment for the segment to be processed"},
        {"ats",                 &minAlongTrackSpread,   "Minimum along track spread of the photons (in meters) in the variable length segment for the segment to be processed"},
        {"len",                 &extentLength,          "Size (in meters) of the variable length segment"},
        {"res",                 &extentStep,            "Step (in meters) of the variable length segments; could also be thought of as the spacing of the segments or the resolution of the segments"},
        {"podppd",              &podppdMask,            "Pointing/geolocation degradation mask; each bit in the mask represents a pointing/geolocation solution quality assessment to be included; the bits are 0: nominal, 1: pod_degrade, 2: ppd_degrade, 3: podppd_degrade, 4: cal_nominal, 5: cal_pod_degrade, 6: cal_ppd_degrade, 7: cal_podppd_degrade"},
        {"fit",                 &fit,                   "Configuration structure for the 'Surface Fitting' algorithm; when provided the servers will fit a surface to the source photon cloud and return an elevation dataset similar to ATL06"},
        {"yapc",                &yapc,                  "Configuration structure for the 'Yet Another Photon Classifier' algorithm; when provided the servers will calculate a density score for each photon and include that score in the response data"},
        {"phoreal",             &phoreal,               "Configuration structure for the 'PhoREAL' algorithm; when provided the servers will calculate canopy metrics on the source photon cloud and return those metrics as a dataset similar to ATL08"},
        {"als",                 &blanket,               "Configuration structure for the 'Surface Blanket' algorithm; when provided the servers will calculate a canopy top and ground using the source photon cloud and return those values in the response"},
        {"atl13",               &atl13,                 "Configuration structure for the 'atl13x' dataset construction"},
        {"atl24",               &atl24,                 "Configuration structure for the 'atl24x' dataset construction"},
        {"maxi",                &maxIterations,         "Maximum number of iterations for the Surface Fitting algorithm to run when fitting a line to the photons in a segment; deprecated, use the fit.maxi field"},
        {"H_min_win",           &minWindow,             "Minimum vertical window used by the Surface Fitting algorithm when fitting a line to the photons in a segment; deprecated, use fit.H_min_win"},
        {"sigma_r_max",         &maxRobustDispersion,   "Maximum robust dispersion used by the Surface Fitting algorithm when fitting a line to the photons in a segment; deprecated, use fit.sigma_r_max"},
        {"atl03_bckgrd_fields", &atl03BckgrdFields,     "Ancillary fields in the 'bckgrd_atlas' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x"},
        {"atl03_geo_fields",    &atl03GeoFields,        "Ancillary fields in the 'geolocation' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x"},
        {"atl03_corr_fields",   &atl03CorrFields,       "Ancillary fields in the 'geophys_corr' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x"},
        {"atl03_ph_fields",     &atl03PhFields,         "Ancillary fields in the 'heights' group of the ATL03 granule to include in the response; supported by atl03x, atl06x, and atl08x"},
        {"atl06_fields",        &atl06Fields,           "Ancillary fields in the 'land_ice_segments' group of the ATL06 granule to include in the response; supported by atl06x"},
        {"atl08_fields",        &atl08Fields,           "Ancillary fields in the 'land_segments' group of the ATL08 granule to include in the response; supported by atl08x"},
        {"atl09_fields",        &atl09Fields,           "Ancillary fields in the ATL09 granule to include in the response (e.g. low_rate/cal_c); supported by all x-series endpoints"},
        {"atl13_fields",        &atl13Fields,           "Ancillary fields in the ATL13 granule to include in the response (e.g. ice_flag); supported by atl13x"},
        {"granule",             &granuleFields,         "Versioning, ground track, and date information pulled from the granule processed; output only"} })
{
    // add additional fields to dictionary
    for(const FieldMap<Field>::init_entry_t elem: init_list)
    {
        const entry_t entry = {elem.field, elem.description, false};
        fields.add(elem.name, entry);
    }

    // add additional functions
    LuaEngine::setAttrFunc(L, "stage", luaStage);
}

/*----------------------------------------------------------------------------
 * luaStage -
 *----------------------------------------------------------------------------*/
int Icesat2Fields::luaStage (lua_State* L)
{
    try
    {
        const Icesat2Fields* lua_obj = dynamic_cast<Icesat2Fields*>(getLuaSelf(L, 1));
        const long stage = getLuaInteger(L, 2);
        if(stage < 0 || stage >= NUM_STAGES)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid stage %ld", stage);
        }

        lua_pushboolean(L, lua_obj->stages[stage]);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting stage: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
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
 * convertToJson - signal_conf_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Fields::signal_conf_t& v)
{
    switch(v)
    {
        case Icesat2Fields::CNF_POSSIBLE_TEP:   return "\"atl03_tep\"";
        case Icesat2Fields::CNF_NOT_CONSIDERED: return "\"atl03_not_considered\"";
        case Icesat2Fields::CNF_BACKGROUND:     return "\"atl03_background\"";
        case Icesat2Fields::CNF_WITHIN_10M:     return "\"atl03_within_10m\"";
        case Icesat2Fields::CNF_SURFACE_LOW:    return "\"atl03_low\"";
        case Icesat2Fields::CNF_SURFACE_MEDIUM: return "\"atl03_medium\"";
        case Icesat2Fields::CNF_SURFACE_HIGH:   return "\"atl03_high\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid signal confidence: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - signal_conf_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::signal_conf_t& v)
{
    switch(v)
    {
        case Icesat2Fields::CNF_POSSIBLE_TEP:   lua_pushstring(L, "atl03_tep");             break;
        case Icesat2Fields::CNF_NOT_CONSIDERED: lua_pushstring(L, "atl03_not_considered");  break;
        case Icesat2Fields::CNF_BACKGROUND:     lua_pushstring(L, "atl03_background");      break;
        case Icesat2Fields::CNF_WITHIN_10M:     lua_pushstring(L, "atl03_within_10m");      break;
        case Icesat2Fields::CNF_SURFACE_LOW:    lua_pushstring(L, "atl03_low");             break;
        case Icesat2Fields::CNF_SURFACE_MEDIUM: lua_pushstring(L, "atl03_medium");          break;
        case Icesat2Fields::CNF_SURFACE_HIGH:   lua_pushstring(L, "atl03_high");            break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid signal confidence: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - signal_conf_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Fields::signal_conf_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Fields::signal_conf_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "atl03_tep"))             v = Icesat2Fields::CNF_POSSIBLE_TEP;
        else if(StringLib::match(str, "atl03_not_considered"))  v = Icesat2Fields::CNF_NOT_CONSIDERED;
        else if(StringLib::match(str, "atl03_background"))      v = Icesat2Fields::CNF_BACKGROUND;
        else if(StringLib::match(str, "atl03_within_10m"))      v = Icesat2Fields::CNF_WITHIN_10M;
        else if(StringLib::match(str, "atl03_low"))             v = Icesat2Fields::CNF_SURFACE_LOW;
        else if(StringLib::match(str, "atl03_medium"))          v = Icesat2Fields::CNF_SURFACE_MEDIUM;
        else if(StringLib::match(str, "atl03_high"))            v = Icesat2Fields::CNF_SURFACE_HIGH;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "signal confidence is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "signal confidence is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - signal_conf_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Icesat2Fields::signal_conf_t& v)
{
    return static_cast<int>(v) + 2;
}

/*----------------------------------------------------------------------------
 * convertFromIndex - signal_conf_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Fields::signal_conf_t& v)
{
    v = static_cast<Icesat2Fields::signal_conf_t>(index - 2);
}

/*----------------------------------------------------------------------------
 * convertToJson - quality_ph_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Fields::quality_ph_t& v)
{
    switch(v)
    {
        case Icesat2Fields::QUALITY_NOMINAL:                    return "\"atl03_quality_nominal\"";
        case Icesat2Fields::QUALITY_POSSIBLE_AFTERPULSE:        return "\"atl03_quality_afterpulse\"";
        case Icesat2Fields::QUALITY_POSSIBLE_IMPULSE_RESPONSE:  return "\"atl03_quality_impulse_response\"";
        case Icesat2Fields::QUALITY_POSSIBLE_TEP:               return "\"atl03_quality_tep\"";
        case Icesat2Fields::QUALITY_NOISE_BURST:                return "\"atl03_quality_noise_burst\"";
        case Icesat2Fields::QUALITY_NOISE_STREAK:               return "\"atl03_quality_noise_streak\"";
        case Icesat2Fields::QUALITY_TX_PART_SAT:                return "\"atl03_quality_tx_part_sat\"";
        case Icesat2Fields::QUALITY_TX_PART_SAT_AFTERPULSE:     return "\"atl03_quality_tx_part_sat_afterpulse\"";
        case Icesat2Fields::QUALITY_TX_PART_SAT_IR_EFFECT:      return "\"atl03_quality_tx_part_sat_ir_effect\"";
        case Icesat2Fields::QUALITY_TX_PART_SAT_BURST:          return "\"atl03_quality_tx_part_sat_burst\"";
        case Icesat2Fields::QUALITY_TX_PART_SAT_STREAK:         return "\"atl03_quality_tx_part_sat_streak\"";
        case Icesat2Fields::QUALITY_TX_FULL_SAT:                return "\"atl03_quality_tx_full_sat\"";
        case Icesat2Fields::QUALITY_TX_FULL_SAT_AFTERPULSE:     return "\"atl03_quality_tx_full_sat_afterpulse\"";
        case Icesat2Fields::QUALITY_TX_FULL_SAT_IR_EFFECT:      return "\"atl03_quality_tx_full_sat_ir_effect\"";
        case Icesat2Fields::QUALITY_TX_FULL_SAT_BURST:          return "\"atl03_quality_tx_full_sat_burst\"";
        case Icesat2Fields::QUALITY_TX_FULL_SAT_STREAK:         return "\"atl03_quality_tx_full_sat_streak\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid photon quality: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - quality_ph_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::quality_ph_t& v)
{
    switch(v)
    {
        case Icesat2Fields::QUALITY_NOMINAL:                    lua_pushstring(L, "atl03_quality_nominal");                 break;
        case Icesat2Fields::QUALITY_POSSIBLE_AFTERPULSE:        lua_pushstring(L, "atl03_quality_afterpulse");              break;
        case Icesat2Fields::QUALITY_POSSIBLE_IMPULSE_RESPONSE:  lua_pushstring(L, "atl03_quality_impulse_response");        break;
        case Icesat2Fields::QUALITY_POSSIBLE_TEP:               lua_pushstring(L, "atl03_quality_tep");                     break;
        case Icesat2Fields::QUALITY_NOISE_BURST:                lua_pushstring(L, "atl03_quality_noise_burst");             break;
        case Icesat2Fields::QUALITY_NOISE_STREAK:               lua_pushstring(L, "atl03_quality_noise_streak");            break;
        case Icesat2Fields::QUALITY_TX_PART_SAT:                lua_pushstring(L, "atl03_quality_tx_part_sat");             break;
        case Icesat2Fields::QUALITY_TX_PART_SAT_AFTERPULSE:     lua_pushstring(L, "atl03_quality_tx_part_sat_afterpulse");  break;
        case Icesat2Fields::QUALITY_TX_PART_SAT_IR_EFFECT:      lua_pushstring(L, "atl03_quality_tx_part_sat_ir_effect");   break;
        case Icesat2Fields::QUALITY_TX_PART_SAT_BURST:          lua_pushstring(L, "atl03_quality_tx_part_sat_burst");       break;
        case Icesat2Fields::QUALITY_TX_PART_SAT_STREAK:         lua_pushstring(L, "atl03_quality_tx_part_sat_streak");      break;
        case Icesat2Fields::QUALITY_TX_FULL_SAT:                lua_pushstring(L, "atl03_quality_tx_full_sat");             break;
        case Icesat2Fields::QUALITY_TX_FULL_SAT_AFTERPULSE:     lua_pushstring(L, "atl03_quality_tx_full_sat_afterpulse");  break;
        case Icesat2Fields::QUALITY_TX_FULL_SAT_IR_EFFECT:      lua_pushstring(L, "atl03_quality_tx_full_sat_ir_effect");   break;
        case Icesat2Fields::QUALITY_TX_FULL_SAT_BURST:          lua_pushstring(L, "atl03_quality_tx_full_sat_burst");       break;
        case Icesat2Fields::QUALITY_TX_FULL_SAT_STREAK:         lua_pushstring(L, "atl03_quality_tx_full_sat_streak");      break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid photon quality: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - quality_ph_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Fields::quality_ph_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Fields::quality_ph_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "atl03_quality_nominal"))                 v = Icesat2Fields::QUALITY_NOMINAL;
        else if(StringLib::match(str, "atl03_quality_afterpulse"))              v = Icesat2Fields::QUALITY_POSSIBLE_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_impulse_response"))        v = Icesat2Fields::QUALITY_POSSIBLE_IMPULSE_RESPONSE;
        else if(StringLib::match(str, "atl03_quality_tep"))                     v = Icesat2Fields::QUALITY_POSSIBLE_TEP;
        else if(StringLib::match(str, "atl03_quality_noise_burst"))             v = Icesat2Fields::QUALITY_NOISE_BURST;
        else if(StringLib::match(str, "atl03_quality_noise_streak"))            v = Icesat2Fields::QUALITY_NOISE_STREAK;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat"))             v = Icesat2Fields::QUALITY_TX_PART_SAT;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_afterpulse"))  v = Icesat2Fields::QUALITY_TX_PART_SAT_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_ir_effect"))   v = Icesat2Fields::QUALITY_TX_PART_SAT_IR_EFFECT;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_burst"))       v = Icesat2Fields::QUALITY_TX_PART_SAT_BURST;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_streak"))      v = Icesat2Fields::QUALITY_TX_PART_SAT_STREAK;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat"))             v = Icesat2Fields::QUALITY_TX_FULL_SAT;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_afterpulse"))  v = Icesat2Fields::QUALITY_TX_FULL_SAT_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_ir_effect"))   v = Icesat2Fields::QUALITY_TX_FULL_SAT_IR_EFFECT;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_burst"))       v = Icesat2Fields::QUALITY_TX_FULL_SAT_BURST;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_streak"))      v = Icesat2Fields::QUALITY_TX_FULL_SAT_STREAK;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "photon quality is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "photon quality is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - quality_ph_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Icesat2Fields::quality_ph_t& v)
{
    return static_cast<int>(v);
}

/*----------------------------------------------------------------------------
 * convertFromIndex - quality_ph_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Fields::quality_ph_t& v)
{
    v = static_cast<Icesat2Fields::quality_ph_t>(index);
}

/*----------------------------------------------------------------------------
 * convertToJson - atl08_class_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Fields::atl08_class_t& v)
{
    switch(v)
    {
        case Icesat2Fields::ATL08_NOISE:            return "\"atl08_noise\"";
        case Icesat2Fields::ATL08_GROUND:           return "\"atl08_ground\"";
        case Icesat2Fields::ATL08_CANOPY:           return "\"atl08_canopy\"";
        case Icesat2Fields::ATL08_TOP_OF_CANOPY:    return "\"atl08_top_of_canopy\"";
        case Icesat2Fields::ATL08_UNCLASSIFIED:     return "\"atl08_unclassified\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl08 classification: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - atl08_class_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::atl08_class_t& v)
{
    switch(v)
    {
        case Icesat2Fields::ATL08_NOISE:            lua_pushstring(L, "atl08_noise");           break;
        case Icesat2Fields::ATL08_GROUND:           lua_pushstring(L, "atl08_ground");          break;
        case Icesat2Fields::ATL08_CANOPY:           lua_pushstring(L, "atl08_canopy");          break;
        case Icesat2Fields::ATL08_TOP_OF_CANOPY:    lua_pushstring(L, "atl08_top_of_canopy");   break;
        case Icesat2Fields::ATL08_UNCLASSIFIED:     lua_pushstring(L, "atl08_unclassified");    break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl08 classification: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - atl08_class_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Fields::atl08_class_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Fields::atl08_class_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "atl08_noise"))           v = Icesat2Fields::ATL08_NOISE;
        else if(StringLib::match(str, "atl08_ground"))          v = Icesat2Fields::ATL08_GROUND;
        else if(StringLib::match(str, "atl08_canopy"))          v = Icesat2Fields::ATL08_CANOPY;
        else if(StringLib::match(str, "atl08_top_of_canopy"))   v = Icesat2Fields::ATL08_TOP_OF_CANOPY;
        else if(StringLib::match(str, "atl08_unclassified"))    v = Icesat2Fields::ATL08_UNCLASSIFIED;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "atl08 classification is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "atl08 classification is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - atl08_class_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Icesat2Fields::atl08_class_t& v)
{
    return static_cast<int>(v);
}

/*----------------------------------------------------------------------------
 * convertFromIndex - atl08_class_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Fields::atl08_class_t& v)
{
    v = static_cast<Icesat2Fields::atl08_class_t>(index);
}

/*----------------------------------------------------------------------------
 * convertToJson - gt_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Fields::gt_t& v)
{
    switch(v)
    {
        case Icesat2Fields::GT1L:   return "\"gt1l\"";
        case Icesat2Fields::GT1R:   return "\"gt1r\"";
        case Icesat2Fields::GT2L:   return "\"gt2l\"";
        case Icesat2Fields::GT2R:   return "\"gt2r\"";
        case Icesat2Fields::GT3L:   return "\"gt3l\"";
        case Icesat2Fields::GT3R:   return "\"gt3r\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid ground track: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - gt_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::gt_t& v)
{
    switch(v)
    {
        case Icesat2Fields::GT1L:   lua_pushstring(L, "gt1l");  break;
        case Icesat2Fields::GT1R:   lua_pushstring(L, "gt1r");  break;
        case Icesat2Fields::GT2L:   lua_pushstring(L, "gt2l");  break;
        case Icesat2Fields::GT2R:   lua_pushstring(L, "gt2r");  break;
        case Icesat2Fields::GT3L:   lua_pushstring(L, "gt3l");  break;
        case Icesat2Fields::GT3R:   lua_pushstring(L, "gt3r");  break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid ground track: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - gt_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Fields::gt_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Fields::gt_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "gt1l"))  v = Icesat2Fields::GT1L;
        else if(StringLib::match(str, "gt1r"))  v = Icesat2Fields::GT1R;
        else if(StringLib::match(str, "gt2l"))  v = Icesat2Fields::GT2L;
        else if(StringLib::match(str, "gt2r"))  v = Icesat2Fields::GT2R;
        else if(StringLib::match(str, "gt3l"))  v = Icesat2Fields::GT3L;
        else if(StringLib::match(str, "gt3r"))  v = Icesat2Fields::GT3R;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "ground track is an invalid value: %s", str);
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "ground track is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - gt_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Icesat2Fields::gt_t& v)
{
    return (static_cast<int>(v) / 10) - 1;
}

/*----------------------------------------------------------------------------
 * convertFromIndex - gt_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Fields::gt_t& v)
{
    v = static_cast<Icesat2Fields::gt_t>((index + 1) * 10);
}

/*----------------------------------------------------------------------------
 * convertToJson - spot_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Fields::spot_t& v)
{
    switch(v)
    {
        case Icesat2Fields::SPOT_1:   return "1";
        case Icesat2Fields::SPOT_2:   return "2";
        case Icesat2Fields::SPOT_3:   return "3";
        case Icesat2Fields::SPOT_4:   return "4";
        case Icesat2Fields::SPOT_5:   return "5";
        case Icesat2Fields::SPOT_6:   return "6";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid spot: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - spot_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::spot_t& v)
{
    switch(v)
    {
        case Icesat2Fields::SPOT_1:   lua_pushinteger(L, 1);  break;
        case Icesat2Fields::SPOT_2:   lua_pushinteger(L, 2);  break;
        case Icesat2Fields::SPOT_3:   lua_pushinteger(L, 3);  break;
        case Icesat2Fields::SPOT_4:   lua_pushinteger(L, 4);  break;
        case Icesat2Fields::SPOT_5:   lua_pushinteger(L, 5);  break;
        case Icesat2Fields::SPOT_6:   lua_pushinteger(L, 6);  break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid spot: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - spot_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Fields::spot_t& v)
{
    if(lua_isinteger(L, index))
    {
        const int i = LuaObject::getLuaInteger(L, index);
        if(i >= 1 && i <= Icesat2Fields::NUM_SPOTS)
        {
            v = static_cast<Icesat2Fields::spot_t>(i);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid spot: %d", i);
        }
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "spot is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - spot_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const Icesat2Fields::spot_t& v)
{
    return static_cast<int>(v) - 1;
}

/*----------------------------------------------------------------------------
 * convertFromIndex - spot_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Fields::spot_t& v)
{
    v = static_cast<Icesat2Fields::spot_t>(index + 1);
}

/*----------------------------------------------------------------------------
 * convertToJson - surface_type_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Fields::surface_type_t& v)
{
    switch(v)
    {
        case Icesat2Fields::SRT_DYNAMIC:        return "\"dynamic\"";
        case Icesat2Fields::SRT_LAND:           return "\"land\"";
        case Icesat2Fields::SRT_OCEAN:          return "\"ocean\"";
        case Icesat2Fields::SRT_SEA_ICE:        return "\"sea_ice\"";
        case Icesat2Fields::SRT_LAND_ICE:       return "\"land_ice\"";
        case Icesat2Fields::SRT_INLAND_WATER:   return "\"inland_water\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid surface type: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - surface_type_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::surface_type_t& v)
{
    switch(v)
    {
        case Icesat2Fields::SRT_DYNAMIC:        lua_pushstring(L, "dynamic");       break;
        case Icesat2Fields::SRT_LAND:           lua_pushstring(L, "land");          break;
        case Icesat2Fields::SRT_OCEAN:          lua_pushstring(L, "ocean");         break;
        case Icesat2Fields::SRT_SEA_ICE:        lua_pushstring(L, "sea_ice");       break;
        case Icesat2Fields::SRT_LAND_ICE:       lua_pushstring(L, "land_ice");      break;
        case Icesat2Fields::SRT_INLAND_WATER:   lua_pushstring(L, "inland_water");  break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid surface type: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - surface_type_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Fields::surface_type_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Fields::surface_type_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "dynamic"))       v = Icesat2Fields::SRT_DYNAMIC;
        else if(StringLib::match(str, "land"))          v = Icesat2Fields::SRT_LAND;
        else if(StringLib::match(str, "ocean"))         v = Icesat2Fields::SRT_OCEAN;
        else if(StringLib::match(str, "sea_ice"))       v = Icesat2Fields::SRT_SEA_ICE;
        else if(StringLib::match(str, "land_ice"))      v = Icesat2Fields::SRT_LAND_ICE;
        else if(StringLib::match(str, "inland_water"))  v = Icesat2Fields::SRT_INLAND_WATER;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "surface type is an invalid value: %s", str);
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "surface type is an invalid type: %d", lua_type(L, index));
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
