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
#include "RasterObject.h"

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

        static const char* rsSampleRecType;
        static const RecordObject::fieldDef_t rsSampleRecDef[];

        static const char* rsGeoRecType;
        static const RecordObject::fieldDef_t rsGeoRecDef[];

        static const char* zsSampleRecType;
        static const RecordObject::fieldDef_t zsSampleRecDef[];

        static const char* zsGeoRecType;
        static const RecordObject::fieldDef_t zsGeoRecDef[];

        static const char* fileIdRecType;
        static const RecordObject::fieldDef_t fileIdRecDef[];

        static const int RASTER_KEY_MAX_LEN = 16; // maximum number of characters to represent raster

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Sample */
        typedef struct {
            double              value;
            double              time;
            uint64_t            file_id;
            uint32_t            flags;
        } sample_t;

        /* Extent Sample Record */
        typedef struct {
            uint64_t            index;
            char                raster_key[RASTER_KEY_MAX_LEN];
            uint32_t            num_samples;
            sample_t            samples[];
        } rs_geo_t;

        /* Zonal Statistics Record */
        typedef struct {
            uint64_t            index;
            char                raster_key[RASTER_KEY_MAX_LEN];
            uint32_t            num_samples;
            RasterSample_t      samples[];
        } zs_geo_t;

        /* File Directory Entry Record */
        typedef struct {
            uint64_t            file_id;
            char                file_name[];
        } file_directory_entry_t;

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

        RasterObject*           raster;
        const char*             rasterKey;
        Publisher*              outQ;
        int                     recordSizeBytes;
        RecordObject::field_t   indexField;
        RecordObject::field_t   lonField;
        RecordObject::field_t   latField;
        RecordObject::field_t   timeField;
        RecordObject::field_t   heightField;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        RasterSampler           (lua_State* L, RasterObject* _raster, const char* raster_key,
                                                 const char* outq_name, const char* rec_type,
                                                 const char* index_key, const char* lon_key, const char* lat_key,
                                                 const char* time_key, const char* height_key);
                        ~RasterSampler          (void);

        bool            processRecord           (RecordObject* record, okey_t key) override;
        bool            processTimeout          (void) override;
        bool            processTermination      (void) override;
};

#endif  /* __raster_sampler__ */
