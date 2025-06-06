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

#ifndef __gedi_parms__
#define __gedi_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "AssetField.h"
#include "FieldEnumeration.h"
#include "FieldElement.h"
#include "RequestFields.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

/******************/
/* Granule Fields */
/******************/
struct GediGranuleFields: public FieldDictionary
{

    FieldElement<int>   year {-1};      // GEDI granule observation date - year
    FieldElement<int>   doy {-1};       // GEDI granule observation date - day of year
    FieldElement<int>   orbit {-1};     // GEDI granule orbit
    FieldElement<int>   region {-1};    // GEDI granule region
    FieldElement<int>   track {-1};     // GEDI granule region
    FieldElement<int>   version {-1};   // GEDI granule version

    GediGranuleFields(void);
    ~GediGranuleFields(void) override = default;

    void parseResource (const char* resource);
};

/***************/
/* GEDI Fields */
/***************/
class GediFields: public RequestFields
{
    public:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Beams */
        typedef enum {
            BEAM0000 = 0,
            BEAM0001 = 1,
            BEAM0010 = 2,
            BEAM0011 = 3,
            BEAM0101 = 5,
            BEAM0110 = 6,
            BEAM1000 = 8,
            BEAM1011 = 11,
            NUM_BEAMS = 8
        } beam_t;

        /* Flags */
        typedef enum {
            DEGRADE_FLAG_MASK    = 0x01,
            L2_QUALITY_FLAG_MASK = 0x02,
            L4_QUALITY_FLAG_MASK = 0x04,
            SURFACE_FLAG_MASK    = 0x80
        } flags_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int64_t GEDI_SDP_EPOCH_GPS = 1198800018; // seconds to add to GEDI delta times to get GPS times

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        // returns nanoseconds since Unix epoch, no leap seconds
        static time8_t deltatime2timestamp (double delta_time)
        {
            return TimeLib::gps2systimeex(delta_time + (double)GEDI_SDP_EPOCH_GPS);
        }

        // returns group name in h5 file given beam
        static const char* beam2group (int beam_index)
        {
            switch(beam_index)
            {
                case 0:  return "BEAM0000";
                case 1:  return "BEAM0001";
                case 2:  return "BEAM0010";
                case 3:  return "BEAM0011";
                case 4:  return "BEAM0101";
                case 5:  return "BEAM0110";
                case 6:  return "BEAM1000";
                case 7:  return "BEAM1011";
                default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam index: %d", beam_index);
            }
        }

        // returns resource as a string
        const char* getResource (void) const { return resource.value.c_str(); }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldEnumeration<beam_t, NUM_BEAMS>     beams {true, true, true, true, true, true, true, true};
        FieldElement<bool>                      degrade_filter {false};
        FieldElement<bool>                      l2_quality_filter {false};
        FieldElement<bool>                      l4_quality_filter {false};
        FieldElement<bool>                      surface_filter {false};
        FieldList<string>                       anc_fields; // list of fields to associate with an GEDI subsetting request
        GediGranuleFields                       granule_fields;  // GEDI granule attributes

        // backwards compatibility
        FieldElement<int>                       degrade_flag {0};
        FieldElement<int>                       l2_quality_flag {0};
        FieldElement<int>                       l4_quality_flag {0};
        FieldElement<int>                       surface_flag {0};

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GediFields (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource);
        virtual ~GediFields (void) override = default;
        void fromLua (lua_State* L, int index) override;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const GediFields::beam_t& v);
int convertToLua(lua_State* L, const GediFields::beam_t& v);
void convertFromLua(lua_State* L, int index, GediFields::beam_t& v);
int convertToIndex(const GediFields::beam_t& v);
void convertFromIndex(int index, GediFields::beam_t& v);

inline uint32_t toEncoding(GediFields::beam_t& v) { (void)v; return Field::INT32; }

#endif  /* __gedi_parms__ */
