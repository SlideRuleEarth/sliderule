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

#ifndef __casals_parms__
#define __casals_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "AssetField.h"
#include "FieldEnumeration.h"
#include "FieldElement.h"
#include "RequestFields.h"
#include "GeoDataFrame.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

/******************/
/* Granule Fields */
/******************/
struct CasalsGranuleFields: public FieldDictionary
{

    FieldElement<int>   year {-1};      // CASALS granule observation date - year
    FieldElement<int>   month {-1};     // CASALS granule observation date - month
    FieldElement<int>   day {-1};       // CASALS granule observation date - day
    FieldElement<int>   version {-1};   // CASALS granule version

    CasalsGranuleFields(void);
    ~CasalsGranuleFields(void) override = default;

    void parseResource (const char* resource);
};

/***************/
/* Casals Fields */
/***************/
class CasalsFields: public RequestFields
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int64_t CASALS_SDP_EPOCH_GPS = 1198800018; // seconds to add to CASALS delta times to get GPS times

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
            return TimeLib::gps2systimeex(delta_time + (double)CASALS_SDP_EPOCH_GPS);
        }

        // returns resource as a string
        const char* getResource (void) const { return resource.value.c_str(); }

        // CRS support
        static const char* crsITRF2020() { static string crs = GeoDataFrame::loadCRSFile("EPSG9989.projjson"); return crs.c_str(); }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldList<string>       anc_fields; // list of fields to associate with an Casals subsetting request
        CasalsGranuleFields     granule_fields;  // Casals granule attributes

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        CasalsFields (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource);
        virtual ~CasalsFields (void) override = default;
        void fromLua (lua_State* L, int index) override;
};

#endif  /* __casals_parms__ */
