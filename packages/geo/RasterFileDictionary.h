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

#ifndef __raster_file_dictionary__
#define __raster_file_dictionary__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include <set>

/******************************************************************************
 * RASTER FIlE DICTIONARY CLASS
 ******************************************************************************/

class RasterFileDictionary
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/


        explicit RasterFileDictionary(uint64_t _keySpace=0): keySpace(_keySpace<<32) {}
                ~RasterFileDictionary(void) = default;

        uint64_t    add      (const std::string& fileName, bool sample=false);
        const char* get      (uint64_t fileId);
        void        setSample(uint64_t sampleFileId);
        void        clear    (void);

        const std::set<uint64_t>& getSampleIds(void) const;
        RasterFileDictionary copy(void);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Dictionary<uint64_t>     fileDict;       // Dictionary to store raster file names
        std::vector<std::string> fileVector;     // Vector to store raster file names by id (index derived from lower 32 bits of fileDict value)
        std::set<uint64_t>       sampleIdSet;    // Set to store raster fileIds used only in returned RasterSamples
        uint64_t                 keySpace;       // Key space
        static Mutex             mutex;          // Mutex for thread safety, only add() method is thread safe
};

#endif  /* __raster_file_dictionary__ */
