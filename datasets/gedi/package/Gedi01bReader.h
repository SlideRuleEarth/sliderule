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

#ifndef __gedi01b_reader__
#define __gedi01b_reader__

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
#include "FootprintReader.h"
#include "GediFields.h"

/******************************************************************************
 * GEDI01B FOOTPRINT
 ******************************************************************************/

#define G01B_MAX_TX_SAMPLES 128
#define G01B_MAX_RX_SAMPLES 2048

typedef struct {
    uint64_t        shot_number;
    time8_t         time_ns;
    double          latitude;
    double          longitude;
    double          elevation_start;
    double          elevation_stop;
    double          solar_elevation;
    uint32_t        orbit;
    uint8_t         beam;
    uint8_t         flags;
    uint16_t        track;
    uint16_t        tx_size;
    uint16_t        rx_size;
    float           tx_waveform[G01B_MAX_TX_SAMPLES];
    float           rx_waveform[G01B_MAX_RX_SAMPLES];
} g01b_footprint_t;

/******************************************************************************
 * GEDI01B READER
 ******************************************************************************/
class Gedi01bReader: public FootprintReader<g01b_footprint_t>
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

        static int  luaCreate           (lua_State* L);
        static void init                (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Gedi01b Data Subclass */
        class Gedi01b
        {
            public:

                Gedi01b             (const info_t* info, const Region& region);
                ~Gedi01b            (void);

                H5Array<uint64_t>   shot_number;
                H5Array<double>     delta_time;
                H5Array<double>     elev_bin0;
                H5Array<double>     elev_lastbin;
                H5Array<float>      solar_elevation;
                H5Array<uint8_t>    degrade_flag;
                H5Array<uint16_t>   tx_sample_count;
                H5Array<uint64_t>   tx_start_index;
                H5Array<uint16_t>   rx_sample_count;
                H5Array<uint64_t>   rx_start_index;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Gedi01bReader           (lua_State* L, const char* outq_name, GediFields* _parms, bool _send_terminator=true);
                        ~Gedi01bReader          (void) override;
        static void*    subsettingThread        (void* parm);
};

#endif  /* __gedi01b_reader__ */
