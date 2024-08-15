/*
 * Copyright (c) 2023, University of Texas
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
 * 3. Neither the name of the University of Texas nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __bathy_refraction_corrector__
#define __bathy_refraction_corrector__

#include "OsApi.h"
#include "LuaObject.h"
#include "H5Array.h"
#include "BathyParms.h"

/******************************************************************************
 * BATHY OPENOCEANS
 ******************************************************************************/

class BathyRefractionCorrector: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* GLOBAL_WATER_RI_MASK;
        static const double GLOBAL_WATER_RI_MASK_MAX_LAT;
        static const double GLOBAL_WATER_RI_MASK_MIN_LAT;
        static const double GLOBAL_WATER_RI_MASK_MAX_LON;
        static const double GLOBAL_WATER_RI_MASK_MIN_LON;
        static const double GLOBAL_WATER_RI_MASK_PIXEL_SIZE;

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate   (lua_State* L);
        static void     init        (void);
        uint64_t        run         (BathyParms::extent_t& extent,
                                     const H5Array<float>& ref_el,
                                     const H5Array<float>& ref_az) const;
    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        BathyRefractionCorrector    (lua_State* L, BathyParms* _parms);
        ~BathyRefractionCorrector   (void) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        BathyParms*         parms;
        GeoLib::TIFFImage*  waterRiMask;

};

#endif