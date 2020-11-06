/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __math_lib__
#define __math_lib__

#include "List.h"

class MathLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAXFREQSPEC = 8192;
        static const int LOG2DATASIZE = 13;
        static const double EARTHRADIUS;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Complex Number */
        typedef struct {
            double r;
            double i;
        } complex_t;

        /* Geospatial Projection */
        typedef enum {
            NORTH_POLAR,
            SOUTH_POLAR,
            PLATE_CARREE
        } proj_t;

        /* Geospatial Coordinate */
        typedef struct {
            double  lat;
            double  lon;
        } coord_t;

        /* Cartesian Coordinate */
        typedef struct {
            double  x;
            double  y;
        } point_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static double   FFT         (double result[], int data[], unsigned long size);
        static void     coord2point (const coord_t c, point_t& p, proj_t projection);
        static void     point2coord (coord_t& c, const point_t p, proj_t projection);
        static bool     inpoly      (List<point_t>& poly, point_t point);
        static bool     ingeopoly   (List<coord_t>& poly, coord_t coord, proj_t projection);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     swapComplex         (complex_t *a, complex_t *b);
        static void     bitReverse          (complex_t data[], unsigned long size);
        static void     freqCorrelation     (complex_t data[], unsigned long size, int isign);
        static double   getPolarMagnitude   (double ReX, double ImX);
        static double   getPolarPhase       (double ReX, double ImX);
};

#endif /* __math_lib__ */
