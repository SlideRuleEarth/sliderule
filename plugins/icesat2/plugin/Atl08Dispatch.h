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

#ifndef __atl08_dispatch__
#define __atl08_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MsgQ.h"

#include "Atl03Reader.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * ATL08 DISPATCH CLASS
 ******************************************************************************/

class Atl08Dispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int BATCH_SIZE = 256;
        static const int NUM_PERCENTILES = 20;
        static const int MAX_BINS = 1000;

        static const int BIN_UNDERFLOW_FLAG = 0x0001;
        static const int BIN_OVERFLOW_FLAG  = 0x0002;

        static const char* vegRecType;
        static const RecordObject::fieldDef_t vegRecDef[];
        static const char* batchRecType;
        static const RecordObject::fieldDef_t batchRecDef[];

        static const char* waveRecType;
        static const RecordObject::fieldDef_t waveRecDef[];

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const double PercentileInterval[NUM_PERCENTILES];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Vegetation Structure */
        typedef struct {
            uint64_t            extent_id;              // unique identifier
            uint32_t            segment_id;             // closest atl06 segment
            uint16_t            pflags;                 // processing flags
            uint16_t            rgt;                    // reference ground track
            uint16_t            cycle;                  // cycle number
            uint8_t             spot;                   // 1 through 6, or 0 if unknown
            uint8_t             gt;                     // gt1l, gt1r, gt2l, gt2r, gt3l, gt3r
            uint32_t            photon_count;           // number of photons in segment
            uint32_t            ground_photon_count;    // number of photons labeled as ground in segment
            uint32_t            vegetation_photon_count;// number of photons labeled as canopy or top of canopy in segment
            uint8_t             landcover;              // atl08 land_segments/segments_landcover
            uint8_t             snowcover;              // atl08 land_segments/segments_snowcover
            int64_t             time_ns;                // nanoseconds from GPS epoch
            double              latitude;               // latitude of extent
            double              longitude;              // longitude of extent
            float               x_atc;                  // distance from the equator
            float               solar_elevation;        // atl03 solar elevation
            float               h_te_median;            // median terrain height for ground photons
            float               h_max_canopy;           // maximum relief height for canopy photons
            float               h_min_canopy;           // minimum relief height for canopy photons
            float               h_mean_canopy;          // average relief height for canopy photons
            float               h_canopy;               // 98th percentile relief height for canopy photons
            float               canopy_openness;        // standard deviation of relief height for canopy photons
            float               canopy_h_metrics[NUM_PERCENTILES];   // relief height at given percentile for canopy photons
        } vegetation_t;

        /* ATL06 Record */
        typedef struct {
            vegetation_t        vegetation[BATCH_SIZE];
        } atl08_t;

        /* Waveform Record */
        typedef struct {
            uint64_t            extent_id;              // unique identifier
            uint16_t            num_bins;               // size of waveform
            float               binsize;                // meters
            float               waveform[MAX_BINS];     // normalized waveform (1.0 == photon_count)
        } waveform_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RecordObject*       recObj;
        atl08_t*            recData;
        Publisher*          outQ;

        Mutex               batchMutex;
        int                 batchIndex;

        Icesat2Parms*       parms;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl08Dispatch                   (lua_State* L, const char* outq_name, Icesat2Parms* _parms);
                        ~Atl08Dispatch                  (void);

        bool            processRecord                   (RecordObject* record, okey_t key, recVec_t* records) override;
        bool            processTimeout                  (void) override;
        bool            processTermination              (void) override;

        void            geolocateResult                 (Atl03Reader::extent_t* extent, vegetation_t& result);
        void            phorealAlgorithm                (Atl03Reader::extent_t* extent, vegetation_t& result);
        void            postResult                      (vegetation_t* result);
        static void     quicksort                       (long* index_array, Atl03Reader::photon_t* ph_array, float Atl03Reader::photon_t::*field, int start, int end);
        static int      quicksortpartition              (long* index_array, Atl03Reader::photon_t* ph_array, float Atl03Reader::photon_t::*field, int start, int end);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        inline bool isVegetation (Atl03Reader::photon_t* ph)
        {
            return (ph->atl08_class == Icesat2Parms::ATL08_CANOPY || ph->atl08_class == Icesat2Parms::ATL08_TOP_OF_CANOPY);
        }

        inline bool isGround (Atl03Reader::photon_t* ph)
        {
            return (ph->atl08_class == Icesat2Parms::ATL08_GROUND);
        }
};

#endif  /* __atl08_dispatch__ */
