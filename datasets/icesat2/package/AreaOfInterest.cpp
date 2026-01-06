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
#include "AreaSubset.h"

template<typename CoordT>
AreaOfInterestT<CoordT>::AreaOfInterestT (H5Object* hdf, const char* beam, const char* latitude_name, const char* longitude_name, const Icesat2Fields* parms, int readTimeoutMs):
    AreaOfInterestT(hdf, beam, latitude_name, longitude_name, NULL, parms, readTimeoutMs, std::function<void(const H5Array<int64_t>&, long&, long&)>())
{
}

template<typename CoordT>
AreaOfInterestT<CoordT>::AreaOfInterestT (H5Object* hdf, const char* beam, const char* latitude_name, const char* longitude_name, const char* refid_name,
                                          const Icesat2Fields* parms, int readTimeoutMs, const std::function<void(const H5Array<int64_t>&, long&, long&)>& prefilter):
    latitude        (hdf, FString("/%s/%s", beam, latitude_name).c_str()),
    longitude       (hdf, FString("/%s/%s", beam, longitude_name).c_str()),
    inclusion_mask  {NULL},
    inclusion_ptr   {NULL}
{
    try
    {
        /* Optional Reference ID */
        const bool use_refid = (refid_name != NULL) && (refid_name[0] != 0) && static_cast<bool>(prefilter);
        std::unique_ptr<H5Array<int64_t>> refid;

        /* Join Reads */
        if(use_refid)
        {
            refid = std::make_unique<H5Array<int64_t>>(hdf, FString("/%s/%s", beam, refid_name).c_str());
            refid->join(readTimeoutMs, true);
        }
        latitude.join(readTimeoutMs, true);
        longitude.join(readTimeoutMs, true);

        /* Initialize Region */
        first_segment = 0;
        num_segments = H5Coro::ALL_ROWS;

        /* Apply Prefilter */
        if(use_refid)
        {
            prefilter(*refid, first_segment, num_segments);

            /* Require Valid Prefilter Result */
            if(num_segments <= 0)
            {
                throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "reference id not found");
            }
        }

        /* Determine Spatial Extent */
        AreaSubset::SubsetResult subset = AreaSubset::computeSubset(latitude, longitude, parms, first_segment, num_segments);
        first_segment = subset.first;
        num_segments = subset.count;
        if(!subset.mask.empty())
        {
            inclusion_mask = new bool [subset.mask.size()];
            for(size_t i = 0; i < subset.mask.size(); i++)
            {
                inclusion_mask[i] = subset.mask[i] != 0;
            }
            inclusion_ptr = &inclusion_mask[first_segment];
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

template class AreaOfInterestT<double>;
template class AreaOfInterestT<float>;
