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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "MathLib.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double MathLib::SPEED_OF_LIGHT = 299792458.0; // meters per second

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void MathLib::init(void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void MathLib::deinit(void)
{
}

/*----------------------------------------------------------------------------
 * sum - double
 *----------------------------------------------------------------------------*/
double MathLib::sum(double* array, int size)
{
    assert(array);

    double total = 0.0;
    for(int i = 0; i < size; i++)
    {
        total += array[i];
    }
    return total;
}

/*----------------------------------------------------------------------------
 * lsf - least squares fit
 *
 *  TODO: currently no protections against divide-by-zero
 *----------------------------------------------------------------------------*/
MathLib::lsf_t MathLib::lsf (point_t* array, int size)
{
    lsf_t fit;

    /* Calculate GT*G and GT*h*/
    double gtg_11 = size;
    double gtg_12_21 = 0.0;
    double gtg_22 = 0.0;
    double gth_1 = 0.0;
    double gth_2 = 0.0;
    for(int p = 0; p < size; p++)
    {
        gtg_12_21 += array[p].x;
        gtg_22 += array[p].x * array[p].x;
        gth_1 += array[p].y;
        gth_2 += array[p].x * array[p].y;
    }

    /* Calculate Inverse of GT*G */
    double det = 1.0 / ((gtg_11 * gtg_22) - (gtg_12_21 * gtg_12_21));
    double igtg_11 = gtg_22 * det;
    double igtg_12_21 = -1 * gtg_12_21 * det;
    double igtg_22 = gtg_11 * det;

    /* Calculate IGTG * GTh */
    fit.intercept = (igtg_11 * gth_1) + (igtg_12_21 * gth_2);
    fit.slope = (igtg_12_21 * gth_1) + (igtg_22 * gth_2);

    /* Return Fit */
    return fit;
}

/*----------------------------------------------------------------------------
 * residuals
 *----------------------------------------------------------------------------*/
void MathLib::residuals (lsf_t fit, point_t* array, int size, double* residuals)
{
    /* Calculate Residuals */
    for(int p = 0; p < size; p++)
    {
        residuals[p] = array[p].y - (fit.intercept + (array[p].x * fit.slope));
    }
}

/*----------------------------------------------------------------------------
 * sort
 *----------------------------------------------------------------------------*/
void MathLib::sort (double* array, int size, int* indices)
{
    quicksort(array, 0, size-1);
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
void MathLib::quicksort(double* array, int start, int end, int* indices)
{
    if(start < end)
    {
        int partition = quicksortpartition(array, start, end, indices);
        quicksort(array, start, partition, indices);
        quicksort(array, partition + 1, end, indices);
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
int MathLib::quicksortpartition(double* array, int start, int end, int* indices)
{
    int pivot = array[(start + end) / 2];

    start--;
    end++;
    while(true)
    {
        while (array[++start] < pivot);
        while (array[--end] > pivot);
        if (start >= end) return end;

        double tmp1 = array[start];
        array[start] = array[end];
        array[end] = tmp1;

        if(indices)
        {
            int tmp2 = indices[start];
            indices[start] = indices[end];
            indices[end] = tmp2;
        }
    }
}
