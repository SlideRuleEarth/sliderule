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

#ifndef __atl03_device__
#define __atl03_device__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GTArray.h"
#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 ATL06 HANDLER
 ******************************************************************************/

class Atl03Device: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Signal Confidence per Photon */
        typedef enum {
            CNF_POSSIBLE_TEP = -2,
            CNF_NOT_CONSIDERED = -1,
            CNF_BACKGROUND = 0,
            CNF_WITHIN_10M = 1,
            CNF_SURFACE_LOW = 2,
            CNF_SURFACE_MEDIUM = 3,
            CNF_SURFACE_HIGH = 4
        } signalConf_t;

        /* Surface Types for Signal Confidence */
        typedef enum {
            SRT_LAND = 0,
            SRT_OCEAN = 1,
            SRT_SEA_ICE = 2,
            SRT_LAND_ICE = 3,
            SRT_INLAND_WATER = 4
        } surfaceType_t;

        /* Extraction Parameters */
        typedef struct {
            surfaceType_t   surface_type;           // surface reference type (used to select signal confidence column)
            signalConf_t    signal_confidence;      // minimal allowed signal confidence
            double          along_track_spread;     // minimal required along track spread of photons in extent (meters)
            int             photon_count;           // minimal required photons in extent
            double          extent_length;          // length of ATL06 extent (meters)
            double          extent_step;            // resolution of the ATL06 extent (meters)
        } parms_t;

        /* Photon Fields */
        typedef struct {
            double          distance_x; // double[]: dist_ph_along
            double          height_y;   // double[]: h_ph
        } photon_t;

        /* Extent Record */
        typedef struct {
            uint8_t         pair_reference_track; // 1, 2, or 3
            uint32_t        segment_id[PAIR_TRACKS_PER_GROUND_TRACK]; // the id of the first ATL03 segment in range
            double          gps_time[PAIR_TRACKS_PER_GROUND_TRACK]; // seconds
            double          start_distance[PAIR_TRACKS_PER_GROUND_TRACK]; // meters
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

        static const parms_t DefaultParms;

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

        parms_t                 parms;
        stats_t                 stats;
        List<RecordObject*>     extentList;
        int                     listIndex;
        bool                    connected;
        char*                   config;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Device     (lua_State* L, const char* ur, parms_t _parms);
                            ~Atl03Device    (void);

        bool                h5open              (const char* url);

        virtual bool        isConnected         (int num_open=0);   // is the file open
        virtual void        closeConnection     (void);             // close the file
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);             // returns file descriptor
        virtual const char* getConfig           (void);             // returns filename with attribute list

        static int          luaParms            (lua_State* L);
        static int          luaStats            (lua_State* L);
};

#endif  /* __atl03_device__ */
