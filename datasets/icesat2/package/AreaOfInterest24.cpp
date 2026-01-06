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

#include "AreaOfInterest24.h"
#include "AreaSubset.h"
#include "OsApi.h"

/*----------------------------------------------------------------------------
 * AreaOfInterest24::Constructor
 *----------------------------------------------------------------------------*/
AreaOfInterest24::AreaOfInterest24 (H5Object* hdf, const char* beam, const Icesat2Fields* parms, int readTimeoutMs):
    lat_ph          (hdf, FString("%s/%s", beam, "lat_ph").c_str()),
    lon_ph          (hdf, FString("%s/%s", beam, "lon_ph").c_str()),
    inclusion_mask  {NULL},
    inclusion_ptr   {NULL}
{
    try
    {
        /* Join Reads */
        lat_ph.join(readTimeoutMs, true);
        lon_ph.join(readTimeoutMs, true);

        /* Determine Spatial Extent */
        AreaSubset::SubsetResult subset = AreaSubset::computeSubset(lat_ph, lon_ph, parms, 0, H5Coro::ALL_ROWS);
        first_photon = subset.first;
        num_photons = subset.count;
        if(!subset.mask.empty())
        {
            inclusion_mask = new bool [subset.mask.size()];
            for(size_t i = 0; i < subset.mask.size(); i++)
            {
                inclusion_mask[i] = subset.mask[i] != 0;
            }
            inclusion_ptr = &inclusion_mask[first_photon];
        }

        /* Check If Anything to Process */
        if(num_photons <= 0)
        {
            throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        lat_ph.trim(first_photon);
        lon_ph.trim(first_photon);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest24::Destructor
 *----------------------------------------------------------------------------*/
AreaOfInterest24::~AreaOfInterest24 (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * AreaOfInterest24::cleanup
 *----------------------------------------------------------------------------*/
void AreaOfInterest24::cleanup (void)
{
    delete [] inclusion_mask;
    inclusion_mask = NULL;
}
