/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __atl03_reader__
#define __atl03_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GTArray.h"
#include "lua_parms.h"
#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"

/******************************************************************************
 * ATL03 READER
 ******************************************************************************/

class Atl03Reader
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Photon Fields */
        typedef struct {
            double          distance_x; // double[]: dist_ph_along
            double          height_y;   // double[]: h_ph
        } photon_t;

        /* Extent Record */
        typedef struct {
            uint8_t         reference_pair_track; // 1, 2, or 3
            uint8_t         spacecraft_orientation; // backward (0), forward (1), transition (2)
            uint16_t        reference_ground_track_start;
            uint16_t        reference_ground_track_end;
            uint16_t        cycle_start;
            uint16_t        cycle_end;
            uint32_t        segment_id[PAIR_TRACKS_PER_GROUND_TRACK]; // the id of the first ATL03 segment in range; TODO: just need one per extent
            double          segment_size[PAIR_TRACKS_PER_GROUND_TRACK]; // meters; TODO: just need one per extent
            double          background_rate[PAIR_TRACKS_PER_GROUND_TRACK]; // PE per second
            double          gps_time[PAIR_TRACKS_PER_GROUND_TRACK]; // seconds
            double          latitude[PAIR_TRACKS_PER_GROUND_TRACK];
            double          longitude[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        photon_count[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        photon_offset[PAIR_TRACKS_PER_GROUND_TRACK];
            photon_t        photons[]; // zero length field
        } extent_t;

        /* Statistics */
        typedef struct {
            uint32_t        segments_read[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        extents_filtered[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        extents_added;
            uint32_t        extents_sent;
        } stats_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* phRecType;
        static const RecordObject::fieldDef_t phRecDef[];

        static const char* exRecType;
        static const RecordObject::fieldDef_t exRecDef[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double ATL03_SEGMENT_LENGTH;
        static const double MAX_ATL06_SEGMENT_LENGTH;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        atl06_parms_t           parms;
        stats_t                 stats;
        List<RecordObject*>     extentList;
        int                     listIndex;
        bool                    connected;
        char*                   config;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Reader         (lua_State* L, const char* ur, atl06_parms_t _parms);
                            ~Atl03Reader        (void);

        bool                bufferData          (const char* url, int track);

        virtual bool        isConnected         (int num_open=0);   // is the file open
        virtual void        closeConnection     (void);             // close the file
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);             // returns file descriptor
        virtual const char* getConfig           (void);             // returns filename with attribute list

        static int          luaParms            (lua_State* L);
        static int          luaStats            (lua_State* L);
};

#endif  /* __atl03_reader__ */
