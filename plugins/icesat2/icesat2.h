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

#ifndef __icesat2__
#define __icesat2__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Hdf5File.h"

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/* Signal Confidence per Photon */
typedef enum {
    CONF_SURFACE_HIGH = 4,
    CONF_SURFACE_MEDIUM = 3,
    CONF_SURFACE_LOW = 2,
    CONF_WITHIN_10M = 1,
    CONF_BACKGROUND = 0
} signal_conf_ph_t;


//  FOR each 40m segment:
//      (1) Photon-Classification Stage {3.1}
//
//          IF (at least 10 photons) AND (at least 20m horizontal spread) THEN
//              a. select the set of photons from ATL03 (2x20m segments) based on signal_conf_ph_t threshold [sig_thresh]
//              b. fit sloping line segment to photons 
//              c. calculate robust spread of the residuals [sigma_r]
//              d. select the set of photons used to fit line AND that fall within max(+/- 1.5m, 3*sigma_r) of line
//          ELSE 
//              a. add 20m to beginning and end of segment to create 80m segment
//              b. histogram all photons into 10m vertical bins
//              c. select the set of photons in the maximum (Nmax) bin AND photons that fall in bins with a count that is Nmax - sqrt(Nmax)
//              d. select subset of photons above that are within the original 40m segment
//
//          FINALY identify height of photons selected by above steps [h_widnow]
//
//      (2) Photon-Selection-Refinement Stage {3.2}
//
//          WHILE iterations are less than 20 AND subset of photons changes each iteration
//              a. least-squares fit current set of photons: x = curr_photon - segment_center, y = photon_height 
//                  i.  calculate mean height [h_mean]
//                  ii. calculate slope [dh/dx]
//              b. calculate robust estimator (similar to standard deviation) of residuals
//                  i.  calculate the median height (i.e. middle of the window at given point) [r_med]
//                  ii. calculate background-corrected spread of distribution [r_o]; force r_o to be at most 5m
//                  iii.calculate expected spread of return photons [h_expected_rms]
//              c. select subset of photons that fall within new window
//                  i.  determine new window: h_window = MAX(6*r_o, 6*h_expected_rms, 0.75 * h_window_last, 3m)
//                  ii. select photon if distance from r_med falls within h_window/2
//
//      (3) Surface Height Quality Stage {3.2.1}
//
//          CALCULATE signal to noise significance


/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

extern "C" {
void initicesat2 (void);
}

#endif  /* __icesat2__ */


