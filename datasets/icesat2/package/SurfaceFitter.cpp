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
 * INCLUDES
 ******************************************************************************/

#include <math.h>
#include <float.h>

#include "OsApi.h"
#include "GeoLib.h"
#include "SurfaceFitter.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

const double SurfaceFitter::SPEED_OF_LIGHT = 299792458.0; // meters per second
const double SurfaceFitter::PULSE_REPITITION_FREQUENCY = 10000.0; // 10Khz
const double SurfaceFitter::RDE_SCALE_FACTOR = 1.3490;
const double SurfaceFitter::SIGMA_BEAM = 4.25; // meters
const double SurfaceFitter::SIGMA_XMIT = 0.00000000068; // seconds

const char* SurfaceFitter::LUA_META_NAME = "SurfaceFitter";
const struct luaL_Reg SurfaceFitter::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int SurfaceFitter::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 1, Icesat2Fields::OBJECT_TYPE));
        return createLuaObject(L, new SurfaceFitter(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SurfaceFitter::SurfaceFitter (lua_State* L, Icesat2Fields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SurfaceFitter::~SurfaceFitter (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool SurfaceFitter::run (GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();

    Atl03DataFrame& df = *dynamic_cast<Atl03DataFrame*>(dataframe);

    // check df not empty
    if(df.length() <= 0) return true;

    // create new dataframe columns
    FieldColumn<int64_t>*    time_ns        = new FieldColumn<int64_t>(Field::TIME_COLUMN); // nanoseconds from GPS epoch
    FieldColumn<double>*     latitude       = new FieldColumn<double>(Field::Y_COLUMN);     // EPSG:7912
    FieldColumn<double>*     longitude      = new FieldColumn<double>(Field::X_COLUMN);     // EPSG:7912
    FieldColumn<double>*     h_mean         = new FieldColumn<double>(Field::Z_COLUMN);     // meters from ellipsoid
    FieldColumn<double>*     x_atc          = new FieldColumn<double>;                      // distance from the equator
    FieldColumn<float>*      y_atc          = new FieldColumn<float>();                     // distance from reference track
    FieldColumn<float>*      dh_fit_dx      = new FieldColumn<float>();                     // along track slope
    FieldColumn<float>*      window_height  = new FieldColumn<float>();
    FieldColumn<float>*      rms_misfit     = new FieldColumn<float>();
    FieldColumn<float>*      h_sigma        = new FieldColumn<float>();
    FieldColumn<uint32_t>*   photon_start   = new FieldColumn<uint32_t>();                  // photon index of start of extent
    FieldColumn<int32_t>*    photon_count   = new FieldColumn<int32_t>();                   // number of photons used in final elevation calculation
    FieldColumn<uint16_t>*   pflags         = new FieldColumn<uint16_t>();                  // processing flags

    // for each photon
    int32_t i0 = 0; // start row
    while(i0 < df.length())
    {
        uint32_t _pflags = 0;

        // find end of extent
        int32_t i1 = i0; // end row
        while( (i1 < df.length()) &&
               ((df.x_atc[i1] - df.x_atc[i0]) < parms->extentLength.value) )
        {
            i1++;
        }

        // check for end of dataframe
        if(i1 == df.length()) i1--;

        // check for valid extent
        if (i1 < i0)
        {
            mlog(CRITICAL, "Invalid extent (%d, %d)\n", i0, i1);
            break;
        }

        // calculate number of photons in extent
        const int32_t num_photons = i1 - i0 + 1;

        // check minimum extent length
        if((df.x_atc[i1] - df.x_atc[i0]) < parms->minAlongTrackSpread)
        {
            _pflags |= PFLAG_SPREAD_TOO_SHORT;
        }

        // check minimum number of photons
        if(num_photons < parms->minPhotonCount)
        {
            _pflags |= PFLAG_TOO_FEW_PHOTONS;
        }

        // run least squares fit
        if(_pflags == 0 || parms->passInvalid)
        {
            const result_t result = iterativeFitStage(df, i0, num_photons);
            time_ns->append(static_cast<int64_t>(result.time_ns));
            latitude->append(result.latitude);
            longitude->append(result.longitude);
            h_mean->append(result.h_mean);
            x_atc->append(result.x_atc);
            y_atc->append(result.y_atc);
            dh_fit_dx->append(result.dh_fit_dx);
            window_height->append(result.window_height);
            rms_misfit->append(result.rms_misfit);
            h_sigma->append(result.h_sigma);
            photon_start->append(df.ph_index[i0]);
            photon_count->append(i1 - i0);
            pflags->append(result.pflags | _pflags);
        }

        // find start of next extent
        const int32_t prev_i0 = i0;
        while( (i0 < df.length()) &&
               ((df.x_atc[i0] - df.x_atc[prev_i0]) < parms->extentStep.value) )
        {
            i0++;
        }

        // check extent moved
        if(i0 == prev_i0)
        {
            mlog(CRITICAL, "Failed to move to next extent in track");
            break;
        }
    }

    // clear all columns from original dataframe
    df.clear(); // frees memory

    // install new columns into dataframe
    df.addExistingColumn("time_ns",         time_ns);
    df.addExistingColumn("latitude",        latitude);
    df.addExistingColumn("longitude",       longitude);
    df.addExistingColumn("h_mean",          h_mean);
    df.addExistingColumn("x_atc",           x_atc);
    df.addExistingColumn("y_atc",           y_atc);
    df.addExistingColumn("dh_fit_dx",       dh_fit_dx);
    df.addExistingColumn("window_height",   window_height);
    df.addExistingColumn("rms_misfit",      rms_misfit);
    df.addExistingColumn("h_sigma",         h_sigma);
    df.addExistingColumn("photon_start",    photon_start);
    df.addExistingColumn("photon_count",    photon_count);
    df.addExistingColumn("pflags",          pflags);

    // update runtime and return success
    updateRunTime(TimeLib::latchtime() - start);
    return true;
}

/*----------------------------------------------------------------------------
 * iterativeFitStage
 *
 *  Note: Section 5.5 - Signal selection based on ATL03 flags
 *        Procedures 4b and after
 *----------------------------------------------------------------------------*/
SurfaceFitter::result_t SurfaceFitter::iterativeFitStage (const Atl03DataFrame& df, int32_t start_photon, int32_t num_photons)
{
    assert(num_photons > 0);

    /* Initialize Results */
    result_t result;

    /* Initial Per Track Calculations */
    const double pulses_in_extent = (parms->extentLength * PULSE_REPITITION_FREQUENCY) / df.spacecraft_velocity[start_photon]; // N_seg_pulses, section 5.4, procedure 1d
    const double background_density = pulses_in_extent * df.background_rate[start_photon] / (SPEED_OF_LIGHT / 2.0); // BG_density, section 5.7, procedure 1c

    /* Generate Along-Track Coordinate */
    const int32_t i_center = start_photon + (num_photons / 2);
    result.x_atc = df.x_atc[i_center];

    /* Initialize Photons Variables */
    point_t* photons = new point_t[num_photons];
    int32_t photons_in_window = num_photons;
    for(int32_t i = 0; i < num_photons; i++)
    {
        photons[i].p = start_photon + i;
        photons[i].r = 0;
        photons[i].x = df.x_atc[start_photon + i] - result.x_atc;
    }

    /* Iterate Processing of Photons */
    bool done = false;
    int iteration = 0;
    while(!done)
    {
        /* Calculate Least Squares Fit */
        leastSquaresFit(df, photons, photons_in_window, false, result);

        /* Sort Points by Residuals */
        quicksort(photons, 0, photons_in_window-1);

        /* Calculate Inputs to Robust Dispersion Estimate */
        double  background_count;       // N_BG
        double  window_lower_bound;     // zmin
        double  window_upper_bound;     // zmax;
        if(iteration == 0)
        {
            window_lower_bound  = photons[0].r; // section 5.5, procedure 4c
            window_upper_bound  = photons[photons_in_window - 1].r; // section 5.5, procedure 4c
            background_count    = background_density * (window_upper_bound - window_lower_bound); // section 5.5, procedure 4b; pe_select_mod.f90 initial_select()
        }
        else
        {
            background_count    = background_density * result.window_height; // section 5.7, procedure 2c
            window_lower_bound  = -(result.window_height / 2.0); // section 5.7, procedure 2c
            window_upper_bound  = result.window_height / 2.0; // section 5.7, procedure 2c
        }

        /* Continued Inputs to Robust Dispersion Estimate */
        const double background_rate  = background_count / (window_upper_bound - window_lower_bound); // bckgrd, section 5.9, procedure 1a
        const double signal_count     = photons_in_window - background_count; // N_sig, section 5.9, procedure 1b
        double sigma_r                = 0.0; // sigma_r

        /* Calculate Robust Dispersion Estimate */
        if(signal_count <= 1)
        {
            sigma_r = (window_upper_bound - window_lower_bound) / photons_in_window; // section 5.9, procedure 1c
        }
        else
        {
            /* Find Smallest Potential Percentiles (0) */
            int32_t i0 = 0;
            while(i0 < photons_in_window)
            {
                const double spp = (0.25 * signal_count) + ((photons[i0].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                if( (((double)i0) + 1.0 - 0.5 + 1.0) < spp )    i0++;   // +1 adjusts for 0 vs 1 based indices, -.5 rounds, +1 looks ahead
                else                                            break;
            }

            /* Find Smallest Potential Percentiles (1) */
            int32_t i1 = photons_in_window - 1;
            while(i1 >= 0)
            {
                const double spp = (0.75 * signal_count) + ((photons[i1].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                if( (((double)i1) + 1.0 - 0.5 - 1.0) > spp )    i1--;   // +1 adjusts for 0 vs 1 based indices, -.5 rounds, +1 looks ahead
                else                                            break;
            }

            /* Check Need to Refind Percentiles */
            if(i1 < i0)
            {
                /* Find Spread of Central Values (0) */
                const double spp0 = (photons_in_window / 2.0) - (signal_count / 4.0); // section 5.9, procedure 5a
                i0 = (int32_t)(spp0 + 0.5) - 1;

                /* Find Spread of Central Values (1) */
                const double spp1 = (photons_in_window / 2.0) + (signal_count / 4.0); // section 5.9, procedure 5b
                i1 = (int32_t)(spp1 + 0.5);
            }

            /* Check Validity of Percentiles */
            if(i0 >= 0 && i1 < photons_in_window)
            {
                /* Calculate Robust Dispersion Estimate */
                sigma_r = (photons[i1].r - photons[i0].r) / RDE_SCALE_FACTOR; // section 5.9, procedure 6
            }
            else
            {
                mlog(CRITICAL, "Out of bounds condition caught: %d, %d, %d", i0, i1, photons_in_window);
                result.pflags |= PFLAG_OUT_OF_BOUNDS;
            }
        }

        /* Calculate Sigma Expected */
        const double se1 = pow((SPEED_OF_LIGHT / 2.0) * SIGMA_XMIT, 2);
        const double se2 = pow(SIGMA_BEAM, 2) * pow(result.dh_fit_dx, 2);
        const double sigma_expected = sqrt(se1 + se2); // sigma_expected, section 5.5, procedure 4d

        /* Calculate Window Height */
        if(sigma_r > parms->fit.maxRobustDispersion.value) sigma_r = parms->fit.maxRobustDispersion.value;
        const double new_window_height = MAX(MAX(parms->fit.minWindow.value, 6.0 * sigma_expected), 6.0 * sigma_r); // H_win, section 5.5, procedure 4e
        result.window_height = MAX(new_window_height, 0.75 * result.window_height); // section 5.7, procedure 2e
        const double window_spread = result.window_height / 2.0;

        /* Precalculate Next Iteration's Conditions (section 5.7, procedure 2h) */
        int32_t next_num_photons = 0;
        double x_min = DBL_MAX;
        double x_max = -DBL_MAX;
        for(int p = 0; p < photons_in_window; p++)
        {
            if(abs(photons[p].r) < window_spread)
            {
                next_num_photons++;
                const double x = df.x_atc[photons[p].p];
                if(x < x_min) x_min = x;
                if(x > x_max) x_max = x;
            }
        }

        /* Check Photon Count */
        if(next_num_photons < parms->minPhotonCount.value)
        {
            result.pflags |= PFLAG_TOO_FEW_PHOTONS;
            done = true;
        }
        /* Check Spread */
        else if((x_max - x_min) < parms->minAlongTrackSpread.value)
        {
            result.pflags |= PFLAG_SPREAD_TOO_SHORT;
            done = true;
        }
        /* Check Change in Number of Photons */
        else if(next_num_photons == photons_in_window)
        {
            done = true;
        }
        /* Check Iterations */
        else if(++iteration >= parms->fit.maxIterations.value)
        {
            result.pflags |= PFLAG_MAX_ITERATIONS_REACHED;
            done = true;
        }
        /* Filtered Out Photons in Results and Iterate Again (section 5.5, procedure 4f) */
        else
        {
            int32_t ph_in = 0;
            for(int p = 0; p < photons_in_window; p++)
            {
                if(abs(photons[p].r) < window_spread)
                {
                    photons[ph_in++] = photons[p];
                }
            }
            photons_in_window = ph_in;
        }
    }

    /*
     *  Note: Section 3.6 - Signal, Noise, and Error Estimates
     *        Section 5.7, procedure 5
     */

    /* Sum Deltas in Photon Heights */
    double delta_sum = 0.0;
    for(int p = 0; p < photons_in_window; p++)
    {
        delta_sum += (photons[p].r * photons[p].r);
    }

    /* Calculate RMS and Scale h_sigma */
    if(photons_in_window > 0)
    {
        result.rms_misfit = sqrt(delta_sum / (double)photons_in_window);
        result.h_sigma = result.rms_misfit * result.h_sigma;
    }

    /* Calculate Latitude, Longitude, and GPS Time using Least Squares Fit */
    leastSquaresFit(df, photons, photons_in_window, true, result);

    /* Clean Up */
    delete [] photons;

    /* Return Results */
    return result;
}

/*----------------------------------------------------------------------------
 * lsf - least squares fit
 *
 *  Notes:
 *  1. The matrix element notation is row/column; so xxx_12 is the element of
 *     matrix xxx at row 1, column 2
 *  2. If there are multiple elements specified, then the value represents both
 *     elements; so xxx_12_21 the value in matrix xxx of the elements at row 1,
 *     column 2, and row 2, column 1
 *
 * Algorithm:
 *  xi          distance of the photon from the start of the segment
 *  h_mean      height at the center of the segment
 *  dh/dx       along track slope of the segment
 *  n           number of photons in the segment
 *
 *  G = [1, xi]                 # n x 2 matrix of along track photon distances
 *  m = [h_mean, dh/dx]         # 2 x 1 matrix representing the line of best fit
 *  z = [hi]                    # 1 x n matrix of along track photon heights
 *
 *  G^-g = (G^T * G)^-1 * G^T   # 2 x 2 matrix which is the generalized inverse of G
 *  m = G^-g * z                # 1 x 2 matrix containing solution
 *
 *  y_sigma = sqrt((G^-g * G^-gT)[0,0]) # square root of first element (row 0, column 0) of covariance matrix
 *
 *  TODO: currently no protections against divide-by-zero
 *----------------------------------------------------------------------------*/
void SurfaceFitter::leastSquaresFit (const Atl03DataFrame& df, point_t* array, int32_t size, bool final, result_t& result)
{
    /* Initialize Fit */
    double fit_height = 0.0;
    double fit_slope = 0.0;
    double fit_y_sigma = 0.0;

    /* Calculate G^T*G and GT*h*/
    const double gtg_11 = size;
    double       gtg_12_21 = 0.0;
    double       gtg_22 = 0.0;
    for(int p = 0; p < size; p++)
    {
        /* Perform Matrix Operation */
        const double x = array[p].x;
        gtg_12_21 += x;
        gtg_22 += x * x;
    }

    /* Calculate (G^T*G)^-1 */
    const double det = 1.0 / ((gtg_11 * gtg_22) - (gtg_12_21 * gtg_12_21));
    const double igtg_11 = gtg_22 * det;
    const double igtg_12_21 = -1 * gtg_12_21 * det;
    const double igtg_22 = gtg_11 * det;

    if(!final) /* Height */
    {
        /* Calculate G^-g and m */
        for(int p = 0; p < size; p++)
        {
            const int32_t i = array[p].p;
            const double x = array[p].x;
            const double y = df.height[i];

            /* Perform Matrix Operation */
            const double gig_1 = igtg_11 + (igtg_12_21 * x);   // G^-g row 1 element
            const double gig_2 = igtg_12_21 + (igtg_22 * x);   // G^-g row 2 element

            /* Calculate m */
            fit_height += gig_1 * y;
            fit_slope += gig_2 * y;

            /* Accumulate y_sigma */
            fit_y_sigma += gig_1 * gig_1;
        }

        /* Calculate y_sigma */
        fit_y_sigma = sqrt(fit_y_sigma);

        /* Populate Results */
        result.h_mean = fit_height;
        result.dh_fit_dx = fit_slope;
        result.h_sigma = fit_y_sigma; // scaled by rms below

        /* Calculate Residuals */
        for(int p = 0; p < size; p++)
        {
            const int32_t i = array[p].p;
            const double x = array[p].x;
            const double y = df.height[i];
            array[p].r = y - (fit_height + (x * fit_slope));
        }
    }
    else /* Latitude, Longitude, GPS Time, Across Track Coordinate, Ancillary Fields */
    {
        if(size > 0)
        {
            double latitude = 0.0;
            double longitude = 0.0;
            double time_ns = 0.0;
            double y_atc = 0.0;

            /* Check Need to Shift Longitudes
               assumes that there isn't a set of photons with
               longitudes that extend for more than 30 degrees */
            bool shift_lon = false;
            const double first_lon = df.longitude[array[0].p];
            if(first_lon < -150.0 || first_lon > 150.0)
            {
                shift_lon = true;
            }

            /* Fixed Fields - Calculate G^-g and m */
            for(int p = 0; p < size; p++)
            {
                const int32_t i = array[p].p;
                double ph_longitude = df.longitude[i];

                /* Shift Longitudes */
                if(shift_lon) ph_longitude = fmod((ph_longitude + 360.0), 360.0);

                /* Perform Matrix Operation */
                const double gig_1 = igtg_11 + (igtg_12_21 * array[p].x);   // G^-g row 1 element

                /* Calculate m */
                latitude += gig_1 * df.latitude[i];
                longitude += gig_1 * ph_longitude;
                time_ns += gig_1 * df.time_ns[i].nanoseconds;
                y_atc += gig_1 * df.y_atc[i];
            }

            /* Check if Longitude Needs to be Shifted Back */
            if(shift_lon) longitude = fmod((longitude + 180.0), 360.0) - 180.0;

            /* Populate Results */
            result.latitude = latitude;
            result.longitude = longitude;
            result.time_ns = (int64_t)time_ns;
            result.y_atc = (float)y_atc;
        }
    }
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
void SurfaceFitter::quicksort(point_t* array, int start, int end) // NOLINT(misc-no-recursion)
{
    if(start < end)
    {
        const int partition = quicksortpartition(array, start, end);
        quicksort(array, start, partition);
        quicksort(array, partition + 1, end);
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
int SurfaceFitter::quicksortpartition(point_t* array, int start, int end)
{
    const double pivot = array[(start + end) / 2].r;

    start--;
    end++;
    while(true)
    {
        while (array[++start].r < pivot);
        while (array[--end].r > pivot);
        if (start >= end) return end;

        const point_t tmp = array[start];
        array[start] = array[end];
        array[end] = tmp;
    }
}
