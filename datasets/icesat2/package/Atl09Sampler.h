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

#ifndef __atl09_sampler__
#define __atl09_sampler__

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "H5Object.h"
#include "Icesat2Fields.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class Atl09Sampler: public GeoDataFrame::FrameRunner
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const int NUM_PROFILES = 3;
        static const int TEMPORAL_SUBSET_THRESHOLD = 3; // number of ATL09 field per group to read before temporal subsetting employed

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate   (lua_State* L);
        bool            run         (GeoDataFrame* dataframe) override;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        Atl09Sampler  (lua_State* L, Icesat2Fields* _parms, H5Object* _hdf09);
        ~Atl09Sampler (void) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Icesat2Fields*  parms;
        H5Object*       hdf09;
};

#endif