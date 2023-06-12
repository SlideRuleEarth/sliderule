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

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "core.h"
#include "Icesat2Parms.h"
#include "PluginMetrics.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* PluginMetrics::CATEGORY = "icesat2";
const char* PluginMetrics::REGION_METRIC = "hits";

int32_t PluginMetrics::regionMetricIds[NUM_REGIONS];

PluginMetrics::region_t continental_us = {
    .name = "continental_us",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {-126.73828125, 49.38237278700955},
        {-124.45312499999999, 34.45221847282654},
        {-99.31640625, 25.403584973186703},
        {-80.85937499999999, 23.885837699862005},
        {-66.4453125, 44.15068115978094},
        {-66.884765625, 47.69497434186282},
        {-90.791015625, 49.49667452747045},
        {-126.73828125, 49.38237278700955}
    },
    .points = {},
    .num_points = 8
};

PluginMetrics::region_t alaska = {
    .name = "alaska",
    .proj = MathLib::NORTH_POLAR,
    .coords = {
        {-130.25390625, 53.85252660044951},
        {-128.32031249999997, 57.231502991478926},
        {-139.5703125, 62.02152819100765},
        {-140.2734375, 70.08056215839737},
        {-167.16796875, 72.55449849665266},
        {-171.03515625, 50.401515322782366},
        {-145.8984375, 59.5343180010956},
        {-130.25390625, 53.85252660044951}
     },
    .points = {},
    .num_points = 8
};

PluginMetrics::region_t canada = {
    .name = "canada",
    .proj = MathLib::NORTH_POLAR,
    .coords = {
        {-125.859375, 48.22467264956519},
        {-94.921875,  48.45835188280866},
        {-81.2109375, 38.54816542304656},
        {-48.515625, 48.22467264956519},
        {-75.5859375, 78.34941069014629},
        {-54.84375, 83.23642648170203},
        {-85.78125, 83.40004205976699},
        {-143.7890625, 71.74643171904148},
        {-142.734375, 58.63121664342478},
        {-125.859375, 48.22467264956519}
    },
    .points = {},
    .num_points = 10
};

PluginMetrics::region_t greenland = {
    .name = "greenland",
    .proj = MathLib::NORTH_POLAR,
    .coords = {
        {-74.70703125, 78.27820145542813},
        {-46.40625, 56.46249048388979},
        {-18.45703125, 70.19999407534661},
        {-9.667968749999998, 81.5182718765338},
        {-30.234375, 84.12497319391095},
        {-62.05078125, 82.1664460084773},
        {-74.70703125, 78.27820145542813}
     },
    .points = {},
    .num_points = 7
};

PluginMetrics::region_t central_america = {
    .name = "central_america",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {-120.9375, 34.59704151614417},
        {-115.6640625, 24.686952411999155},
        {-82.79296874999999, 3.8642546157214084},
        {-61.87499999999999, 19.145168196205297},
        {-76.46484375, 25.799891182088334},
        {-96.328125, 26.745610382199022},
        {-104.58984375, 32.69486597787505},
        {-120.9375, 34.59704151614417}
     },
     .points = {{0,0}},
    .num_points = 8
};

PluginMetrics::region_t south_america = {
    .name = "south_america",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {-30.585937499999996, -4.740675384778361},
        {-72.0703125, 17.308687886770034},
        {-85.60546875, -1.4061088354351594},
        {-72.7734375, -22.105998799750566},
        {-78.22265625, -51.94426487902876},
        {-62.05078125, -59.62332522313022},
        {-30.585937499999996, -4.740675384778361}
     },
    .points = {},
    .num_points = 7
};

PluginMetrics::region_t africa = {
    .name = "africa",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {56.42578125, 11.350796722383672},
        {31.289062500000004, 34.161818161230386},
        {6.15234375, 39.095962936305476},
        {-22.5, 34.30714385628804},
        {-16.5234375, -0.7031073524364783},
        {3.1640625, 3.162455530237848},
        {17.75390625, -37.71859032558814},
        {51.328125, -34.59704151614416},
        {56.42578125, 11.350796722383672}
     },
    .points = {},
    .num_points = 9
};

PluginMetrics::region_t middle_east = {
    .name = "middle_east",
    .proj = MathLib::PLATE_CARREE,
     .coords = {
       {24.08203125, 39.50404070558415},
        {45.17578125, 7.013667927566642},
        {83.84765625, 35.746512259918504},
        {55.1953125, 50.84757295365389},
        {24.08203125, 39.50404070558415}
     },
    .points = {},
    .num_points = 5
};

PluginMetrics::region_t europe = {
    .name = "europe",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {-10.546875, 35.17380831799959},
        {44.29687499999999, 36.59788913307022},
        {49.5703125, 64.32087157990324},
        {26.71875, 73.02259157147301},
        {-11.6015625, 57.70414723434193},
        {-10.546875, 35.17380831799959}
    },
    .points = {},
    .num_points = 6
};

PluginMetrics::region_t north_asia = {
    .name = "north_asia",
    .proj = MathLib::NORTH_POLAR,
    .coords = {
        {37.6171875, 43.58039085560784},
        {135.0, 41.244772343082076},
        {193.359375, 63.39152174400882},
        {184.5703125, 72.39570570653261},
        {99.140625, 81.72318761821155},
        {50.9765625, 82.35580019800932},
        {4.5703125, 80.05804956215623},
        {37.6171875, 43.58039085560784}
     },
    .points = {},
    .num_points = 8
};

PluginMetrics::region_t south_asia = {
    .name = "south_asia",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {150.46875, 46.07323062540835},
        {83.671875, 50.736455137010665},
        {51.67968749999999, 27.994401411046148},
        {104.4140625, -23.24134610238612},
        {138.515625, -1.7575368113083125},
        {150.46875, 46.07323062540835}
    },
    .points = {},
    .num_points = 6
};

PluginMetrics::region_t oceania = {
    .name = "oceania",
    .proj = MathLib::PLATE_CARREE,
    .coords = {
        {132.1875, 3.8642546157214084},
        {108.80859375, -23.563987128451217},
        {111.26953125, -39.232253141714885},
        {174.0234375, -49.83798245308484},
        {183.69140625, -40.3130432088809},
        {182.4609375, -7.01366792756663},
        {132.1875, 3.8642546157214084}
    },
    .points = {},
    .num_points = 7
};

PluginMetrics::region_t antarctica = {
    .name = "antarctica",
    .proj = MathLib::SOUTH_POLAR,
    .coords = {
        {-60.0, 1800.0},
        {-60.0, -180.0},
        {-60.0, 180.0},
     },
    .points = {},
    .num_points = 3
};

PluginMetrics::region_t unknown_region = {
    .name = "unknown_region",
    .proj = MathLib::PLATE_CARREE,
    .coords = {},
    .points = {},
    .num_points = 0
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * region2str
 *----------------------------------------------------------------------------*/
bool PluginMetrics::init (void)
{
    bool status = true;

    /* Loop Through All Regions */
    for(int r = 0; r < NUM_REGIONS; r++)
    {
        region_t* region = region2struct((regions_t)r);

        /* Build Polygon */
        for(int p = 0; p < region->num_points; p++)
        {
            region->points[p] = MathLib::coord2point(region->coords[p], region->proj);
        }

        /* Register Metric */
        regionMetricIds[r] = EventLib::registerMetric(CATEGORY, EventLib::COUNTER, "%s.%s", region->name, REGION_METRIC);
        if(regionMetricIds[r] == EventLib::INVALID_METRIC)
        {
            mlog(ERROR, "Registry failed for %s.%s", region->name, REGION_METRIC);
            status = false;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * region2str
 *----------------------------------------------------------------------------*/
PluginMetrics::region_t* PluginMetrics::region2struct (regions_t region)
{
    switch(region)
    {
        case REGION_CONTINENTAL_US:     return &continental_us;
        case REGION_ALASKA:             return &alaska;
        case REGION_CANADA:             return &canada;
        case REGION_GREENLAND:          return &greenland;
        case REGION_CENTRAL_AMERICA:    return &central_america;
        case REGION_SOUTH_AMERICA:      return &south_america;
        case REGION_AFRICA:             return &africa;
        case REGION_MIDDLE_EAST:        return &middle_east;
        case REGION_EUROPE:             return &europe;
        case REGION_NORTH_ASIA:         return &north_asia;
        case REGION_SOUTH_ASIA:         return &south_asia;
        case REGION_OCEANIA:            return &oceania;
        case REGION_ANTARCTICA:         return &antarctica;
        case REGION_UNKNOWN:            return &unknown_region;
        default:                        return &unknown_region;
    }
}

/*----------------------------------------------------------------------------
 * setRegion
 *----------------------------------------------------------------------------*/
bool PluginMetrics::setRegion (Icesat2Parms* parms)
{
    if(parms->polygon.length() > 0)
    {
        MathLib::coord_t coord = parms->polygon[0];
        regions_t region_found = REGION_UNKNOWN;

        if(coord.lat > -60)
        {
            /* Check Non-Antartica Regions */
            for(int r = 0; r < REGION_ANTARCTICA; r++)
            {
                if(checkRegion(coord, (regions_t)r))
                {
                    region_found = (regions_t)r;
                    break;
                }
            }
        }
        else
        {
            /* Set Region to Antartica */
            region_found = REGION_ANTARCTICA;
        }

        /* Increment Metric for Region */
        EventLib::incrementMetric(regionMetricIds[region_found]);
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * checkRegion
 *----------------------------------------------------------------------------*/
bool PluginMetrics::checkRegion (MathLib::coord_t coord, regions_t r)
{
    region_t* region = region2struct(r);
    MathLib::point_t point = MathLib::coord2point(coord, region->proj);
    return MathLib::inpoly(region->points, region->num_points, point);
}
