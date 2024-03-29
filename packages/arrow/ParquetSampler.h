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

#ifndef __parquet_sampler__
#define __parquet_sampler__

/*
 * ParquetSampler works on batches of records.  It expects the `batch_rec_type`
 * passed into the constructor to be the type that defines each of the column headings,
 * then it expects to receive records that are arrays (or batches) of that record
 * type.  The field defined as an array is transparent to this class - it just
 * expects the record to be a single array.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "ArrowParms.h"
#include "RasterObject.h"
#include "OsApi.h"

/******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/

class ArrowImpl; // arrow implementation

/******************************************************************************
 * PARQUET SAMPLER CLASS
 ******************************************************************************/

class ParquetSampler: public LuaObject
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
        typedef std::vector<RasterSample*> sample_list_t;

        typedef struct Sampler {
            RasterObject*               robj;
            int64_t                     gps;    /* Overrides RasterObject 'closest_time' */
            std::vector<sample_list_t*> samples;
            ParquetSampler*             obj;

            explicit Sampler (RasterObject* _robj, ParquetSampler* _obj);
                    ~Sampler (void);
            void     clearSamples (void);
        } sampler_t;


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              luaCreate       (lua_State* L);
        static int              luaSample       (lua_State* L);
        static void             init            (void);
        static void             deinit          (void);

        void                    sample          (int64_t gps);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*                inputPath;
        const char*                outputPath;
        std::vector<Thread*>       samplerPids;
        std::vector<sampler_t*>    samplers;
        std::vector<OGRPoint>      points;

        ArrowImpl* impl; // private arrow data

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        ParquetSampler          (lua_State* L,
                                                 const char* input_file, const char* output_file,
                                                 const std::vector<RasterObject*>& robjs);
                        ~ParquetSampler         (void);
        static void*    samplerThread           (void* parm);
};

#endif  /* __parquet_sampler__*/
