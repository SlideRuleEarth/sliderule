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

#ifndef __arrow_sampler_impl__
#define __arrow_sampler_impl__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "ArrowCommon.h"
#include "LuaObject.h"
#include "ArrowSampler.h"
#include "OsApi.h"

#include <arrow/table.h>
#include <parquet/arrow/reader.h>

/******************************************************************************
 * ARROW SAMPLER CLASS
 ******************************************************************************/

class ArrowSamplerImpl
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef RasterObject::sample_list_t sample_list_t;
        typedef RasterObject::point_info_t point_info_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit ArrowSamplerImpl (ArrowSampler* _sampler);
        ~ArrowSamplerImpl         (void);

        void processInputFile     (const char* file_path, List<point_info_t*>& points);
        bool processSamples       (ArrowSampler::batch_sampler_t* sampler);
        void createOutpuFiles     (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        ArrowSampler*                                     arrowSampler;
        Mutex                                             mutex;
        std::vector<std::shared_ptr<arrow::Field>>        newFields;
        std::vector<std::shared_ptr<arrow::ChunkedArray>> newColumns;

        std::shared_ptr<arrow::io::ReadableFile>          inputFile;
        std::unique_ptr<parquet::arrow::FileReader>       reader;

        char* timeKey;
        char* xKey;
        char* yKey;
        bool  asGeo;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void                          getMetadata             (void);
        void                          getPoints               (List<point_info_t*>& points);
        void                          getXYPoints             (List<point_info_t*>& points);
        void                          getGeoPoints            (List<point_info_t*>& points);
        std::shared_ptr<arrow::Table> inputFileToTable        (const std::vector<const char*>& columnNames = {});
        std::shared_ptr<arrow::Table> addNewColumns           (const std::shared_ptr<arrow::Table>& table);
        bool                          makeColumnsWithLists    (ArrowSampler::batch_sampler_t* sampler);
        bool                          makeColumnsWithOneSample(ArrowSampler::batch_sampler_t* sampler);
        static RasterSample*          getFirstValidSample     (sample_list_t* slist);
        static void                   tableToParquet          (const std::shared_ptr<arrow::Table>& table,
                                                               const char* file_path);
        static void                   tableToCsv              (const std::shared_ptr<arrow::Table>& table,
                                                               const char* file_path);
        static void                   tableToFeather          (const std::shared_ptr<arrow::Table>& table,
                                                               const char* file_path);
        static std::shared_ptr<arrow::Table> removeGeometryColumn(std::shared_ptr<arrow::Table>& table);
        static ArrowCommon::wkbpoint_t convertWKBToPoint      (const std::string& wkb_data);
        static void                   printParquetMetadata    (const char* file_path);
        std::string                   createFileMap           (void);
        static void                   metadataToJson          (const std::shared_ptr<arrow::Table>& table,
                                                               const char* file_path);
};

#endif  /* __arrow_sampler_impl__ */
