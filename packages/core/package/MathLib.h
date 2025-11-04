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

#ifndef __math_lib__
#define __math_lib__

#include "OsApi.h"

class MathLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAXFREQSPEC = 8192;
        static const int LOG2DATASIZE = 13;
        static const double EARTHRADIUS;
        static const char* B64CHARS;
        static const int B64INDEX[256];

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
            PLATE_CARREE,
            AUTOMATIC_PROJECTION
        } proj_t;

        /* Geospatial Coordinate */
        typedef struct {
            double  lon;
            double  lat;
        } coord_t;

        /* Geospatial Datum */
        typedef enum {
            UNSPECIFIED_DATUM = 0,
            ITRF2014 = 1,
            ITRF2020 = 2,
            EGM08 = 3,
            NAVD88 = 4,
        } datum_t;

        /* Cartesian Coordinate */
        typedef struct {
            double  x;
            double  y;
        } point_t;

        /* 3d Point */
        typedef struct {
            double  x;
            double  y;
            double  z;
        } point_3d_t;

        typedef struct {
            point_t ll;
            point_t ur;
        } extent_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static point_t      coord2point         (coord_t c, proj_t projection);
        static coord_t      point2coord         (point_t p, proj_t projection);
        static bool         inpoly              (point_t* poly, int len, point_t point);

        static std::string  b64encode           (const void* data, const size_t &len);
        static std::string  b64decode           (const void* data, const size_t &len);
        static const char*  proj2str            (proj_t projection);

        static void         quicksort           (double* array, long start, long end); // NOLINT(misc-no-recursion)
        static long         quicksortpartition  (double* array, long start, long end);

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
