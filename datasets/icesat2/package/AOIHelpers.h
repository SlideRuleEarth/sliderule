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

#ifndef __aoi_helpers__
#define __aoi_helpers__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <functional>
#include <memory>

#include "H5Array.h"

/******************************************************************************
 * TYPES
 ******************************************************************************/

typedef struct {
    long                        first_index;
    long                        num_indices;
    std::unique_ptr<bool[]>     inclusion_mask;
    bool*                       inclusion_ptr;
} segment_aoi_t;

typedef struct {
    long                        first_segment;
    long                        num_segments;
    long                        first_count;
    long                        num_counts;
    std::unique_ptr<bool[]>     inclusion_mask;
    bool*                       inclusion_ptr;
} segment_count_aoi_t;

/******************************************************************************
 * HELPERS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * computeSegmentAoI
 *----------------------------------------------------------------------------*/
inline segment_aoi_t computeSegmentAoI(const H5Array<double>& latitude,
                                       const H5Array<double>& longitude,
                                       const std::function<bool(double,double)>& polyIncludes,
                                       const std::function<bool(double,double)>& maskIncludes,
                                       bool use_poly,
                                       bool use_mask)
{
    segment_aoi_t result {};
    result.first_index = 0;
    result.num_indices = H5Coro::ALL_ROWS;
    result.inclusion_mask = std::unique_ptr<bool[]>();
    result.inclusion_ptr = NULL;

    if(latitude.size <= 0)
    {
        result.num_indices = 0;
        return result;
    }

    if(use_mask)
    {
        bool first_found = false;
        long last_index = 0;
        result.inclusion_mask = std::unique_ptr<bool[]>(new bool[latitude.size]);
        result.inclusion_ptr = result.inclusion_mask.get();

        for(long idx = 0; idx < latitude.size; idx++)
        {
            const bool inclusion = maskIncludes ? maskIncludes(longitude[idx], latitude[idx]) : false;
            result.inclusion_mask[idx] = inclusion;

            if(inclusion)
            {
                if(!first_found)
                {
                    first_found = true;
                    result.first_index = idx;
                }
                last_index = idx;
            }
        }

        if(first_found)
        {
            result.num_indices = last_index - result.first_index + 1;
            result.inclusion_ptr = result.inclusion_mask.get() + result.first_index;
        }
        else
        {
            result.num_indices = 0;
            result.inclusion_ptr = NULL;
        }
    }
    else if(use_poly)
    {
        bool first_found = false;
        long idx = 0;
        while(idx < latitude.size)
        {
            const bool inclusion = polyIncludes ? polyIncludes(longitude[idx], latitude[idx]) : false;

            if(!first_found && inclusion)
            {
                first_found = true;
                result.first_index = idx;
            }
            else if(first_found && !inclusion)
            {
                break;
            }

            idx++;
        }

        if(first_found)
        {
            result.num_indices = idx - result.first_index;
        }
        else
        {
            result.num_indices = 0;
        }
    }
    else
    {
        result.first_index = 0;
        result.num_indices = latitude.size;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * computeSegmentCountAoI
 *----------------------------------------------------------------------------*/
template<class CountArray>
inline segment_count_aoi_t computeSegmentCountAoI(const H5Array<double>& latitude,
                                                  const H5Array<double>& longitude,
                                                  const CountArray& segment_counts,
                                                  const std::function<bool(double,double)>& polyIncludes,
                                                  const std::function<bool(double,double)>& maskIncludes,
                                                  bool use_poly,
                                                  bool use_mask)
{
    segment_count_aoi_t result {};
    result.first_segment = 0;
    result.num_segments = H5Coro::ALL_ROWS;
    result.first_count = 0;
    result.num_counts = H5Coro::ALL_ROWS;
    result.inclusion_mask = std::unique_ptr<bool[]>();
    result.inclusion_ptr = NULL;

    if(segment_counts.size <= 0)
    {
        result.num_segments = 0;
        result.num_counts = 0;
        return result;
    }

    if(use_mask)
    {
        bool first_found = false;
        long last_segment = 0;
        long current_counts = 0;
        result.inclusion_mask = std::unique_ptr<bool[]>(new bool[segment_counts.size]);
        result.inclusion_ptr = result.inclusion_mask.get();

        for(long segment = 0; segment < segment_counts.size; segment++)
        {
            if(segment_counts[segment] != 0)
            {
                const bool inclusion = maskIncludes ? maskIncludes(longitude[segment], latitude[segment]) : false;
                result.inclusion_mask[segment] = inclusion;

                if(!first_found)
                {
                    if(inclusion)
                    {
                        first_found = true;
                        result.first_segment = segment;
                        last_segment = segment;
                        current_counts = segment_counts[segment];
                        result.num_counts = current_counts;
                    }
                    else
                    {
                        result.first_count += segment_counts[segment];
                    }
                }
                else
                {
                    current_counts += segment_counts[segment];

                    if(inclusion)
                    {
                        result.num_counts = current_counts;
                        last_segment = segment;
                    }
                }
            }
            else
            {
                result.inclusion_mask[segment] = false;
            }
        }

        if(first_found)
        {
            result.num_segments = last_segment - result.first_segment + 1;
            result.inclusion_ptr = result.inclusion_mask.get() + result.first_segment;
        }
        else
        {
            result.num_segments = 0;
            result.num_counts = 0;
            result.inclusion_ptr = NULL;
        }
    }
    else if(use_poly)
    {
        bool first_found = false;
        long segment = 0;
        while(segment < segment_counts.size)
        {
            bool inclusion = false;
            if(segment_counts[segment] != 0)
            {
                inclusion = polyIncludes ? polyIncludes(longitude[segment], latitude[segment]) : false;
            }

            if(!first_found && inclusion)
            {
                first_found = true;
                result.first_segment = segment;
            }
            else if(first_found && !inclusion && segment_counts[segment] != 0)
            {
                break;
            }

            if(first_found)
            {
                result.num_counts += segment_counts[segment];
            }
            else
            {
                result.first_count += segment_counts[segment];
            }

            segment++;
        }

        if(first_found)
        {
            result.num_segments = segment - result.first_segment;
        }
        else
        {
            result.num_segments = 0;
            result.num_counts = 0;
        }
    }
    else
    {
        result.first_segment = 0;
        result.num_segments = segment_counts.size;
        result.first_count = 0;
        result.num_counts = 0;
        for(long segment = 0; segment < segment_counts.size; segment++)
        {
            result.num_counts += segment_counts[segment];
        }
    }

    return result;
}

#endif  /* __aoi_helpers__ */
