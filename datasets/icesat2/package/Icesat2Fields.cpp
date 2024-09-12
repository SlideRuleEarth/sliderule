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
#include "FieldDictionary.h"
#include "LuaObject.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int Icesat2Fields::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new Icesat2Fields(L, 1, {}));
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
Icesat2Fields::Icesat2Fields(lua_State* L, int index, const std::initializer_list<FieldDictionary::entry_t>& init_list):
    RequestFields (L, index, {  {"asset",               &assetName},
                                {"resource",            &resource},
                                {"srt",                 &surfaceType},
                                {"pass_invalid",        &passInvalid},
                                {"dist_in_seg",         &distInSeg},
                                {"cnf",                 &atl03Cnf},
                                {"quality_ph",          &qualityPh},
                                {"atl08_class",         &atl08Class},
                                {"beams",               &beams},
                                {"yapc",                &yapc},
                                {"track",               &track},
                                {"maxi",                &maxIterations},
                                {"cnt",                 &minPhotonCount},
                                {"ats",                 &alongTrackSpread},
                                {"H_min_win",           &minWindow},
                                {"sigma_r_max",         &maxRobustDispersion},
                                {"len",                 &extentLength},
                                {"res",                 &extentStep},
                                {"phoreal",             &phoreal},
                                {"atl03_geo_fields",    &atl03GeoFields},
                                {"atl03_ph_fields",     &atl03PhFields},
                                {"atl06_fields",        &atl06Fields},
                                {"atl08_fields",        &atl08Fields},
                                {"atl13_fields",        &atl13Fields}  })
{
    // add additional fields to dictionary
    for(const FieldDictionary::entry_t elem: init_list) 
    {
        fields.add(elem.name, elem);
    }

    // handle asset
    asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetName.value.c_str(), Asset::OBJECT_TYPE));
    if(!asset) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to find asset %s", assetName.value.c_str());

    // handle signal confidence options
    if(atl03Cnf.provided && atl03Cnf.asSingle)
    {        
        // when signal confidence is supplied as a single option
        // instead of setting only that option, treat it as a level
        // where every selection that is that option or above is set
        for(int i = 1; i < NUM_SIGNAL_CONF; i++)
        {
            // set every element true after the first one found that is set to true
            atl03Cnf.values[i] = atl03Cnf.values[i - 1]; 
        }
    }

    // handle YAPC options
    if(yapc.provided)
    {
        stages[STAGE_YAPC] = true;
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

    // handle ATL08 fields
    if(atl08Fields.provided)
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
 * Destructor
 *----------------------------------------------------------------------------*/
Icesat2Fields::~Icesat2Fields(void)
{
    if(asset) asset->releaseLuaObject();
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

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
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid PhoREAL geolocation: %d", static_cast<int>(v));
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
        else throw RunTimeException(CRITICAL, RTE_ERROR, "geolocation is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "geolocation is an invalid type:%d", lua_type(L, index));
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
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid signal confidence: %d", static_cast<int>(v));
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
        else throw RunTimeException(CRITICAL, RTE_ERROR, "signal confidence is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "signal confidence is an invalid type: %d", lua_type(L, index));
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
 * convertToLua - quality_ph_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::quality_ph_t& v)
{
    switch(v)
    {
        case Icesat2Fields::QUALITY_NOMINAL:                    lua_pushstring(L, "atl03_quality_nominal");             break;
        case Icesat2Fields::QUALITY_POSSIBLE_AFTERPULSE:        lua_pushstring(L, "atl03_quality_afterpulse");          break;
        case Icesat2Fields::QUALITY_POSSIBLE_IMPULSE_RESPONSE:  lua_pushstring(L, "atl03_quality_impulse_response");    break;
        case Icesat2Fields::QUALITY_POSSIBLE_TEP:               lua_pushstring(L, "atl03_quality_tep");                 break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid photon quality: %d", static_cast<int>(v));
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
        if     (StringLib::match(str, "atl03_quality_nominal"))             v = Icesat2Fields::QUALITY_NOMINAL;
        else if(StringLib::match(str, "atl03_quality_afterpulse"))          v = Icesat2Fields::QUALITY_POSSIBLE_AFTERPULSE;
        else if(StringLib::match(str, "atl03_quality_impulse_response"))    v = Icesat2Fields::QUALITY_POSSIBLE_IMPULSE_RESPONSE;
        else if(StringLib::match(str, "atl03_quality_tep"))                 v = Icesat2Fields::QUALITY_POSSIBLE_TEP;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "photon quality is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "photon quality is an invalid type: %d", lua_type(L, index));
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
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid atl08 classification: %d", static_cast<int>(v));
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
        else throw RunTimeException(CRITICAL, RTE_ERROR, "atl08 classification is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "atl08 classification is an invalid type: %d", lua_type(L, index));
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
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid ground track: %d", static_cast<int>(v));
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
        else throw RunTimeException(CRITICAL, RTE_ERROR, "ground track is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "ground track is an invalid type: %d", lua_type(L, index));
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
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid spot: %d", static_cast<int>(v));
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
        int i = LuaObject::getLuaInteger(L, index);
        if(i >= 1 && i <= Icesat2Fields::NUM_SPOTS)
        {
            v = static_cast<Icesat2Fields::spot_t>(i);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid spot: %d", i);
        }
    }    
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "spot is an invalid type: %d", lua_type(L, index));
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
 * convertToLua - surface_type_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const Icesat2Fields::surface_type_t& v)
{
    switch(v)
    {
        case Icesat2Fields::SRT_DYNAMIC:        lua_pushstring(L, "SRT_DYNAMIC");       break;
        case Icesat2Fields::SRT_LAND:           lua_pushstring(L, "SRT_LAND");          break;
        case Icesat2Fields::SRT_OCEAN:          lua_pushstring(L, "SRT_OCEAN");         break;
        case Icesat2Fields::SRT_SEA_ICE:        lua_pushstring(L, "SRT_SEA_ICE");       break;
        case Icesat2Fields::SRT_LAND_ICE:       lua_pushstring(L, "SRT_LAND_ICE");      break;
        case Icesat2Fields::SRT_INLAND_WATER:   lua_pushstring(L, "SRT_INLAND_WATER");  break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid surface type: %d", static_cast<int>(v));
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
        if     (StringLib::match(str, "SRT_DYNAMIC"))       v = Icesat2Fields::SRT_DYNAMIC;
        else if(StringLib::match(str, "SRT_LAND"))          v = Icesat2Fields::SRT_LAND;
        else if(StringLib::match(str, "SRT_OCEAN"))         v = Icesat2Fields::SRT_OCEAN;
        else if(StringLib::match(str, "SRT_SEA_ICE"))       v = Icesat2Fields::SRT_SEA_ICE;
        else if(StringLib::match(str, "SRT_LAND_ICE"))      v = Icesat2Fields::SRT_LAND_ICE;
        else if(StringLib::match(str, "SRT_INLAND_WATER"))  v = Icesat2Fields::SRT_INLAND_WATER;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "surface type is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "surface type is an invalid type: %d", lua_type(L, index));
    }
}
