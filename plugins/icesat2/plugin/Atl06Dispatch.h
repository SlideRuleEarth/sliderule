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
#include "Icesat2Parms.h"

#include <vector>
using std::vector;

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
        static const double RDE_SCALE_FACTOR;
        static const double SIGMA_BEAM;
        static const double SIGMA_XMIT;

        static const int BATCH_SIZE = 256;

        static const uint16_t PFLAG_SPREAD_TOO_SHORT        = 0x0001;   // RqstParm::ALONG_TRACK_SPREAD
        static const uint16_t PFLAG_TOO_FEW_PHOTONS         = 0x0002;   // RqstParm::MIN_PHOTON_COUNT
        static const uint16_t PFLAG_MAX_ITERATIONS_REACHED  = 0x0004;   // RqstParm::MAX_ITERATIONS
        static const uint16_t PFLAG_OUT_OF_BOUNDS           = 0x0008;

        static const char* elRecType;
        static const RecordObject::fieldDef_t elRecDef[];
        static const char* atRecType;
        static const RecordObject::fieldDef_t atRecDef[];

        static const char* ancFieldRecType;
        static const RecordObject::fieldDef_t ancFieldRecDef[];
        static const char* ancRecType;
        static const RecordObject::fieldDef_t ancRecDef[];

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Statistics */
        typedef struct {
            std::atomic<uint32_t>   h5atl03_rec_cnt;
            std::atomic<uint32_t>   filtered_cnt;
            uint32_t                post_success_cnt;
            uint32_t                post_dropped_cnt;
        } stats_t;

        /* Elevation Measurement */
        typedef struct {
            uint64_t            extent_id;              // unique identifier
            uint32_t            segment_id;             // closest atl06 segment
            int32_t             photon_count;           // number of photons used in final elevation calculation
            uint16_t            pflags;                 // processing flags
            uint16_t            rgt;                    // reference ground track
            uint16_t            cycle;                  // cycle number
            uint8_t             spot;                   // 1 through 6, or 0 if unknown
            uint8_t             gt;                     // gt1l, gt1r, gt2l, gt2r, gt3l, gt3r
            int64_t             time_ns;                // nanoseconds from GPS epoch
            double              latitude;
            double              longitude;
            double              x_atc;                  // distance from the equator
            double              h_mean;                 // meters from ellipsoid
            double              h_sigma;
            float               dh_fit_dx;
            float               y_atc;
            float               window_height;
            float               rms_misfit;
        } elevation_t;

        /* ATL06 Record */
        typedef struct {
            elevation_t         elevation[BATCH_SIZE];
        } atl06_t;

        /* Ancillary Field Record */
        typedef struct {
            uint8_t             anc_type;     // Atl03Reader::anc_type_t
            uint8_t             field_index;    // position in request parameter list
            double              value;
        } anc_field_t;

        /* Ancillary Record */
        typedef struct {
            uint64_t            extent_id;
            anc_field_t         fields[];
        } anc_t;

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
            double          height;
            double          slope;
            double          y_sigma;
        } lsf_t;

        typedef struct {
            uint32_t        p;  // index into photon array
            double          r;  // residual
        } point_t;

       /* Algorithm Result */
        typedef struct {
            bool                provided;
            elevation_t         elevation;
            point_t*            photons;
            vector<anc_field_t> anc_fields;
            vector<double*>     anc_values;
        } result_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RecordObject        elevationRecord;
        atl06_t*            elevationRecordData;
        RecordObject*       ancillaryRecords[BATCH_SIZE]; // because there are variable number of fields, this cannot be predefined
        int                 ancillaryTotalSize;
        Publisher*          outQ;

        Mutex               postingMutex;
        int                 elevationIndex;
        int                 ancillaryIndex;

        Icesat2Parms*       parms;
        stats_t             stats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl06Dispatch                   (lua_State* L, const char* outq_name, Icesat2Parms* _parms);
                        ~Atl06Dispatch                  (void);

        bool            processRecord                   (RecordObject* record, okey_t key, recVec_t* records) override;
        bool            processTimeout                  (void) override;
        bool            processTermination              (void) override;

        void            iterativeFitStage               (Atl03Reader::extent_t* extent, result_t* result);
        void            postResult                      (result_t* result);

        static int      luaStats                        (lua_State* L);

        static lsf_t    lsf                             (Atl03Reader::extent_t* extent, result_t& result, bool final);
        static void     quicksort                       (point_t* array, int start, int end);
        static int      quicksortpartition              (point_t* array, int start, int end);

        /* Unit Tests */
        friend class UT_Atl06Dispatch;
};

#endif  /* __atl06_dispatch__ */
