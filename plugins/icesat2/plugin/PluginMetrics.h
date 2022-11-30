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

#ifndef __plugin_metrics__
#define __plugin_metrics__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "RqstParms.h"

/******************************************************************************
 * METRICS FOR PLUGIN
 ******************************************************************************/

class PluginMetrics
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_POINTS_IN_POLY = 10;

         /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            REGION_CONTINENTAL_US = 0,
            REGION_ALASKA = 1,
            REGION_CANADA = 2,
            REGION_GREENLAND = 3,
            REGION_CENTRAL_AMERICA = 4,
            REGION_SOUTH_AMERICA = 5,
            REGION_AFRICA = 6,
            REGION_MIDDLE_EAST = 7,
            REGION_EUROPE = 8,
            REGION_NORTH_ASIA = 9,
            REGION_SOUTH_ASIA = 10,
            REGION_OCEANIA = 11,
            REGION_ANTARCTICA = 12,
            REGION_UNKNOWN = 13,
            NUM_REGIONS = 14
        } regions_t;

        typedef struct {
            const char* name;
            MathLib::proj_t proj;
            MathLib::coord_t coords[MAX_POINTS_IN_POLY];
            MathLib::point_t points[MAX_POINTS_IN_POLY];
            int num_points;
        } region_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* CATEGORY;
        static const char* REGION_METRIC;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static bool         init            (void);
        static region_t*    region2struct   (regions_t region);
        static bool         setRegion       (RqstParms* parms);
        static bool         checkRegion     (MathLib::coord_t coord, regions_t r);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static int32_t regionMetricIds[NUM_REGIONS];
};

#endif  /* __plugin_metrics__ */
