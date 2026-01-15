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

#ifndef __gedi_area_of_interest__
#define __gedi_area_of_interest__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "H5Array.h"
#include "H5Object.h"

#include "GediFields.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

class GediAreaOfInterest
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GediAreaOfInterest(H5Object* hdf, const char* latitude_name, const char* longitude_name,
            const GediFields* parms, int readTimeoutMs) :
            latitude(hdf, latitude_name),
            longitude(hdf, longitude_name),
            inclusion_mask(NULL),
            inclusion_ptr(NULL)
        {
            /* Join Reads */
            latitude.join(readTimeoutMs, true);
            longitude.join(readTimeoutMs, true);

            /* Initialize Region */
            first_index = 0;
            count = H5Coro::ALL_ROWS;

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
                count = latitude.size;
            }

            /* Check If Anything to Process */
            if(count <= 0)
            {
                cleanup();
                throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "empty spatial region");
            }

            /* Trim Geospatial Datasets Read from HDF5 File */
            latitude.trim(first_index);
            longitude.trim(first_index);
        }

        ~GediAreaOfInterest (void)
        {
            cleanup();
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Array<double>  latitude;
        H5Array<double>  longitude;

        bool*            inclusion_mask;
        bool*            inclusion_ptr;

        long             first_index;
        long             count;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void cleanup (void)
        {
            delete [] inclusion_mask;
            inclusion_mask = NULL;
        }

        void polyregion (const GediFields* parms)
        {
            /* Find First Footprint In Polygon */
            bool first_found = false;
            int footprint = 0;
            while(footprint < latitude.size)
            {
                /* Test Inclusion */
                const bool inclusion = parms->polyIncludes(longitude[footprint], latitude[footprint]);

                /* Find First Footprint */
                if(!first_found && inclusion)
                {
                    first_found = true;
                    first_index = footprint;
                }
                else if(first_found && !inclusion)
                {
                    break;
                }

                /* Bump Footprint */
                footprint++;
            }

            /* Set Number of Footprints */
            if(first_found)
            {
                count = footprint - first_index;
            }
        }

        void rasterregion (const GediFields* parms)
        {
            /* Check Size */
            if(latitude.size <= 0) return;

            /* Allocate Inclusion Mask */
            inclusion_mask = new bool [latitude.size];
            inclusion_ptr = inclusion_mask;

            /* Loop Through Footprints */
            bool first_found = false;
            long last_footprint = 0;
            int footprint = 0;
            while(footprint < latitude.size)
            {
                const bool inclusion = parms->maskIncludes(longitude[footprint], latitude[footprint]);
                inclusion_mask[footprint] = inclusion;

                if(inclusion)
                {
                    if(!first_found)
                    {
                        first_found = true;
                        first_index = footprint;
                    }
                    last_footprint = footprint;
                }

                footprint++;
            }

            if(first_found)
            {
                count = last_footprint - first_index + 1;
                inclusion_ptr = &inclusion_mask[first_index];
            }
        }
};

#endif  /* __gedi_area_of_interest__ */
