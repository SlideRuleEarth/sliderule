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

#include "AOI.h"
#include "OsApi.h"

/*----------------------------------------------------------------------------
 * AOI::Constructor
 *----------------------------------------------------------------------------*/
AOI::AOI (H5Object* hdf, const char* beam, const Icesat2Fields* parms, int readTimeoutMs):
    useRefId        (parms->atl13.reference_id.value > 0),
    atl13refid      (useRefId ? hdf : NULL, FString("%s/%s", beam, "atl13refid").c_str()),
    latitude        (hdf, FString("%s/%s", beam, "segment_lat").c_str()),
    longitude       (hdf, FString("%s/%s", beam, "segment_lon").c_str()),
    inclusionMask   {NULL},
    inclusionPtr    {NULL}
{
    try
    {
        /* Initialize Segment Range */
        lastSegment = -1;
        firstSegment = 0;
        numSegments = -1;

        /* Perform Initial Reference ID Search */
        if(useRefId)
        {
            /* Join ATL13 Reference ID Read */
            atl13refid.join(readTimeoutMs, true);

            bool first_segment_found = false;
            for(long i = 0; i < atl13refid.size; i++)
            {
                if(atl13refid[i] == parms->atl13.reference_id.value)
                {
                    if(!first_segment_found)
                    {
                        first_segment_found = true;
                        firstSegment = i;
                    }
                    lastSegment = i;
                }
            }

            /* Calculate Initial Number of Segments */
            numSegments = lastSegment - firstSegment + 1;
            if(numSegments <= 0) throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "reference id not found");
        }

        /* Join Latitude/Longitude Reads */
        latitude.join(readTimeoutMs, true);
        longitude.join(readTimeoutMs, true);

        /* Determine Spatial Extent */
        if(parms->regionMask.valid())
        {
            rasterregion(parms);
        }
        else if(parms->pointsInPolygon.value > 0)
        {
            polyregion(parms);
        }

        /* Check If Anything to Process */
        if(numSegments <= 0)
        {
            throw RunTimeException(DEBUG, RTE_RESOURCE_EMPTY, "empty spatial region");
        }

        /* Trim Geospatial Extent Datasets Read from HDF5 File */
        latitude.trim(firstSegment);
        longitude.trim(firstSegment);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * AOI::Destructor
 *----------------------------------------------------------------------------*/
AOI::~AOI (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * AOI::cleanup
 *----------------------------------------------------------------------------*/
void AOI::cleanup (void)
{
    delete [] inclusionMask;
    inclusionMask = NULL;
}

/*----------------------------------------------------------------------------
 * AOI::polyregion
 *----------------------------------------------------------------------------*/
void AOI::polyregion (const Icesat2Fields* parms)
{
    /* Find First Segment In Polygon */
    bool first_segment_found = false;
    int segment = firstSegment;
    const int max_segment = (numSegments < 0) ? longitude.size : numSegments;
    while(segment < max_segment)
    {
        /* Test Inclusion */
        const bool inclusion = parms->polyIncludes(longitude[segment], latitude[segment]);

        /* Check First Segment */
        if(!first_segment_found && inclusion)
        {
            /* If Coordinate Is In Polygon */
            first_segment_found = true;
            firstSegment = segment;
        }
        else if(first_segment_found && !inclusion)
        {
            /* If Coordinate Is NOT In Polygon */
            break; // full extent found!
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        numSegments = segment - firstSegment;
    }
    else
    {
        numSegments = 0;
    }
}

/*----------------------------------------------------------------------------
 * AOI::rasterregion
 *----------------------------------------------------------------------------*/
void AOI::rasterregion (const Icesat2Fields* parms)
{
    /* Find First Segment In Raster */
    bool first_segment_found = false;

    /* Allocate Inclusion Mask */
    inclusionMask = new bool [numSegments];
    inclusionPtr = inclusionMask;

    /* Loop Throuh Segments */
    int segment = firstSegment;
    const int max_segment = (numSegments < 0) ? longitude.size : numSegments;
    while(segment < max_segment)
    {
        /* Check Inclusion */
        const bool inclusion = parms->maskIncludes(longitude[segment], latitude[segment]);
        inclusionMask[segment] = inclusion;
        if(inclusion)
        {
            if(!first_segment_found)
            {
                first_segment_found = true;
                firstSegment = segment;
            }
            lastSegment = segment;
        }

        /* Bump Segment */
        segment++;
    }

    /* Set Number of Segments */
    if(first_segment_found)
    {
        numSegments = lastSegment - firstSegment + 1;

        /* Trim Inclusion Mask */
        inclusionPtr = &inclusionMask[firstSegment];
    }
    else
    {
        numSegments = 0;
    }
}
