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

        void getPointsFromFile    (const char* file_path, std::vector<ParquetSampler::point_info_t*>& points);
        void createParquetFile    (const char* input_file, const char* output_file);
        bool processSamples       (ParquetSampler::sampler_t* sampler);
        void clearColumns         (void);

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
        std::vector<std::shared_ptr<arrow::Field>>        new_fields;
        std::vector<std::shared_ptr<arrow::ChunkedArray>> new_columns;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        wkbpoint_t                    convertWKBToPoint   (const std::string& wkb_data);
        std::shared_ptr<arrow::Table> parquetFileToTable  (const char* file_path, const std::vector<const char*>& columnNames = {});
        void                          tableToParquetFile  (std::shared_ptr<arrow::Table> table, const char* file_path);
        void                          printParquetMetadata(const char* file_path);
        std::string                   createFileMap       (void);
};

#endif  /* __arrow_sampler_impl__ */
