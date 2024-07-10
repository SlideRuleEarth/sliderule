
/*
 * Copyright (c) 2023, University of Texas
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
 * 3. Neither the name of the University of Texas nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __bathy_fields__
#define __bathy_fields__

#include "OsApi.h"

namespace BathyFields
{
    /* Photon Classifiers */
    typedef enum {
        INVALID_CLASSIFIER  = -1,
        QTREES              = 0,
        COASTNET            = 1,
        OPENOCEANS          = 2,
        MEDIANFILTER        = 3,
        CSHELPH             = 4,
        BATHY_PATHFINDER    = 5,
        POINTNET2           = 6,
        LOCAL_CONTRAST      = 7,
        ENSEMBLE            = 8,
        NUM_CLASSIFIERS     = 9
    } classifier_t;

    /* Photon Classifications */
    typedef enum {
        UNCLASSIFIED        = 0,
        OTHER               = 1,
        BATHYMETRY          = 40,
        SEA_SURFACE         = 41,
        WATER_COLUMN        = 45
    } bathy_class_t;

    /* Processing Flags */
    typedef enum {
        EXCEEDS_MAX_SENSOR_DEPTH = 0x01
    } flags_t;

    /* Photon Fields */
    typedef struct {
        int64_t         time_ns;                // nanoseconds since GPS epoch
        int32_t         index_ph;               // unique index of photon in granule
        int32_t         index_seg;              // index into segment level groups in source ATL03 granule
        double          latitude;
        double          longitude;
        double          x_ph;                   // the easting coordinate in meters of the photon for the given UTM zone
        double          y_ph;                   // the northing coordinate in meters of the photon for the given UTM zone
        double          x_atc;                  // along track distance calculated from segment_dist_x and dist_ph_along
        double          y_atc;                  // dist_ph_across
        double          background_rate;        // PE per second
        float           geoid;                  // geoid correction
        float           ortho_h;                // geoid corrected height of photon, calculated from h_ph and geoid
        float           dem_h;                  // best available dem height, geoid corrected
        float           sigma_h;                // height aerial uncertainty
        float           sigma_along;            // along track aerial uncertainty
        float           sigma_across;           // across track aerial uncertainty
        float           solar_elevation;
        float           sigma_thu;              // total horizontal uncertainty
        float           sigma_tvu;              // total vertical uncertainty
        float           ref_az;                 // reference azimuth
        float           ref_el;                 // reference elevation
        float           wind_v;                 // the wind speed at the center photon of the subsetted granule; calculated from met_u10m and met_v10m
        float           pointing_angle;         // angle of beam as measured from nadir
        float           ndwi;                   // normalized difference water index using HLS data
        uint32_t        processing_flags;
        uint8_t         yapc_score;
        int8_t          max_signal_conf;        // maximum value in the atl03 confidence table
        int8_t          quality_ph;
        int8_t          class_ph;               // photon classification        
    } photon_t;

    /* Extent Record */
    typedef struct {
        uint8_t         region;
        uint8_t         track;                  // 1, 2, or 3
        uint8_t         pair;                   // 0 (l), 1 (r)
        uint8_t         spot;                   // 1, 2, 3, 4, 5, 6
        uint16_t        reference_ground_track;
        uint8_t         cycle;
        uint8_t         utm_zone;
        uint64_t        extent_id;
        float           surface_h;              // orthometric (in meters)            
        uint32_t        photon_count;
        photon_t        photons[];              // zero length field
    } extent_t;
}

#endif
