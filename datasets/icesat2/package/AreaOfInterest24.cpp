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

        /* Initialize AreaOfInterest */
        first_photon = 0;
        num_photons = H5Coro::ALL_ROWS;

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
            num_photons = lat_ph.size;
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

/*----------------------------------------------------------------------------
 * AreaOfInterest24::polyregion
 *----------------------------------------------------------------------------*/
void AreaOfInterest24::polyregion (const Icesat2Fields* parms)
{
    /* Find First Photon In Polygon */
    bool first_photon_found = false;
    int photon = 0;
    while(photon < lat_ph.size)
    {
        /* Test Inclusion */
        const bool inclusion = parms->polyIncludes(lon_ph[photon], lat_ph[photon]);

        /* Check First Photon */
        if(!first_photon_found)
        {
            /* If Coordinate Is In Polygon */
            if(inclusion)
            {
                /* Set First Segment */
                first_photon_found = true;
                first_photon = photon;
            }
            else
            {
                /* Update Photon Index */
                first_photon++;
            }
        }
        else
        {
            /* If Coordinate Is NOT In Polygon */
            if(!inclusion)
            {
                break; // full extent found!
            }
        }

        /* Bump Photon */
        photon++;
    }

    /* Set Number of Photons */
    if(first_photon_found)
    {
        num_photons = photon - first_photon;
    }
}

/*----------------------------------------------------------------------------
 * AreaOfInterest24::rasterregion
 *----------------------------------------------------------------------------*/
void AreaOfInterest24::rasterregion (const Icesat2Fields* parms)
{
    /* Find First Photon In Polygon */
    bool first_photon_found = false;

    /* Check Size */
    if(lat_ph.size <= 0)
    {
        return;
    }

    /* Allocate Inclusion Mask */
    inclusion_mask = new bool [lat_ph.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Segments */
    long last_photon = 0;
    int photon = 0;
    while(photon < lat_ph.size)
    {
        if(lat_ph[photon] != 0)
        {
            /* Check Inclusion */
            const bool inclusion = parms->maskIncludes(lon_ph[photon], lat_ph[photon]);
            inclusion_mask[photon] = inclusion;

            /* Check For First Photon */
            if(!first_photon_found)
            {
                /* If Coordinate Is In Raster */
                if(inclusion)
                {
                    first_photon_found = true;

                    /* Set First Photon */
                    first_photon = photon;
                    last_photon = photon;
                }
                else
                {
                    /* Update Photon Index */
                    first_photon += lat_ph[photon];
                }
            }
            else
            {
                /* If Coordinate Is In Raster */
                if(inclusion)
                {
                    /* Update Last Photon */
                    last_photon = photon;
                }
            }
        }
        else
        {
            inclusion_mask[photon] = false;
        }

        /* Bump Photon */
        photon++;
    }

    /* Set Number of Photons */
    if(first_photon_found)
    {
        num_photons = last_photon - first_photon + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_photon];
    }
    else
    {
        num_photons = 0;
    }
}
