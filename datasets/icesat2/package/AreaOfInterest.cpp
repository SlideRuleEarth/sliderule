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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "AreaOfInterest.h"

template<typename CoordT>
AreaOfInterestT<CoordT>::AreaOfInterestT (H5Object* hdf, const char* beam, const char* latitude_name, const char* longitude_name, const Icesat2Fields* parms, int readTimeoutMs):
    latitude        (hdf, FString("/%s/%s", beam, latitude_name).c_str()),
    longitude       (hdf, FString("/%s/%s", beam, longitude_name).c_str()),
    inclusion_mask  {NULL},
    inclusion_ptr   {NULL}
{
    try
    {
        /* Join Reads */
        latitude.join(readTimeoutMs, true);
        longitude.join(readTimeoutMs, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;

        /* Determine Spatial Extent */
        if(parms->regionMask.valid())
        {
            rasterregion(parms);
        }
        else if(parms->pointsInPolygon.value > 0)
        {
            polyregion(parms);
        }
        else
        {
            num_segments = latitude.size;
        }

        /* Check If Anything to Process */
        if(num_segments <= 0)
        {
            throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        latitude.trim(first_segment);
        longitude.trim(first_segment);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::Destructor
 *----------------------------------------------------------------------------*/
template<typename CoordT>
AreaOfInterestT<CoordT>::~AreaOfInterestT (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::cleanup
 *----------------------------------------------------------------------------*/
template<typename CoordT>
void AreaOfInterestT<CoordT>::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::polyregion
 *----------------------------------------------------------------------------*/
template<typename CoordT>
void AreaOfInterestT<CoordT>::polyregion (const Icesat2Fields* parms)
{
    bool first_segment_found = false;
    int segment = 0;
    while(segment < latitude.size)
    {
        const bool inclusion = parms->polyIncludes(longitude[segment], latitude[segment]);

        if(!first_segment_found && inclusion)
        {
            first_segment_found = true;
            first_segment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            break;
        }

        segment++;
    }

    if(first_segment_found)
    {
        num_segments = segment - first_segment;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest::rasterregion
 *----------------------------------------------------------------------------*/
template<typename CoordT>
void AreaOfInterestT<CoordT>::rasterregion (const Icesat2Fields* parms)
{
    bool first_segment_found = false;

    if(latitude.size <= 0)
    {
        return;
    }

    inclusion_mask = new bool [latitude.size];
    inclusion_ptr = inclusion_mask;

    long last_segment = 0;
    int segment = 0;
    while(segment < latitude.size)
    {
        const bool inclusion = parms->maskIncludes(longitude[segment], latitude[segment]);
        inclusion_mask[segment] = inclusion;

        if(inclusion)
        {
            if(!first_segment_found)
            {
                first_segment_found = true;
                first_segment = segment;
            }
            last_segment = segment;
        }

        segment++;
    }

    if(first_segment_found)
    {
        num_segments = last_segment - first_segment + 1;
        inclusion_ptr = &inclusion_mask[first_segment];
    }
}

template class AreaOfInterestT<double>;
template class AreaOfInterestT<float>;
