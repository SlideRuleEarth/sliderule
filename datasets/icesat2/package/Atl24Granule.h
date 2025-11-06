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

#ifndef __atl24_granule__
#define __atl24_granule__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "LuaObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "H5Coro.h"
#include "H5Object.h"
#include "BathyFields.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class Atl24Granule: public LuaObject, public FieldDictionary
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static int  luaExport   (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<double>        atlas_sdp_gps_epoch;
        FieldElement<string>        data_end_utc;
        FieldElement<string>        data_start_utc;
        FieldElement<double>        end_delta_time;
        FieldElement<int32_t>       end_geoseg;
        FieldElement<double>        end_gpssow;
        FieldElement<int32_t>       end_gpsweek;
        FieldElement<int32_t>       end_orbit;
        FieldElement<string>        release;
        FieldElement<string>        granule_end_utc;
        FieldElement<string>        granule_start_utc;
        FieldElement<double>        start_delta_time;
        FieldElement<int32_t>       start_geoseg;
        FieldElement<double>        start_gpssow;
        FieldElement<int32_t>       start_gpsweek;
        FieldElement<int32_t>       start_orbit;
        FieldElement<string>        version;
        FieldElement<double>        crossing_time;
        FieldElement<double>        lan;
        FieldElement<int16_t>       orbit_number;
        FieldElement<int8_t>        sc_orient;
        FieldElement<double>        sc_orient_time;
        FieldElement<string>        sliderule;
        FieldElement<string>        profile;
        FieldElement<string>        stats;
        FieldElement<string>        extent;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        std::atomic<bool>           active;
        Thread*                     pid;
        Icesat2Fields*              parmsPtr;
        const Icesat2Fields&        parms;
        Publisher                   rqstQ;
        int                         readTimeoutMs;
        H5Object*                   hdf24; // ATL24 file

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl24Granule    (lua_State* L, Icesat2Fields* _parms, H5Object* _hdf24, const char* rqstq_name);
        virtual         ~Atl24Granule   (void) override;

        static void*    readingThread   (void* parm);
};

#endif  /* __atl24_granule__ */
