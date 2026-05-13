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
#include "Icesat2Parameters.h"
#include "FieldMap.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Icesat2Parameters::LUA_META_NAME = "Icesat2Parameters";

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
 * fromLua
 *----------------------------------------------------------------------------*/
void Icesat2Parameters::fromLua (lua_State* L, int index)
{
    RequestParameters::fromLua(L, index);

    // parse resource name
    if(!resource.value.empty())
    {
        granuleFields.parseResource(resource.value.c_str());
    }

    // handle atl09 sampline options
    if(atl09Fields.length() > 0)
    {
        stages[STAGE_ATL09] = true;
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
Icesat2Parameters::Icesat2Parameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name):
    RequestParameters (L, key_space, asset_name, _resource, lua_meta_name)
{
    addParameter("spots",               &spots,                 "List of spots (1, 2, 3, 4, 5, 6) to process; this is only supported by the atl03x endpoint");
    addParameter("beams",               &beams,                 "List of beams (gt1l, gt1r, gt2l, gt2r, gt3l, gt3r; defaults to all) to process");
    addParameter("track",               &track,                 "Reference pair track number (1, 2, 3, or 0 to include for all three; defaults to 0) to process; note that when provided, this is combined with the beam selection as a union of the two");
    addParameter("atl09_fields",        &atl09Fields,           "Ancillary fields in the ATL09 granule to include in the response (e.g. low_rate/cal_c); supported by all x-series endpoints");
    addParameter("granule",             &granuleFields,         "Versioning, ground track, and date information pulled from the granule processed; output only");

    // add additional functions
    LuaEngine::setAttrFunc(L, "stage", luaStage);
}

/*----------------------------------------------------------------------------
 * luaStage -
 *----------------------------------------------------------------------------*/
int Icesat2Parameters::luaStage (lua_State* L)
{
    try
    {
        const Icesat2Parameters* lua_obj = dynamic_cast<Icesat2Parameters*>(getLuaSelf(L, 1));
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
 * convertToJson - signal_conf_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Parameters::signal_conf_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::CNF_POSSIBLE_TEP:   return "\"atl03_tep\"";
        case Icesat2Parameters::CNF_NOT_CONSIDERED: return "\"atl03_not_considered\"";
        case Icesat2Parameters::CNF_BACKGROUND:     return "\"atl03_background\"";
        case Icesat2Parameters::CNF_WITHIN_10M:     return "\"atl03_within_10m\"";
        case Icesat2Parameters::CNF_SURFACE_LOW:    return "\"atl03_low\"";
        case Icesat2Parameters::CNF_SURFACE_MEDIUM: return "\"atl03_medium\"";
        case Icesat2Parameters::CNF_SURFACE_HIGH:   return "\"atl03_high\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid signal confidence: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - signal_conf_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Parameters::signal_conf_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::CNF_POSSIBLE_TEP:   lua_pushstring(L, "atl03_tep");             break;
        case Icesat2Parameters::CNF_NOT_CONSIDERED: lua_pushstring(L, "atl03_not_considered");  break;
        case Icesat2Parameters::CNF_BACKGROUND:     lua_pushstring(L, "atl03_background");      break;
        case Icesat2Parameters::CNF_WITHIN_10M:     lua_pushstring(L, "atl03_within_10m");      break;
        case Icesat2Parameters::CNF_SURFACE_LOW:    lua_pushstring(L, "atl03_low");             break;
        case Icesat2Parameters::CNF_SURFACE_MEDIUM: lua_pushstring(L, "atl03_medium");          break;
        case Icesat2Parameters::CNF_SURFACE_HIGH:   lua_pushstring(L, "atl03_high");            break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid signal confidence: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - signal_conf_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Parameters::signal_conf_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Parameters::signal_conf_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "atl03_tep"))             v = Icesat2Parameters::CNF_POSSIBLE_TEP;
        else if(StringLib::match(str, "atl03_not_considered"))  v = Icesat2Parameters::CNF_NOT_CONSIDERED;
        else if(StringLib::match(str, "atl03_background"))      v = Icesat2Parameters::CNF_BACKGROUND;
        else if(StringLib::match(str, "atl03_within_10m"))      v = Icesat2Parameters::CNF_WITHIN_10M;
        else if(StringLib::match(str, "atl03_low"))             v = Icesat2Parameters::CNF_SURFACE_LOW;
        else if(StringLib::match(str, "atl03_medium"))          v = Icesat2Parameters::CNF_SURFACE_MEDIUM;
        else if(StringLib::match(str, "atl03_high"))            v = Icesat2Parameters::CNF_SURFACE_HIGH;
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
int convertToIndex(const Icesat2Parameters::signal_conf_t& v)
{
    return static_cast<int>(v) + 2;
}

/*----------------------------------------------------------------------------
 * convertFromIndex - signal_conf_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Parameters::signal_conf_t& v)
{
    v = static_cast<Icesat2Parameters::signal_conf_t>(index - 2);
}

/*----------------------------------------------------------------------------
 * convertToJson - quality_ph_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Parameters::quality_ph_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::QUALITY_NOMINAL:                    return "\"atl03_quality_nominal\"";
        case Icesat2Parameters::QUALITY_POSSIBLE_AFTERPULSE:        return "\"atl03_quality_afterpulse\"";
        case Icesat2Parameters::QUALITY_POSSIBLE_IMPULSE_RESPONSE:  return "\"atl03_quality_impulse_response\"";
        case Icesat2Parameters::QUALITY_POSSIBLE_TEP:               return "\"atl03_quality_tep\"";
        case Icesat2Parameters::QUALITY_NOISE_BURST:                return "\"atl03_quality_noise_burst\"";
        case Icesat2Parameters::QUALITY_NOISE_STREAK:               return "\"atl03_quality_noise_streak\"";
        case Icesat2Parameters::QUALITY_TX_PART_SAT:                return "\"atl03_quality_tx_part_sat\"";
        case Icesat2Parameters::QUALITY_TX_PART_SAT_AFTERPULSE:     return "\"atl03_quality_tx_part_sat_afterpulse\"";
        case Icesat2Parameters::QUALITY_TX_PART_SAT_IR_EFFECT:      return "\"atl03_quality_tx_part_sat_ir_effect\"";
        case Icesat2Parameters::QUALITY_TX_PART_SAT_BURST:          return "\"atl03_quality_tx_part_sat_burst\"";
        case Icesat2Parameters::QUALITY_TX_PART_SAT_STREAK:         return "\"atl03_quality_tx_part_sat_streak\"";
        case Icesat2Parameters::QUALITY_TX_FULL_SAT:                return "\"atl03_quality_tx_full_sat\"";
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_AFTERPULSE:     return "\"atl03_quality_tx_full_sat_afterpulse\"";
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_IR_EFFECT:      return "\"atl03_quality_tx_full_sat_ir_effect\"";
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_BURST:          return "\"atl03_quality_tx_full_sat_burst\"";
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_STREAK:         return "\"atl03_quality_tx_full_sat_streak\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid photon quality: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - quality_ph_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Parameters::quality_ph_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::QUALITY_NOMINAL:                    lua_pushstring(L, "atl03_quality_nominal");                 break;
        case Icesat2Parameters::QUALITY_POSSIBLE_AFTERPULSE:        lua_pushstring(L, "atl03_quality_afterpulse");              break;
        case Icesat2Parameters::QUALITY_POSSIBLE_IMPULSE_RESPONSE:  lua_pushstring(L, "atl03_quality_impulse_response");        break;
        case Icesat2Parameters::QUALITY_POSSIBLE_TEP:               lua_pushstring(L, "atl03_quality_tep");                     break;
        case Icesat2Parameters::QUALITY_NOISE_BURST:                lua_pushstring(L, "atl03_quality_noise_burst");             break;
        case Icesat2Parameters::QUALITY_NOISE_STREAK:               lua_pushstring(L, "atl03_quality_noise_streak");            break;
        case Icesat2Parameters::QUALITY_TX_PART_SAT:                lua_pushstring(L, "atl03_quality_tx_part_sat");             break;
        case Icesat2Parameters::QUALITY_TX_PART_SAT_AFTERPULSE:     lua_pushstring(L, "atl03_quality_tx_part_sat_afterpulse");  break;
        case Icesat2Parameters::QUALITY_TX_PART_SAT_IR_EFFECT:      lua_pushstring(L, "atl03_quality_tx_part_sat_ir_effect");   break;
        case Icesat2Parameters::QUALITY_TX_PART_SAT_BURST:          lua_pushstring(L, "atl03_quality_tx_part_sat_burst");       break;
        case Icesat2Parameters::QUALITY_TX_PART_SAT_STREAK:         lua_pushstring(L, "atl03_quality_tx_part_sat_streak");      break;
        case Icesat2Parameters::QUALITY_TX_FULL_SAT:                lua_pushstring(L, "atl03_quality_tx_full_sat");             break;
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_AFTERPULSE:     lua_pushstring(L, "atl03_quality_tx_full_sat_afterpulse");  break;
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_IR_EFFECT:      lua_pushstring(L, "atl03_quality_tx_full_sat_ir_effect");   break;
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_BURST:          lua_pushstring(L, "atl03_quality_tx_full_sat_burst");       break;
        case Icesat2Parameters::QUALITY_TX_FULL_SAT_STREAK:         lua_pushstring(L, "atl03_quality_tx_full_sat_streak");      break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid photon quality: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - quality_ph_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Parameters::quality_ph_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Parameters::quality_ph_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "atl03_quality_nominal"))                 v = Icesat2Parameters::QUALITY_NOMINAL;
        else if(StringLib::match(str, "atl03_quality_afterpulse"))              v = Icesat2Parameters::QUALITY_POSSIBLE_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_impulse_response"))        v = Icesat2Parameters::QUALITY_POSSIBLE_IMPULSE_RESPONSE;
        else if(StringLib::match(str, "atl03_quality_tep"))                     v = Icesat2Parameters::QUALITY_POSSIBLE_TEP;
        else if(StringLib::match(str, "atl03_quality_noise_burst"))             v = Icesat2Parameters::QUALITY_NOISE_BURST;
        else if(StringLib::match(str, "atl03_quality_noise_streak"))            v = Icesat2Parameters::QUALITY_NOISE_STREAK;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat"))             v = Icesat2Parameters::QUALITY_TX_PART_SAT;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_afterpulse"))  v = Icesat2Parameters::QUALITY_TX_PART_SAT_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_ir_effect"))   v = Icesat2Parameters::QUALITY_TX_PART_SAT_IR_EFFECT;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_burst"))       v = Icesat2Parameters::QUALITY_TX_PART_SAT_BURST;
        else if(StringLib::match(str, "atl03_quality_tx_part_sat_streak"))      v = Icesat2Parameters::QUALITY_TX_PART_SAT_STREAK;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat"))             v = Icesat2Parameters::QUALITY_TX_FULL_SAT;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_afterpulse"))  v = Icesat2Parameters::QUALITY_TX_FULL_SAT_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_ir_effect"))   v = Icesat2Parameters::QUALITY_TX_FULL_SAT_IR_EFFECT;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_burst"))       v = Icesat2Parameters::QUALITY_TX_FULL_SAT_BURST;
        else if(StringLib::match(str, "atl03_quality_tx_full_sat_streak"))      v = Icesat2Parameters::QUALITY_TX_FULL_SAT_STREAK;
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
int convertToIndex(const Icesat2Parameters::quality_ph_t& v)
{
    return static_cast<int>(v);
}

/*----------------------------------------------------------------------------
 * convertFromIndex - quality_ph_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Parameters::quality_ph_t& v)
{
    v = static_cast<Icesat2Parameters::quality_ph_t>(index);
}

/*----------------------------------------------------------------------------
 * convertToJson - atl08_class_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Parameters::atl08_class_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::ATL08_NOISE:            return "\"atl08_noise\"";
        case Icesat2Parameters::ATL08_GROUND:           return "\"atl08_ground\"";
        case Icesat2Parameters::ATL08_CANOPY:           return "\"atl08_canopy\"";
        case Icesat2Parameters::ATL08_TOP_OF_CANOPY:    return "\"atl08_top_of_canopy\"";
        case Icesat2Parameters::ATL08_UNCLASSIFIED:     return "\"atl08_unclassified\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl08 classification: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - atl08_class_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Parameters::atl08_class_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::ATL08_NOISE:            lua_pushstring(L, "atl08_noise");           break;
        case Icesat2Parameters::ATL08_GROUND:           lua_pushstring(L, "atl08_ground");          break;
        case Icesat2Parameters::ATL08_CANOPY:           lua_pushstring(L, "atl08_canopy");          break;
        case Icesat2Parameters::ATL08_TOP_OF_CANOPY:    lua_pushstring(L, "atl08_top_of_canopy");   break;
        case Icesat2Parameters::ATL08_UNCLASSIFIED:     lua_pushstring(L, "atl08_unclassified");    break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid atl08 classification: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - atl08_class_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Parameters::atl08_class_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Parameters::atl08_class_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "atl08_noise"))           v = Icesat2Parameters::ATL08_NOISE;
        else if(StringLib::match(str, "atl08_ground"))          v = Icesat2Parameters::ATL08_GROUND;
        else if(StringLib::match(str, "atl08_canopy"))          v = Icesat2Parameters::ATL08_CANOPY;
        else if(StringLib::match(str, "atl08_top_of_canopy"))   v = Icesat2Parameters::ATL08_TOP_OF_CANOPY;
        else if(StringLib::match(str, "atl08_unclassified"))    v = Icesat2Parameters::ATL08_UNCLASSIFIED;
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
int convertToIndex(const Icesat2Parameters::atl08_class_t& v)
{
    return static_cast<int>(v);
}

/*----------------------------------------------------------------------------
 * convertFromIndex - atl08_class_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Parameters::atl08_class_t& v)
{
    v = static_cast<Icesat2Parameters::atl08_class_t>(index);
}

/*----------------------------------------------------------------------------
 * convertToJson - gt_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Parameters::gt_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::GT1L:   return "\"gt1l\"";
        case Icesat2Parameters::GT1R:   return "\"gt1r\"";
        case Icesat2Parameters::GT2L:   return "\"gt2l\"";
        case Icesat2Parameters::GT2R:   return "\"gt2r\"";
        case Icesat2Parameters::GT3L:   return "\"gt3l\"";
        case Icesat2Parameters::GT3R:   return "\"gt3r\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid ground track: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - gt_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Parameters::gt_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::GT1L:   lua_pushstring(L, "gt1l");  break;
        case Icesat2Parameters::GT1R:   lua_pushstring(L, "gt1r");  break;
        case Icesat2Parameters::GT2L:   lua_pushstring(L, "gt2l");  break;
        case Icesat2Parameters::GT2R:   lua_pushstring(L, "gt2r");  break;
        case Icesat2Parameters::GT3L:   lua_pushstring(L, "gt3l");  break;
        case Icesat2Parameters::GT3R:   lua_pushstring(L, "gt3r");  break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid ground track: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - gt_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Parameters::gt_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Parameters::gt_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "gt1l"))  v = Icesat2Parameters::GT1L;
        else if(StringLib::match(str, "gt1r"))  v = Icesat2Parameters::GT1R;
        else if(StringLib::match(str, "gt2l"))  v = Icesat2Parameters::GT2L;
        else if(StringLib::match(str, "gt2r"))  v = Icesat2Parameters::GT2R;
        else if(StringLib::match(str, "gt3l"))  v = Icesat2Parameters::GT3L;
        else if(StringLib::match(str, "gt3r"))  v = Icesat2Parameters::GT3R;
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
int convertToIndex(const Icesat2Parameters::gt_t& v)
{
    return (static_cast<int>(v) / 10) - 1;
}

/*----------------------------------------------------------------------------
 * convertFromIndex - gt_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Parameters::gt_t& v)
{
    v = static_cast<Icesat2Parameters::gt_t>((index + 1) * 10);
}

/*----------------------------------------------------------------------------
 * convertToJson - spot_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Parameters::spot_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::SPOT_1:   return "1";
        case Icesat2Parameters::SPOT_2:   return "2";
        case Icesat2Parameters::SPOT_3:   return "3";
        case Icesat2Parameters::SPOT_4:   return "4";
        case Icesat2Parameters::SPOT_5:   return "5";
        case Icesat2Parameters::SPOT_6:   return "6";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid spot: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - spot_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Parameters::spot_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::SPOT_1:   lua_pushinteger(L, 1);  break;
        case Icesat2Parameters::SPOT_2:   lua_pushinteger(L, 2);  break;
        case Icesat2Parameters::SPOT_3:   lua_pushinteger(L, 3);  break;
        case Icesat2Parameters::SPOT_4:   lua_pushinteger(L, 4);  break;
        case Icesat2Parameters::SPOT_5:   lua_pushinteger(L, 5);  break;
        case Icesat2Parameters::SPOT_6:   lua_pushinteger(L, 6);  break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid spot: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - spot_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Parameters::spot_t& v)
{
    if(lua_isinteger(L, index))
    {
        const int i = LuaObject::getLuaInteger(L, index);
        if(i >= 1 && i <= Icesat2Parameters::NUM_SPOTS)
        {
            v = static_cast<Icesat2Parameters::spot_t>(i);
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
int convertToIndex(const Icesat2Parameters::spot_t& v)
{
    return static_cast<int>(v) - 1;
}

/*----------------------------------------------------------------------------
 * convertFromIndex - spot_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, Icesat2Parameters::spot_t& v)
{
    v = static_cast<Icesat2Parameters::spot_t>(index + 1);
}

/*----------------------------------------------------------------------------
 * convertToJson - surface_type_t
 *----------------------------------------------------------------------------*/
string convertToJson(const Icesat2Parameters::surface_type_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::SRT_DYNAMIC:        return "\"dynamic\"";
        case Icesat2Parameters::SRT_LAND:           return "\"land\"";
        case Icesat2Parameters::SRT_OCEAN:          return "\"ocean\"";
        case Icesat2Parameters::SRT_SEA_ICE:        return "\"sea_ice\"";
        case Icesat2Parameters::SRT_LAND_ICE:       return "\"land_ice\"";
        case Icesat2Parameters::SRT_INLAND_WATER:   return "\"inland_water\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid surface type: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - surface_type_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Parameters::surface_type_t& v)
{
    switch(v)
    {
        case Icesat2Parameters::SRT_DYNAMIC:        lua_pushstring(L, "dynamic");       break;
        case Icesat2Parameters::SRT_LAND:           lua_pushstring(L, "land");          break;
        case Icesat2Parameters::SRT_OCEAN:          lua_pushstring(L, "ocean");         break;
        case Icesat2Parameters::SRT_SEA_ICE:        lua_pushstring(L, "sea_ice");       break;
        case Icesat2Parameters::SRT_LAND_ICE:       lua_pushstring(L, "land_ice");      break;
        case Icesat2Parameters::SRT_INLAND_WATER:   lua_pushstring(L, "inland_water");  break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid surface type: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - surface_type_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, Icesat2Parameters::surface_type_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<Icesat2Parameters::surface_type_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "dynamic"))       v = Icesat2Parameters::SRT_DYNAMIC;
        else if(StringLib::match(str, "land"))          v = Icesat2Parameters::SRT_LAND;
        else if(StringLib::match(str, "ocean"))         v = Icesat2Parameters::SRT_OCEAN;
        else if(StringLib::match(str, "sea_ice"))       v = Icesat2Parameters::SRT_SEA_ICE;
        else if(StringLib::match(str, "land_ice"))      v = Icesat2Parameters::SRT_LAND_ICE;
        else if(StringLib::match(str, "inland_water"))  v = Icesat2Parameters::SRT_INLAND_WATER;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "surface type is an invalid value: %s", str);
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "surface type is an invalid type: %d", lua_type(L, index));
    }
}
