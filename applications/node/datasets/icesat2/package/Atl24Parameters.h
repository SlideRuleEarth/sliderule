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

#ifndef __atl24_fields__
#define __atl24_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "Icesat2Parameters.h"
#include "FieldElement.h"

/******************************************************************************
 * CLASSES
 ******************************************************************************/

/****************/
/* Atl24 Fields */
/****************/
struct Atl24Fields: public FieldMap<Field>
{
    typedef enum {
        UNCLASSIFIED        = 0,
        BATHYMETRY          = 40,
        SEA_SURFACE         = 41,
        NUM_CLASSES         = 3
    } class_t;

    typedef enum {
        FLAG_OFF = 0,
        FLAG_ON = 1,
        NUM_FLAGS = 2
    } flag_t;

    FieldElement<bool>                      compact {true};                     // reduce number of fields from atl24
    FieldEnumeration<class_t,NUM_CLASSES>   class_ph {false, true, false};      // list of desired bathymetry classes of photons
    FieldElement<double>                    confidence_threshold {0.0};         // filter based on confidence
    FieldEnumeration<flag_t,NUM_FLAGS>      invalid_kd {true, true};            // filter on invalid kd flag
    FieldEnumeration<flag_t,NUM_FLAGS>      invalid_wind_speed {true, true};    // filter on invalid wind speed
    FieldEnumeration<flag_t,NUM_FLAGS>      low_confidence {true, true};        // filter on low confidence flag
    FieldEnumeration<flag_t,NUM_FLAGS>      night {true, true};                 // filter based on night flag
    FieldEnumeration<flag_t,NUM_FLAGS>      sensor_depth_exceeded {true, true}; // filter based on sensor depth exceeded flag
    FieldList<string>                       anc_fields;                         // list of additional ATL24 fields

    Atl24Fields(void);
    ~Atl24Fields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/********************/
/* ATL24 Parameters */
/********************/
class Atl24Parameters: public Icesat2Parameters
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual void    fromLua             (lua_State* L, int index) override;
                        Atl24Parameters     (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name = LUA_META_NAME);
        virtual         ~Atl24Parameters    (void) override = default;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Atl24Fields atl24;  // atl24 algorithm settings
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const Atl24Fields::class_t& v);
int convertToLua(lua_State* L, const Atl24Fields::class_t& v);
void convertFromLua(lua_State* L, int index, Atl24Fields::class_t& v);
int convertToIndex(const Atl24Fields::class_t& v);
void convertFromIndex(int index, Atl24Fields::class_t& v);

string convertToJson(const Atl24Fields::flag_t& v);
int convertToLua(lua_State* L, const Atl24Fields::flag_t& v);
void convertFromLua(lua_State* L, int index, Atl24Fields::flag_t& v);
int convertToIndex(const Atl24Fields::flag_t& v);
void convertFromIndex(int index, Atl24Fields::flag_t& v);

inline uint32_t toEncoding(Atl24Fields::class_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Atl24Fields::flag_t& v) { (void)v; return Field::INT32; }

#endif  /* __atl24_fields__ */
