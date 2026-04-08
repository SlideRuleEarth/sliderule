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

#ifndef __casals1b_dataframe__
#define __casals1b_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "GeoDataFrame.h"
#include "LuaObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "H5Array.h"
#include "H5VarSet.h"
#include "H5Object.h"
#include "CasalsFields.h"
#include "AreaOfInterest.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

class Casals1bDataFrame: public GeoDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<time8_t>        time_ns   {Field::TIME_COLUMN, 0, "Return timestamp (Unix ns)"};
        FieldColumn<double>         latitude  {Field::Y_COLUMN,    0, "Latitude (degrees)"};
        FieldColumn<double>         longitude {Field::X_COLUMN,    0, "Longitude (degrees)"};
        FieldColumn<float>          refh      {Field::Z_COLUMN,    0, "Reference height (m)"};

        /* DataFrame MetaData */
        FieldElement<string>        granule;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* CASALS 1B Data Subclass */
        class Casals1bData
        {
            public:

                Casals1bData        (Casals1bDataFrame* df, const AreaOfInterest<double>& aoi);
                ~Casals1bData       (void) = default;

                H5Array<double>     delta_time;
                H5Array<double>     refh;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        std::atomic<bool>   active;
        Thread*             readerPid;
        const int           readTimeoutMs;
        Publisher*          outQ;
        CasalsFields*       parms;
        H5Object*           hdf1b;  // casals granule
        okey_t              dfKey;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Casals1bDataFrame   (lua_State* L, CasalsFields* _parms, H5Object* _hdf1b, const char* outq_name);
                        ~Casals1bDataFrame  (void) override;
        okey_t          getKey              (void) const override;
        static void*    subsettingThread    (void* parm);
};

#endif  /* __casals1b_dataframe__ */
