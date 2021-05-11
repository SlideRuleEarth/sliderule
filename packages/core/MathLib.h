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
        static point_t  coord2point (const coord_t c, proj_t projection);
        static coord_t  point2coord (const point_t p, proj_t projection);
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
