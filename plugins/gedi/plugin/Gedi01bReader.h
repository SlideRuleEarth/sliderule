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

#include "GediParms.h"

/******************************************************************************
 * GEDI01B READER
 ******************************************************************************/

class Gedi01bReader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int BATCH_SIZE = 256;

        static const char* fpRecType;
        static const RecordObject::fieldDef_t fpRecDef[];

        static const char* batchRecType;
        static const RecordObject::fieldDef_t batchRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const int MAX_TX_SAMPLES = 128;
        static const int MAX_RX_SAMPLES = 2048;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Footprint Record */
        typedef struct {
            uint64_t        shot_number;
            int64_t         time_ns;
            double          latitude;
            double          longitude;
            double          elevation_start;
            double          elevation_stop;
            double          solar_elevation;
            uint8_t         beam;
            uint8_t         flags;
            uint16_t        tx_size;
            uint16_t        rx_size;
            float           tx_waveform[MAX_TX_SAMPLES];
            float           rx_waveform[MAX_RX_SAMPLES];
        } footprint_t;

        /* Batch Record */
        typedef struct {
            footprint_t     footprint[BATCH_SIZE];
        } gedi01b_t;

        /* Statistics */
        typedef struct {
            uint32_t        footprints_read;
            uint32_t        footprints_filtered;
            uint32_t        footprints_sent;
            uint32_t        footprints_dropped;
            uint32_t        footprints_retried;
        } stats_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            Gedi01bReader*  reader;
            int             beam;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                Region              (info_t* info);
                ~Region             (void);

                void cleanup        (void);
                void polyregion     (info_t* info);
                void rasterregion   (info_t* info);

                H5Array<double>     lat_bin0;
                H5Array<double>     lon_bin0;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_footprint;
                long                num_footprints;
        };

        /* Gedi01b Data Subclass */
        class Gedi01b
        {
            public:

                Gedi01b             (info_t* info, Region& region);
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
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid[GediParms::NUM_BEAMS];
        Mutex               threadMut;
        int                 threadCount;
        int                 numComplete;
        Asset*              asset;
        const char*         resource;
        bool                sendTerminator;
        const int           read_timeout_ms;
        Publisher*          outQ;
        GediParms*          parms;
        stats_t             stats;
        H5Coro::context_t   context;
        RecordObject        batchRecord;
        int                 batchIndex;
        gedi01b_t*          batchData;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Gedi01bReader           (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator=true);
                            ~Gedi01bReader          (void);
        void                postRecordBatch         (stats_t* local_stats);
        static void*        subsettingThread        (void* parm);
        static int          luaStats                (lua_State* L);
};

#endif  /* __gedi01b_reader__ */
