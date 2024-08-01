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

#ifndef __gedi04a_reader__
#define __gedi04a_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "StringLib.h"
#include "H5Array.h"

#include "GediParms.h"

/******************************************************************************
 * GEDI04A FOOTPRINT
 ******************************************************************************/

typedef struct {
    uint64_t        shot_number;
    int64_t         time_ns;
    double          latitude;
    double          longitude;
    float           agbd;
    float           elevation;
    float           solar_elevation;
    float           sensitivity;
    uint8_t         beam;
    uint8_t         flags;
} g04a_footprint_t;

/******************************************************************************
 * GEDI04A READER
 ******************************************************************************/

class Gedi04aReader: public FootprintReader<g04a_footprint_t>
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* fpRecType;
        static const RecordObject::fieldDef_t fpRecDef[];

        static const char* batchRecType;
        static const RecordObject::fieldDef_t batchRecDef[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Gedi04a Data Subclass */
        class Gedi04a
        {
            public:

                Gedi04a             (const info_t* info, const Region& region);
                ~Gedi04a            (void);

                H5Array<uint64_t>   shot_number;
                H5Array<double>     delta_time;
                H5Array<float>      agbd;
                H5Array<float>      elev_lowestmode;
                H5Array<float>      solar_elevation;
                H5Array<float>      sensitivity;
                H5Array<uint8_t>    degrade_flag;
                H5Array<uint8_t>    l2_quality_flag;
                H5Array<uint8_t>    l4_quality_flag;
                H5Array<uint8_t>    surface_flag;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Gedi04aReader           (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator=true);
                            ~Gedi04aReader          (void) override;
        static void*        subsettingThread        (void* parm);
};

#endif  /* __gedi04a_reader__ */
