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

#include "GediParms.h"

/******************************************************************************
 * GEDI04A READER
 ******************************************************************************/

class Gedi04aReader: public LuaObject
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

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Flags */
        typedef enum {
            DEGRADE_FLAG    = 0x01,
            L2_QUALITY_FLAG = 0x02,
            L4_QUALITY_FLAG = 0x04,
            SURFACE_FLAG    = 0x80
        } flags_t;

        /* Footprint Record */
        typedef struct {
            double          delta_time;
            double          latitude;
            double          longitude;
            double          agbd;
            double          elevation;
            double          solar_elevation;
            uint8_t         beam;
            uint8_t         flags;
        } footprint_t;

        /* Batch Record */
        typedef struct {
            footprint_t     footprint[BATCH_SIZE];
        } gedil4a_t;

        /* Statistics */
        typedef struct {
            uint32_t        footprints_read;
            uint32_t        fottprints_filtered;
            uint32_t        fottprints_sent;
            uint32_t        fottprints_dropped;
            uint32_t        fottprints_retried;
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
            Gedi04aReader*  reader;
            int             bream;
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

                HArray<double>      lat_lowestmode;
                HArray<double>      lon_lowestmode;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_footprint;
                long                num_footprints;
        };

        /* Gedi04a Data Subclass */
        class Gedi04a
        {
            public:

                Gedi04a             (info_t* info, Region& region);
                ~Gedi04a            (void);

                HArray<double>      delta_time;
                HArray<double>      agbd;
                HArray<double>      elev_lowestmode;
                HArray<double>      solar_elevation;
                HArray<uint8_t>     degrade_flag;
                HArray<uint8_t>     l2_quality_flag;
                HArray<uint8_t>     l4_quality_flag;
                HArray<uint8_t>     surface_flag;
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

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Gedi04aReader           (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, GediParms* _parms, bool _send_terminator=true);
                            ~Gedi04aReader          (void);

        static void*        subsettingThread        (void* parm);

        static int          luaParms                (lua_State* L);
        static int          luaStats                (lua_State* L);
};

#endif  /* __gedi04a_reader__ */
