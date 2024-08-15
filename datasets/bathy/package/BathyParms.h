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

#ifndef __bathy_parms__
#define __bathy_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * REQUEST PARAMETERS
 ******************************************************************************/

class BathyParms: public Icesat2Parms
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* phRecType;
        static const RecordObject::fieldDef_t phRecDef[];
        static const char* exRecType;
        static const RecordObject::fieldDef_t exRecDef[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Photon Classifiers */
        typedef enum {
            INVALID_CLASSIFIER  = -1,
            QTREES              = 0,
            COASTNET            = 1,
            OPENOCEANSPP        = 2,
            MEDIANFILTER        = 3,
            CSHELPH             = 4,
            BATHYPATHFINDER     = 5,
            POINTNET            = 6,
            OPENOCEANS          = 7,
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
            SENSOR_DEPTH_EXCEEDED   = 0x01,
            SEA_SURFACE_UNDETECTED  = 0x02
        } flags_t;

        /* Photon Fields */
        typedef struct {
            int64_t         time_ns;                // nanoseconds since GPS epoch
            int32_t         index_ph;               // unique index of photon in granule
            int32_t         index_seg;              // index into segment level groups in source ATL03 granule
            double          lat_ph;                 // latitude of photon (EPSG 7912)
            double          lon_ph;                 // longitude of photon (EPSG 7912)
            double          x_ph;                   // the easting coordinate in meters of the photon for the given UTM zone
            double          y_ph;                   // the northing coordinate in meters of the photon for the given UTM zone
            double          x_atc;                  // along track distance calculated from segment_dist_x and dist_ph_along
            double          y_atc;                  // dist_ph_across
            double          background_rate;        // PE per second
            float           delta_h;                // refraction correction of height
            float           surface_h;              // orthometric height of sea surface at each photon location
            float           ortho_h;                // geoid corrected height of photon, calculated from h_ph and geoid
            float           ellipse_h;              // height of photon with respect to reference ellipsoid
            float           sigma_thu;              // total horizontal uncertainty
            float           sigma_tvu;              // total vertical uncertainty
            uint32_t        processing_flags;       // bit mask of flags for capturing errors and warnings
            uint8_t         yapc_score;             // atl03 density estimate (Yet Another Photon Classifier)
            int8_t          max_signal_conf;        // maximum value in the atl03 confidence table
            int8_t          quality_ph;             // atl03 quality flags
            int8_t          class_ph;               // photon classification
            int8_t          predictions[NUM_CLASSIFIERS];
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
            float           wind_v;                 // wind speed (in meters/second)            
            float           ndwi;                   // normalized difference water index using HLS data
            uint32_t        photon_count;
            photon_t        photons[];              // zero length field
        } extent_t;

        /* Bathymetry Surface Finder */
        struct surface_t {
            double          bin_size;           // meters
            double          max_range;          // meters
            long            max_bins;           // bins
            double          signal_threshold;   // standard deviations
            double          min_peak_separation;// meters
            double          highest_peak_ratio;
            double          surface_width;      // standard deviations
            bool            model_as_poisson;
            
            surface_t():
                bin_size                (0.5),
                max_range               (1000.0),
                max_bins                (10000),
                signal_threshold        (3.0),
                min_peak_separation     (0.5),
                highest_peak_ratio      (1.2),
                surface_width           (3.0),
                model_as_poisson        (true) {};
            ~surface_t() {
            };

            void        fromLua (lua_State* L, int index);
            const char* toJson  (void) const;
        };

        /* Refraction Correction */
        struct refraction_t {
            bool            use_water_ri_mask;      // global water refractive index mask downloaded in atl24 init lua routine
            double          ri_air;                 // refraction index of air
            double          ri_water;               // refraction index of water

            refraction_t():
                use_water_ri_mask   (true),
                ri_air              (1.00029),
                ri_water            (1.34116) {};

            ~refraction_t() = default;

            void        fromLua (lua_State* L, int index);
            const char* toJson  (void) const;
        };

        /* Uncertainty Calculation */
        struct uncertainty_t {
            Asset*          assetKd;                // asset for reading Kd resources
            const char*     resourceKd;             // filename for Kd (uncertainty calculation)

            uncertainty_t():
                assetKd             (NULL),
                resourceKd          (NULL) {};

            ~uncertainty_t() {
                if(assetKd) assetKd->releaseLuaObject();
                delete [] resourceKd;
            };

            void        fromLua (lua_State* L, int index);
            const char* toJson  (void) const;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate       (lua_State* L);
        static void         init            (void);
        static classifier_t str2classifier  (const char* str);
        static const char*  classifier2str  (classifier_t classifier);
        const char*         tojson          (void) const override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Asset*          asset;              // asset for ATL03 resources
        Asset*          asset09;            // asset for ATL09 resources
        const char*     resource;           // ATL03 granule
        const char*     resource09;         // ATL09 granule
        double          max_dem_delta;      // initial filter of heights against DEM (For removing things like clouds)
        double          min_dem_delta;      // initial filter of heights against DEM (For removing things like clouds)
        int             ph_in_extent;       // number of photons in each extent
        bool            generate_ndwi;      // use HLS data to generate NDWI for each segment lat,lon
        bool            use_bathy_mask;     // global bathymetry mask downloaded in atl24 init lua routine
        bool            classifiers[NUM_CLASSIFIERS]; // which bathymetry classifiers to run
        bool            return_inputs;      // return the atl03 bathy records back to client
        bool            spots[NUM_SPOTS];   // only used by downstream algorithms
        surface_t       surface;
        refraction_t    refraction;
        uncertainty_t   uncertainty;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        BathyParms      (lua_State* L, int index);
                        ~BathyParms     (void) override;
        static void     getSpotList     (lua_State* L, int index, bool* provided, bool* spots, int size=NUM_SPOTS);
        static void     getClassifiers  (lua_State* L, int index, bool* provided, bool* classifiers, int size=NUM_CLASSIFIERS);
};

#endif  /* __bathy_parms__ */
