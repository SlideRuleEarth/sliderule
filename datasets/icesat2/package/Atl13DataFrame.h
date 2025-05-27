/*
 * Copyright (c) 2025, University of Washington
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

#ifndef __atl13_dataframe__
#define __atl13_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoDataFrame.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "H5Array.h"
#include "H5VarSet.h"
#include "H5Object.h"
#include "Icesat2Fields.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/
class Atl13DataFrame: public GeoDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<time8_t>        time_ns {Field::TIME_COLUMN};   // nanoseconds since GPS epoch
        FieldColumn<double>         latitude {Field::Y_COLUMN};
        FieldColumn<double>         longitude {Field::X_COLUMN};
        FieldColumn<float>          ht_ortho;
        FieldColumn<float>          ht_water_surf;
        FieldColumn<float>          stdev_water_surf;
        FieldColumn<float>          water_depth;

        /* DataFrame MetaData */
        FieldElement<uint8_t>       spot;                           // 1, 2, 3, 4, 5, 6
        FieldElement<uint8_t>       cycle;
        FieldElement<uint16_t>      rgt;
        FieldElement<uint8_t>       gt;                             // Icesat2Fields::gt_t
        FieldElement<string>        granule;                        // name of the ATL13 granule

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Area of Interest Subclass */
        class AreaOfInterest
        {
            public:

                explicit AreaOfInterest (const Atl13DataFrame* df);
                ~AreaOfInterest         (void);

                void cleanup            (void);
                void polyregion         (const Atl13DataFrame* df);
                void rasterregion       (const Atl13DataFrame* df);

                H5Array<int64_t>        atl13refid;
                H5Array<double>         latitude;
                H5Array<double>         longitude;

                bool*                   inclusionMask;
                bool*                   inclusionPtr;

                long                    firstSegment;
                long                    lastSegment;
                long                    numSegments;
        };

        /* Atl03 Data Subclass */
        class Atl13Data
        {
            public:

                Atl13Data           (Atl13DataFrame* df, const AreaOfInterest& aoi);
                ~Atl13Data          (void) = default;

                H5Array<int8_t>     sc_orient;
                H5Array<double>     delta_time;
                H5Array<float>      ht_ortho;
                H5Array<float>      ht_water_surf;
                H5Array<float>      stdev_water_surf;
                H5Array<float>      water_depth;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid;
        const int           readTimeoutMs;
        const char*         beam;
        Publisher*          outQ;
        Icesat2Fields*      parms;
        H5Object*           hdf13;  // atl13 granule
        okey_t              dfKey;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl13DataFrame      (lua_State* L, const char* beam_str, Icesat2Fields* _parms,
                                             H5Object* _hdf13, const char* outq_name);
                        ~Atl13DataFrame     (void) override;
        okey_t          getKey              (void) const override;
        static void*    subsettingThread    (void* parm);
};

#endif  /* __atl13_dataframe__ */
