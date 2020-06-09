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

#ifndef __hdf5_atl03__
#define __hdf5_atl03__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>

#include "Hdf5Handle.h"
#include "GTArray.h"
#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 ATL06 HANDLER
 ******************************************************************************/

class Hdf5Atl03Handle: public Hdf5Handle
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
            surfaceType_t   srt;
            signalConf_t    cnf;
        } parms_t;

        /* Photon Fields */
        typedef struct {
            double      distance_x; // double[]: dist_ph_along + segment_dist_x
            double      height_y;   // double[]: h_ph
        } photon_t;

        /* Segment Record */
        typedef struct {
            uint8_t         track;
            uint32_t        segment_id;
            uint32_t        num_photons[PAIR_TRACKS_PER_GROUND_TRACK];
            photon_t        photons[]; // zero length field
        } segment_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const parms_t DefaultParms;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int                 groundTrack;
        parms_t             extractParms;
        List<segment_t*>    segmentList;
        int                 listIndex;
        bool                rawMode;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    Hdf5Atl03Handle     (lua_State* L, int track, long id, bool raw_mode);
                    ~Hdf5Atl03Handle    (void);

        bool        open                (const char* filename, DeviceObject::role_t role);
        int         read                (void* buf, int len);
        int         write               (const void* buf, int len);
        void        close               (void);

        static int  luaConfig           (lua_State* L);
        static int  luaParms            (lua_State* L);
};

#endif  /* __hdf5_atl03__ */
