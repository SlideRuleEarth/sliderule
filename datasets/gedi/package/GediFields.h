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
                default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam index: %d", beam_index);
            }
        }


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        AssetField                              asset;      // name of Asset in asset dictionary to use for granules
        FieldElement<string>                    resource;   // granule name (including file extension)
        FieldEnumeration<beam_t, NUM_BEAMS>     beams {true, true, true, true, true, true, true, true};
        FieldElement<bool>                      degrade_filter {false};
        FieldElement<bool>                      l2_quality_filter {false};
        FieldElement<bool>                      l4_quality_filter {false};
        FieldElement<bool>                      surface_filter {false};

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GediFields  (lua_State* L, const char* default_asset_name, const char* default_resource);
        ~GediFields (void) override = default;
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
