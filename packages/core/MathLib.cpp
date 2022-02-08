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

/*
 * TODO:    this module likely includes code snippets or algorithms
 *          taken from a third party source that I haven't yet been able
 *          to track down;  the sources need to be found and referenced.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#define _USE_MATH_DEFINES
#include <cmath>
#include <string>

#include "MathLib.h"
#include "LocalLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double MathLib::EARTHRADIUS = 6367.5;

const char* MathLib::B64CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const int MathLib::B64INDEX[256] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63,
    0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

/******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * FFT
 *----------------------------------------------------------------------------*/
double MathLib::FFT(double result[], int input[], unsigned long size)
{
	static complex_t frequency_spectrum[MAXFREQSPEC];
    double maxvalue = 0.0;

    /* Zero out Frequency Spectrum - Since Size not power of two */
    LocalLib::set(frequency_spectrum, 0, sizeof(frequency_spectrum));

	/* Load Data into Complex Array */
	for(unsigned long k = 0; k < size; k++)
	{
		frequency_spectrum[k].r = (double)input[k];
//		frequency_spectrum[k].i = 0.0;
	}

	/* Perform FFT */
	bitReverse(frequency_spectrum, size);
	freqCorrelation(frequency_spectrum, size, 1);

    /* Zero First Value - (Remove DC Component */
    result[0] = 0.0;
    result[size / 2] = 0.0;

	/* Populate Polar Form */
    for(unsigned long k = 1; k < size / 2; k++)
    {
        result[k] = getPolarMagnitude(frequency_spectrum[k].r, frequency_spectrum[k].i);
        result[k + (size / 2)] = getPolarPhase(frequency_spectrum[k].r, frequency_spectrum[k].i);

        if(result[k] > maxvalue) maxvalue = result[k];
        if(result[k + (size / 2)] > maxvalue) maxvalue = result[k + (size / 2)];
    }

    /* Return Maximum Value */
    return maxvalue;
}

/*----------------------------------------------------------------------------
 * coord2point
 *----------------------------------------------------------------------------*/
MathLib::point_t MathLib::coord2point (const coord_t c, proj_t projection)
{
    point_t p;

    /* Convert to Radians */
    double lonrad = c.lon * M_PI / 180.0;
    double latrad = c.lat * M_PI / 180.0;

    if(projection == NORTH_POLAR || projection == SOUTH_POLAR)
    {
        double r = 0.0, o = 0.0;

        /* Calculate r */
        if(projection == NORTH_POLAR)
        {
            double latradp = (M_PI / 4.0) - (latrad / 2.0);
            r = 2 * tan(latradp);
        }
        else if(projection == SOUTH_POLAR)
        {
            double latradp = -(M_PI / 4.0) - (latrad / 2.0);
            r = -2 * tan(latradp);
        }

        /* Calculate o */
        if(projection == NORTH_POLAR)
        {
            o = lonrad;
        }
        else if(projection == SOUTH_POLAR)
        {
            o = -lonrad;
        }

        /* Calculate Point */
        p.x = r * cos(o);
        p.y = r * sin(o);
    }
    else if(projection == PLATE_CARREE)
    {
        /* Calculate Point */
        p.x = EARTHRADIUS * lonrad;
        p.y = EARTHRADIUS * latrad;
    }

    return p;
}

/*----------------------------------------------------------------------------
 * point2coord
 *----------------------------------------------------------------------------*/
MathLib::coord_t MathLib::point2coord (const point_t p, proj_t projection)
{
    coord_t c;

    double lonrad = 0.0, latrad = 90.0;

    if(projection == NORTH_POLAR || projection == SOUTH_POLAR)
    {
        /* Calculate r */
        double r = sqrt((p.x*p.x) + (p.y*p.y));

        /* Calculate o */
        double o = 0.0;
        if(p.x != 0.0)
        {
            o = atan(p.y / p.x);

            /* Adjust for Quadrants */
            if(p.x < 0.0 && p.y >= 0.0) o += M_PI;
            else if(p.x < 0.0 && p.y < 0.0) o -= M_PI;
        }
        else
        {
            /* PI/2 or -PI/2 */
            o = asin(p.y / r);
        }

        /* Calculate Latitude */
        if(projection == NORTH_POLAR)
        {
            double latradp = atan(r / 2.0);
            latrad = (M_PI / 2.0) - (2.0 * latradp);
        }
        else if(projection == SOUTH_POLAR)
        {
            double latradp = atan(r / -2.0);
            latrad = (-2.0 * latradp) - (M_PI / 2.0);
        }

        /* Calculate Longitude */
        if(projection == NORTH_POLAR)
        {
            lonrad = o;
        }
        else if(projection == SOUTH_POLAR)
        {
            lonrad = -o;
        }
    }
    else if(projection == PLATE_CARREE)
    {
        /* Calculate Coordinates */
        lonrad = p.x / EARTHRADIUS;
        latrad = p.y / EARTHRADIUS;
    }

    /* Convert to Degress */
    c.lon = lonrad * (180.0 / M_PI);
    c.lat = latrad * (180.0 / M_PI);

    return c;
}

/*----------------------------------------------------------------------------
 * inpoly
 *
 * Algorithm based off of https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html;
 * the copyright notice associated with code provided on the website is reproduced
 * below:
 *
 *
 * Copyright (c) 1970-2003, Wm. Randolph Franklin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 *   Redistributions of source code must retain the above copyright notice, this list of
 *   conditions and the following disclaimers.
 *
 *   Redistributions in binary form must reproduce the above copyright notice in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   The name of W. Randolph Franklin may not be used to endorse or promote products derived
 *   from this Software without specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *----------------------------------------------------------------------------*/
bool MathLib::inpoly (point_t* poly, int len, point_t point)
{
    int c = 0;
    for (int i = 0, j = len - 1; i < len; j = i++)
    {
        double x_extent = (poly[j].x - poly[i].x) * (point.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x;
        if( ((poly[i].y > point.y) != (poly[j].y > point.y)) && (point.x < x_extent) ) c = !c;
    }

    /* Return Inclusion
     *  if c == 0: number of intersections were even --> point is outside polygon
     *  if c == 1: number of intersections were odd --> point is inside polygon
     */
    return c == 1;
}

/*----------------------------------------------------------------------------
 * b64encode
 *
 * Code sourced from https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c/13935718
 * Author: polfosol
 * License: assumed to be CC BY-SA 3.0
 *----------------------------------------------------------------------------*/
const std::string MathLib::b64encode(const void* data, const size_t &len)
{
    std::string result((len + 2) / 3 * 4, '=');
    unsigned char *p = (unsigned  char*) data;
    char *str = &result[0];
    size_t j = 0, pad = len % 3;
    const size_t last = len - pad;

    for (size_t i = 0; i < last; i += 3)
    {
        int n = int(p[i]) << 16 | int(p[i + 1]) << 8 | p[i + 2];
        str[j++] = B64CHARS[n >> 18];
        str[j++] = B64CHARS[n >> 12 & 0x3F];
        str[j++] = B64CHARS[n >> 6 & 0x3F];
        str[j++] = B64CHARS[n & 0x3F];
    }
    if (pad)  /// Set padding
    {
        int n = --pad ? int(p[last]) << 8 | p[last + 1] : p[last];
        str[j++] = B64CHARS[pad ? n >> 10 & 0x3F : n >> 2];
        str[j++] = B64CHARS[pad ? n >> 4 & 0x03F : n << 4 & 0x3F];
        str[j++] = pad ? B64CHARS[n << 2 & 0x3F] : '=';
    }
    return result;
}

/*----------------------------------------------------------------------------
 * b64encode
 *
 * Code sourced from https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c/13935718
 * Author: polfosol
 * License: assumed to be CC BY-SA 3.0
 *----------------------------------------------------------------------------*/
const std::string MathLib::b64decode(const void* data, const size_t &len)
{
    if (len == 0) return "";

    unsigned char *p = (unsigned char*) data;
    size_t j = 0,
        pad1 = len % 4 || p[len - 1] == '=',
        pad2 = pad1 && (len % 4 > 2 || p[len - 2] != '=');
    const size_t last = (len - pad1) / 4 << 2;
    std::string result(last / 4 * 3 + pad1 + pad2, '\0');
    unsigned char *str = (unsigned char*) &result[0];

    for (size_t i = 0; i < last; i += 4)
    {
        int n = B64INDEX[p[i]] << 18 | B64INDEX[p[i + 1]] << 12 | B64INDEX[p[i + 2]] << 6 | B64INDEX[p[i + 3]];
        str[j++] = n >> 16;
        str[j++] = n >> 8 & 0xFF;
        str[j++] = n & 0xFF;
    }
    if (pad1)
    {
        int n = B64INDEX[p[last]] << 18 | B64INDEX[p[last + 1]] << 12;
        str[j++] = n >> 16;
        if (pad2)
        {
            n |= B64INDEX[p[last + 2]] << 6;
            str[j++] = n >> 8 & 0xFF;
        }
    }
    return result;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * swapComplex
 *
 *  swaps two complex samples
 *----------------------------------------------------------------------------*/
void MathLib::swapComplex(complex_t *a, complex_t *b)
{
    complex_t tmp;

    tmp.r = a->r;
    tmp.i = a->i;

    a->r = b->r;
    a->i = b->i;

    b->r = tmp.r;
    b->i = tmp.i;
}

/*----------------------------------------------------------------------------
 * bitReverse
 *
 *  sorts data into bit reverse order
 *----------------------------------------------------------------------------*/
void MathLib::bitReverse(complex_t data[], unsigned long size)
{
    unsigned long steps[LOG2DATASIZE], s;
    unsigned long i,j,k;

    // Calculate Steps //
    steps[0] = size / 2;
    for(s = 1; s < LOG2DATASIZE; s++)
    {
        steps[s] = (unsigned long)(3 * size / pow(2, (s + 1)));
    }

    // Reorder Data //
    j = 0;
    for(i = 0; i < size; i++)
    {
        // Swap If Not Equal //
        if(i < j) // only swap if less than so that you don't swap back
        {
            swapComplex(&data[i], &data[j]);
        }

        // Calculate Step Size //
        s = 0;
        k = i;
        while(k % 2 != 0) // trying to find first zero in binary representation
        {
            k >>= 1;
            s++;
        }

        // Take Next Step //
        j = (j + steps[s]) % size;
    }
}

/*----------------------------------------------------------------------------
 * freqCorrelation
 *
 * data     real time domain signal
 * size     number of samples in data
 *
 * return   modifies the data array in place
 *----------------------------------------------------------------------------*/
void MathLib::freqCorrelation(complex_t data[], unsigned long size, int isign)
{
    unsigned long   halfperiod; // half period of frequency
    unsigned long   offset;     // offset within halfperiod
    unsigned long   i,j;        // sample indices
    double          theta;
    complex_t       temp;
    complex_t       w;
    complex_t       wp;

    for(halfperiod = 1; halfperiod < size; halfperiod *= 2)
    {
        theta = isign * (M_PI / halfperiod);

        wp.r = -2.0 * pow(sin(0.5 * theta),2);
        wp.i = sin(theta);

        w.r = 1.0;
        w.i = 0.0;

        for(offset = 0; offset < halfperiod; offset += 1)
        {
            for(i = offset; i < size; i += (halfperiod * 2))
            {
                j = i + halfperiod;

                temp.r = w.r * data[j].r - w.i * data[j].i;
                temp.i = w.r * data[j].i + w.i * data[j].r;

                data[j].r = data[i].r - temp.r;
                data[j].i = data[i].i - temp.i;

                data[i].r += temp.r;
                data[i].i += temp.i;
            }

            temp.r = w.r * wp.r - w.i * wp.i + w.r;
            temp.i = w.i * wp.r + w.r * wp.i + w.i;

            w.r = temp.r;
            w.i = temp.i;
        }
    }
}

/*----------------------------------------------------------------------------
 * Get Polar Magnitude
 *
 * ReX      real component of frequency domain
 * ImX      imaginary component of frequency domain
 *
 * return   magnitude of cosine component in polar form
 *----------------------------------------------------------------------------*/
double MathLib::getPolarMagnitude(double ReX, double ImX)
{
	return sqrt(pow(ReX,2) + pow(ImX,2));
}

/*----------------------------------------------------------------------------
 * Get Polar Phase
 *
 * ReX      real component of frequency domain
 * ImX      imaginary component of frequency domain
 *
 * return   phase of cosine component in polar form
 *----------------------------------------------------------------------------*/
double MathLib::getPolarPhase(double ReX, double ImX)
{
	double offset;

	/* Special Divide by Zero Case */
	if(ReX == 0.0)
	{
		ReX = 1e-20;
	}

	/* Arctan Range Correction */
	if(ReX < 0.0 && ImX < 0.0)
	{
		offset = -M_PI;
	}
	else if (ReX < 0.0 && ImX > 0.0)
	{
		offset = M_PI;
	}
	else
	{
		offset = 0.0;
	}

	/* Phase Calculation */
	return atan(ImX/ReX) + offset;
}
