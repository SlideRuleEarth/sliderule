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
#include "MathLib.h"
#include "GTArray.h"
#include "Atl03Device.h"

/******************************************************************************
 * ATL06 DISPATCH CLASS
 ******************************************************************************/

class Atl06Dispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double PULSE_REPITITION_FREQUENCY;
        static const double SPACECRAFT_GROUND_SPEED;
        static const double RDE_SCALE_FACTOR;
        static const double SIGMA_BEAM;
        static const double SIGMA_XMIT;
        static const double H_WIN_MIN;

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

        /* Algorithm Stages */
        typedef enum {
            STAGE_LSF = 0,  // least squares fit
            STAGE_RSR = 1,  // robust spread of residuals
            NUM_STAGES = 2
        } stages_t;

        /* Statistics --> TODO: NOT THREAD SAFE */
        typedef struct {
            uint32_t    h5atl03_rec_cnt;
            uint32_t    post_success_cnt;
            uint32_t    post_dropped_cnt;
        } stats_t;

        /* Elevation Measurement */
        typedef struct {
            uint32_t        segment_id;
            uint16_t        grt;                    // ground reference track
            uint16_t        cycle;
            double          gps_time;               // seconds from GPS epoch
            double          latitude;
            double          longitude;
            double          height;                 // meters from ellipsoid
            double          along_track_slope;
            double          across_track_slope;
        } elevation_t;

        /* Algorithm Result */
        typedef struct {
            bool                    status[PAIR_TRACKS_PER_GROUND_TRACK];
            elevation_t             elevation[PAIR_TRACKS_PER_GROUND_TRACK];
            int32_t                 photon_count[PAIR_TRACKS_PER_GROUND_TRACK];
            Atl03Device::photon_t*  photons[PAIR_TRACKS_PER_GROUND_TRACK];
        } result_t;

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
         * Data
         *--------------------------------------------------------------------*/

        RecordObject*   recObj;
        atl06_t*        recData;
        Publisher*      outQ;

        Mutex           elevationMutex;
        int             elevationIndex;

        stats_t         stats;
        bool            stages[NUM_STAGES];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl06Dispatch                   (lua_State* L, const char* otuq_name);
                        ~Atl06Dispatch                  (void);

        bool            processRecord                   (RecordObject* record, okey_t key) override;
        bool            processTimeout                  (void) override;

        void            populateElevation               (elevation_t* elevation);

        bool            leastSquaresFitStage            (Atl03Device::extent_t* extent, result_t* result);
        bool            robustSpreadOfResidualsStage    (Atl03Device::extent_t* extent, result_t* result);

        static int      luaStats                        (lua_State* L);
        static int      luaSelect                       (lua_State* L);
};

#endif  /* __atl06_dispatch__ */
