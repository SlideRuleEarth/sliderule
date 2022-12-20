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

#ifndef __raster_sampler__
#define __raster_sampler__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MsgQ.h"
#include "VrtRaster.h"

/******************************************************************************
 * RASTER SAMPLER DISPATCH CLASS
 ******************************************************************************/

class RasterSampler: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const char* sampleRecType;
        static const RecordObject::fieldDef_t sampleRecDef[];

        static const char* extentRecType;
        static const RecordObject::fieldDef_t extentRecDef[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Extent Sample Record */
        typedef struct {
            uint64_t            extent_id;
            uint16_t            raster_index;
            uint32_t            num_samples;
            VrtRaster::sample_t samples[];
        } extent_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate   (lua_State* L);
        static void     init        (void);
        static void     deinit      (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        VrtRaster*              raster;
        int                     rasterIndex;
        Publisher*              outQ;
        int                     extentSizeBytes;
        RecordObject::field_t   extentField;
        RecordObject::field_t   lonField;
        RecordObject::field_t   latField;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        RasterSampler           (lua_State* L, VrtRaster* _raster, int raster_index, const char* outq_name, const char* rec_type, const char* extent_key, const char* lon_key, const char* lat_key);
                        ~RasterSampler          (void);

        bool            processRecord           (RecordObject* record, okey_t key) override;
        bool            processTimeout          (void) override;
        bool            processTermination      (void) override;
};

#endif  /* __raster_sampler__ */
