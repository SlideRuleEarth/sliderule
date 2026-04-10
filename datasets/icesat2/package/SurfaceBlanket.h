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

#ifndef __surface_blanket__
#define __surface_blanket__

#include <limits>

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class SurfaceBlanket: public GeoDataFrame::FrameRunner
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate   (lua_State* L);
        bool            run         (GeoDataFrame* dataframe) override;

        static const GeoDataFrame::schema_description_t descriptions[];

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct result_t
        {
            float top_of_surface_h;
            float median_ground_h;
            uint16_t pflags;
            result_t(void):
                top_of_surface_h(std::numeric_limits<float>::quiet_NaN()),
                median_ground_h(std::numeric_limits<float>::quiet_NaN()),
                pflags(0) {};
        };

         /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        SurfaceBlanket  (lua_State* L, Icesat2Fields* _parms);
        ~SurfaceBlanket (void) override;
        void algorithm (const Atl03DataFrame& df, uint32_t start_photon, uint32_t num_photons, result_t& result);
        static double percentile(vector<float>& data, double p);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Icesat2Fields*  parms;
};

#endif