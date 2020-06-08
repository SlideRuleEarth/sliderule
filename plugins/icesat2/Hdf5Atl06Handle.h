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

#ifndef __hdf5_atl06__
#define __hdf5_atl06__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>

#include "Hdf5Handle.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 ATL06 HANDLER
 ******************************************************************************/

class Hdf5Atl06Handle: public Hdf5Handle
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const int MAX_PHOTONS_PER_SEGMENT = 0x10000;
        static const int TRACKS_PER_GROUND_TRACK = 2;
        static const int GT_LEFT = 0;
        static const int GT_RIGHT = 1;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Signal Confidence per Photon */
        typedef enum {
            CONF_SURFACE_HIGH = 4,
            CONF_SURFACE_MEDIUM = 3,
            CONF_SURFACE_LOW = 2,
            CONF_WITHIN_10M = 1,
            CONF_BACKGROUND = 0,
            CONF_NOT_CONSIDERED = -1,
            CONF_POSSIBLE_TEP = -2
        } signalConf_t;

        /* Segment */
        typedef struct {
            uint32_t    num_photons;
            uint32_t    distance_x_offset;  // double[]: dist_ph_along + segment_dist_x
            uint32_t    height_y_offset;    // double[]: h_ph
            uint32_t    confidence_offset;  // int8_t[]: signal_conf_ph[0]
        } segment_t;

        /* ATL06 Record */
        typedef struct {            
            uint8_t     track;
            uint32_t    segment_id;
            segment_t   photons[TRACKS_PER_GROUND_TRACK];
        } atl06Record_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        hid_t       handle;
        const char* dataName;
        uint8_t*    dataBuffer;
        int         dataSize;
        int         dataOffset;
        bool        rawMode;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                Hdf5Atl06Handle   (lua_State* L, const char* dataset_name, long id, bool raw_mode);
                ~Hdf5Atl06Handle  (void);

        bool    open                (const char* filename, DeviceObject::role_t role);
        int     read                (void* buf, int len);
        int     write               (const void* buf, int len);
        void    close               (void);
};

#endif  /* __hdf5_atl06__ */
