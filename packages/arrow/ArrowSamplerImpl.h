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

#include "ArrowImpl.h"
#include "LuaObject.h"
#include "ParquetSampler.h"
#include "OsApi.h"

/******************************************************************************
 * ARROW SAMPLER CLASS
 ******************************************************************************/

class ArrowSamplerImpl
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit ArrowSamplerImpl (ParquetSampler* _sampler);
        ~ArrowSamplerImpl         (void);

        void openInputFile        (const char* file_path);
        void getInputFileMetadata (ParquetSampler::record_info_t& recInfo);
        void getInputFilePoints   (std::vector<ParquetSampler::point_info_t*>& points);
        bool processSamples       (ParquetSampler::sampler_t* sampler);
        void createOutpuFile      (void);

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

        ParquetSampler*                                   parquetSampler;
        Mutex                                             mutex;
        std::vector<std::shared_ptr<arrow::Field>>        newFields;
        std::vector<std::shared_ptr<arrow::ChunkedArray>> newColumns;

        std::shared_ptr<arrow::io::ReadableFile>          inputFile;
        std::unique_ptr<parquet::arrow::FileReader>       reader;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        std::shared_ptr<arrow::Table> inputFileToTable    (const std::vector<const char*>& columnNames = {});
        std::shared_ptr<arrow::Table> appendSamplesColumns(const std::shared_ptr<arrow::Table> table);
        void                          tableToParquetFile  (const std::shared_ptr<arrow::Table> table, const char* file_path);
        void                          tableToCsvFile      (const std::shared_ptr<arrow::Table> table, const char* file_path);

        wkbpoint_t                    convertWKBToPoint   (const std::string& wkb_data);
        void                          printParquetMetadata(const char* file_path);
        std::string                   createFileMap       (void);
        std::string                   createMetadataFileName (const char* file_path);
        void                          tableMetadataToJson (const std::shared_ptr<arrow::Table> table, const char* file_path);
        void                          getXYPoints         (std::vector<ParquetSampler::point_info_t*>& points);
        void                          getGeoPoints        (std::vector<ParquetSampler::point_info_t*>& points);
};

#endif  /* __arrow_sampler_impl__ */
