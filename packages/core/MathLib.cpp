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

/*
 * TODO:    this module likely includes either code snippets or algorithms
 *          taken from a third party source;  the source is currently 
 *          unknown and needs to be found and referenced. 
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#define _USE_MATH_DEFINES
#include <cmath>

#include "MathLib.h"
#include "LocalLib.h"

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
 * geo2polar
 *----------------------------------------------------------------------------*/
void MathLib::geo2polar (const coord_t c, point_t& p, proj_t projection)
{
    double r = 0.0, o = 0.0;

    /* Convert to Radians */
    double lonrad = c.lon * M_PI / 180.0;
    double latrad = c.lat * M_PI / 180.0;

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

    /* Calculate X */
    p.x = r * cos(o);

    /* Calculate Y */
    p.y = r * sin(o);
}

/*----------------------------------------------------------------------------
 * polar2geo
 *----------------------------------------------------------------------------*/
void MathLib::polar2geo (coord_t& c, const point_t p, proj_t projection)
{
    double latrad = 90.0, lonrad = 0.0;

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

    /* Convert to Degress */
    c.lat = latrad * (180.0 / M_PI);
    c.lon = lonrad * (180.0 / M_PI);
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
