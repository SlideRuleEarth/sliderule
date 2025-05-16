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

#ifndef __atl24_dataframe__
#define __atl24_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoDataFrame.h"
#include "LuaObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "H5Array.h"
#include "H5VarSet.h"
#include "H5Object.h"
#include "Icesat2Fields.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/
class Atl24DataFrame: public GeoDataFrame
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
        FieldColumn<int8_t>         class_ph;
        FieldColumn<double>         confidence;
        FieldColumn<time8_t>        time_ns {Field::TIME_COLUMN};   // nanoseconds since GPS epoch
        FieldColumn<float>          ellipse_h;
        FieldColumn<uint8_t>        invalid_kd;
        FieldColumn<uint8_t>        invalid_wind_speed;
        FieldColumn<double>         lat_ph {Field::Y_COLUMN};
        FieldColumn<double>         lon_ph {Field::X_COLUMN};
        FieldColumn<uint8_t>        low_confidence_flag;
        FieldColumn<uint8_t>        night_flag;
        FieldColumn<float>          ortho_h;
        FieldColumn<uint8_t>        sensor_depth_exceeded;
        FieldColumn<float>          sigma_thu;
        FieldColumn<float>          sigma_tvu;
        FieldColumn<float>          surface_h;
        FieldColumn<double>         x_atc;
        FieldColumn<float>          y_atc;

        /* DataFrame MetaData */
        FieldElement<uint8_t>       spot {0, Field::META_COLUMN};   // 1, 2, 3, 4, 5, 6
        FieldElement<uint8_t>       cycle {0, Field::META_COLUMN};
        FieldElement<uint8_t>       region {0, Field::META_COLUMN};
        FieldElement<uint16_t>      rgt {0, Field::META_COLUMN};
        FieldElement<uint8_t>       gt {0, Field::META_COLUMN};     // Icesat2Fields::gt_t
        FieldElement<string>        granule;                        // name of the ATL24 granule

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

                explicit AreaOfInterest (const Atl24DataFrame* df);
                ~AreaOfInterest         (void);

                void cleanup            (void);
                void polyregion         (const Atl24DataFrame* df);
                void rasterregion       (const Atl24DataFrame* df);

                H5Array<double>         lat_ph;
                H5Array<double>         lon_ph;

                bool*                   inclusion_mask;
                bool*                   inclusion_ptr;

                long                    first_photon;
                long                    num_photons;
        };

        /* Atl24 Data Subclass */
        class Atl24Data
        {
            public:

                Atl24Data           (Atl24DataFrame* df, const AreaOfInterest& aoi);
                ~Atl24Data          (void) = default;

                bool                compact;

                H5Array<int8_t>     sc_orient;

                H5Array<int8_t>     class_ph;
                H5Array<double>     confidence;
                H5Array<double>     delta_time;
                H5Array<float>      ellipse_h;
                H5Array<uint8_t>    invalid_kd;
                H5Array<uint8_t>    invalid_wind_speed;
                H5Array<uint8_t>    low_confidence_flag;
                H5Array<uint8_t>    night_flag;
                H5Array<float>      ortho_h;
                H5Array<uint8_t>    sensor_depth_exceeded;
                H5Array<float>      sigma_thu;
                H5Array<float>      sigma_tvu;
                H5Array<float>      surface_h;
                H5Array<double>     x_atc;
                H5Array<float>      y_atc;

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
        H5Object*           hdf24;  // atl24 granule
        okey_t              dfKey;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl24DataFrame      (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf24, const char* outq_name);
                        ~Atl24DataFrame     (void) override;
        okey_t          getKey              (void) const override;
        static void*    subsettingThread    (void* parm);
};

#endif  /* __atl24_dataframe__ */
