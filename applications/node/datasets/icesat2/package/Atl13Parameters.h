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

#ifndef __atl13_fields__
#define __atl13_fields__

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
/* Atl13 Fields */
/****************/
struct Atl13Fields: public FieldMap<Field>
{
    typedef enum {
        LAKE                = 1,
        KNOWN_RESERVOIR     = 2,
        EPHEMERAL_WATER     = 4,
        RIVER               = 5,
        ESTUARY_BAY         = 6,
        COASTAL_WATER       = 7
    } body_type_t;

    typedef enum {
        GT_10000_KM2        = 1,
        GT_1000_KM2         = 2,
        GT_100_KM2          = 3,
        GT_10_KM2           = 4,
        GT_1_KM2            = 5,
        GT_01_KM2           = 6,
        GT_001_KM2          = 7,
        NOT_ASSIGNED        = 9
    } body_size_t;

    typedef enum {
        HYDRO_LAKES                         = 1,
        GLOBAL_LAKES_AND_WETLANDS_DATABASE  = 2,
        NAMED_MARINE_WATER_BODIES           = 3,
        GSHHG_SHORELINE                     = 4,
        GLOBAL_RIVER_WIDTHS_FROM_LANDSAT    = 5
    } body_source_t;

    FieldElement<int64_t>           reference_id {0};       // atl13refid
    FieldElement<string>            name;                   // lake name
    FieldElement<MathLib::coord_t>  coordinate {{0.0, 0.0}};// lake coordinate (contains)
    FieldList<string>               anc_fields;             // list of additional ATL13 fields

    Atl13Fields(void);
    ~Atl13Fields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/********************/
/* Atl13 Parameters */
/********************/
class Atl13Parameters: public Icesat2Parameters
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
                        Atl13Parameters     (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name = LUA_META_NAME);
        virtual         ~Atl13Parameters    (void) override = default;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Atl13Fields     atl13;  // atl13 subsetter parameters
};

#endif  /* __atl13_fields__ */
