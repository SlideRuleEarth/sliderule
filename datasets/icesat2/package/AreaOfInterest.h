/*
 * Copyright (c) 2025, University of Washington
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

#ifndef __area_of_interest__
#define __area_of_interest__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "H5Array.h"
#include "H5Object.h"
#include "Icesat2Fields.h"
#include <functional>

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

template<typename CoordT>
class AreaOfInterestT
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

         AreaOfInterestT (H5Object* hdf, const char* beam, const char* latitude_name, const char* longitude_name, const Icesat2Fields* parms, int readTimeoutMs);
         AreaOfInterestT (H5Object* hdf, const char* beam, const char* latitude_name, const char* longitude_name, const char* refid_name,
                          const Icesat2Fields* parms, int readTimeoutMs, const std::function<void(const H5Array<int64_t>&, long&, long&)>& prefilter);
         ~AreaOfInterestT(void);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Array<CoordT>         latitude;
        H5Array<CoordT>         longitude;

        bool*                   inclusion_mask;
        bool*                   inclusion_ptr;

        long                    first_index;
        long                    count;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void cleanup            (void);
        void polyregion         (const Icesat2Fields* parms);
        void rasterregion       (const Icesat2Fields* parms);
};

using AreaOfInterest06 = AreaOfInterestT<double>;
using AreaOfInterest08 = AreaOfInterestT<float>;
using AreaOfInterest13 = AreaOfInterestT<double>;
using AreaOfInterest24 = AreaOfInterestT<double>;

#endif  /* __area_of_interest__ */
