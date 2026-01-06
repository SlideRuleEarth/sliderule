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

#ifndef __areasubset__
#define __areasubset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "H5Array.h"
#include "H5Coro.h"
#include "Icesat2Fields.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/
namespace AreaSubset
{
    struct SubsetResult
    {
        long            first;
        long            count;
        vector<uint8_t> mask;
    };

    template<typename CoordT>
    SubsetResult computeSubset (const H5Array<CoordT>& latitude, const H5Array<CoordT>& longitude,
                                const Icesat2Fields* parms, long first_index, long num_indices)
    {
        SubsetResult result {first_index, num_indices, {}};

        /* Default span */
        if(result.count == H5Coro::ALL_ROWS || result.count < 0)
        {
            result.count = latitude.size;
        }

        /* Check for empty input */
        if(latitude.size <= 0 || longitude.size <= 0)
        {
            result.count = 0;
            return result;
        }

        /* Polygon selection */
        if(parms->pointsInPolygon.value > 0 && !parms->regionMask.valid())
        {
            bool found_first = false;
            int index = static_cast<int>(result.first);
            const long max_index = result.count;
            while(index < max_index)
            {
                const bool inclusion = parms->polyIncludes(longitude[index], latitude[index]);

                if(!found_first && inclusion)
                {
                    found_first = true;
                    result.first = index;
                }
                else if(found_first && !inclusion)
                {
                    break;
                }

                index++;
            }

            if(found_first)
            {
                result.count = index - result.first;
            }
            else
            {
                result.count = 0;
            }
        }
        /* Raster selection */
        else if(parms->regionMask.valid())
        {
            bool found_first = false;

            result.mask.assign(latitude.size, 0);
            long last_segment = 0;
            int index = static_cast<int>(result.first);
            const long max_index = result.count;
            while(index < max_index)
            {
                const bool inclusion = parms->maskIncludes(longitude[index], latitude[index]);
                result.mask[index] = inclusion ? 1 : 0;

                if(inclusion)
                {
                    if(!found_first)
                    {
                        found_first = true;
                        result.first = index;
                    }
                    last_segment = index;
                }

                index++;
            }

            if(found_first)
            {
                result.count = last_segment - result.first + 1;
            }
            else
            {
                result.count = 0;
            }
        }
        /* Default (no spatial filter) */
        else
        {
            /* result.count already set */
        }

        return result;
    }
}

#endif  /* __areasubset__ */
