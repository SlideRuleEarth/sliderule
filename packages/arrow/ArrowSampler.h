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
 * ArrowSampler works on batches of records.  It expects the `batch_rec_type`
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

#include <set>

/******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/

class ArrowSamplerImpl; // arrow implementation

/******************************************************************************
 * PARQUET SAMPLER CLASS
 ******************************************************************************/

class ArrowSampler: public LuaObject
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
        typedef struct
        {
            const char*    rkey;
            RasterObject*  robj;
        } raster_info_t;

        typedef struct
        {
            double        x;
            double        y;
            double        gps;
        } point_info_t;

        typedef List<RasterSample*> sample_list_t;
        typedef struct BatchSampler
        {
            const char*                  rkey;
            RasterObject*                robj;
            ArrowSampler*                obj;
            std::vector<sample_list_t*>  samples;
            std::set<uint64_t>           file_ids;
            std::vector<std::pair<uint64_t, const char*>> filemap;

            explicit BatchSampler (const char* _rkey, RasterObject* _robj, ArrowSampler* _obj);
                    ~BatchSampler (void);
        } batch_sampler_t;

        typedef struct
        {
            uint32_t   start_indx;
            uint32_t   end_indx;
        } reader_range_t;

        typedef struct Reader
        {
            RasterObject*               robj;
            ArrowSampler*               obj;
            reader_range_t              range;
            std::vector<sample_list_t*> samples;

            explicit Reader (RasterObject* _robj, ArrowSampler* _obj);
                    ~Reader (void);
        } reader_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int                           luaCreate       (lua_State* L);
        static void                          init            (void);
        static void                          deinit          (void);
        const ArrowParms*                    getParms        (void);
        const char*                          getDataFile     (void);
        const char*                          getMetadataFile (void);
        const std::vector<batch_sampler_t*>& getBatchSamplers(void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                          active;
        Thread*                       mainPid;
        ArrowParms*                   parms;
        Publisher*                    outQ;
        std::vector<point_info_t*>    points;
        std::vector<batch_sampler_t*> batchSamplers;
        ArrowSamplerImpl*             impl;
        const char*                   dataFile;           // used locally to build parquet file
        const char*                   metadataFile;       // used locally to build json metadata file
        const char*                   outputPath;         // final destination of the data file
        const char*                   outputMetadataPath; // final destination of the metadata file


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        ArrowSampler     (lua_State* L, ArrowParms* _parms, const char* input_file,
                                          const char* outq_name, const std::vector<raster_info_t>& rasters);
                        ~ArrowSampler    (void) override;
        void            Delete           (void);
        void            getReadersRange  (std::vector<reader_range_t>& ranges, uint32_t maxNumThreads);
        static void*    mainThread       (void* parm);
        static void     batchSampling    (batch_sampler_t* sampler);
        static void*    readerThread     (void* parm);
};

#endif  /* __parquet_sampler__*/
