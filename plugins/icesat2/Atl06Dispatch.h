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

#ifndef __atl06_dispatch__
#define __atl06_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MsgQ.h"
#include "GTArray.h"
#include "Atl03Reader.h"
#include "lua_parms.h"

/******************************************************************************
 * ATL06 DISPATCH CLASS
 ******************************************************************************/

class Atl06Dispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double SPEED_OF_LIGHT;
        static const double PULSE_REPITITION_FREQUENCY;
        static const double SPACECRAFT_GROUND_SPEED;
        static const double RDE_SCALE_FACTOR;
        static const double SIGMA_BEAM;
        static const double SIGMA_XMIT;

        static const int BATCH_SIZE = 256;

        static const char* elRecType;
        static const RecordObject::fieldDef_t elRecDef[];

        static const char* atRecType;
        static const RecordObject::fieldDef_t atRecDef[];

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Statistics --> TODO: NOT THREAD SAFE */
        typedef struct {
            uint32_t        h5atl03_rec_cnt;
            uint32_t        post_success_cnt;
            uint32_t        post_dropped_cnt;
        } stats_t;

        /* Elevation Measurement */
        typedef struct {
            uint32_t        segment_id;
            uint16_t        rgt;                    // reference ground track
            uint16_t        cycle;                  // cycle number
            uint8_t         spot;                   // 1 through 6, or 0 if unknown
            double          gps_time;               // seconds from GPS epoch
            double          latitude;
            double          longitude;
            double          h_mean;                 // meters from ellipsoid
            double          along_track_slope;
            double          across_track_slope;
        } elevation_t;

        /* ATL06 Record */
        typedef struct {
            elevation_t     elevation[BATCH_SIZE];
        } atl06_t;

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
            double      intercept;
            double      slope;
            double      x_min;
            double      x_max;
        } lsf_t;

        typedef struct {
            double      x;  // distance
            double      y;  // height
            double      r;  // residual
        } point_t;

       /* Algorithm Result */
        typedef struct {
            bool        provided;
            bool        violated_spread;
            bool        violated_count;
            bool        violated_iterations;
            elevation_t elevation;
            double      window_height;
            int32_t     photon_count;
            point_t*    photons;
        } result_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RecordObject*   recObj;
        atl06_t*        recData;
        Publisher*      outQ;

        Mutex           elevationMutex;
        int             elevationIndex;

        atl06_parms_t   parms;
        stats_t         stats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl06Dispatch                   (lua_State* L, const char* outq_name, const atl06_parms_t _parms);
                        ~Atl06Dispatch                  (void);

        bool            processRecord                   (RecordObject* record, okey_t key) override;
        bool            processTimeout                  (void) override;
        bool            processTermination              (void) override;

        void            calculateBeam                   (sc_orient_t sc_orient, track_t track, result_t* result);
        void            postResult                      (elevation_t* elevation);

        void            iterativeFitStage               (Atl03Reader::extent_t* extent, result_t* result);

        static int      luaStats                        (lua_State* L);
        static int      luaSelect                       (lua_State* L);

        static lsf_t    lsf                             (point_t* array, int size);
        static void     quicksort                       (point_t* array, int start, int end);
        static int      quicksortpartition              (point_t* array, int start, int end);

        // Unit Tests */
        friend class UT_Atl06Dispatch;
};

#endif  /* __atl06_dispatch__ */
