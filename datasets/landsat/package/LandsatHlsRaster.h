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

#ifndef __landsat_hls_raster__
#define __landsat_hls_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoIndexedRaster.h"

/******************************************************************************
 * LANDSAT RASTER CLASS
 ******************************************************************************/

class LandsatHlsRaster: public GeoIndexedRaster
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/
        static const char* L8_bands[];     /* Landsat 8  */
        static const char* S2_bands[];     /* Sentinel 2 */
        static const char* ALGO_names[];   /* Algorithms names */
        static const char* ALGO_bands[];   /* Algorithms bands */
        static const char* URL_str;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            LANDSAT8  = 0,
            SENTINEL2 = 1,
            ALGOBAND  = 2,
            ALGONAME  = 3
        } band_type_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static RasterObject* create(lua_State* L, RequestFields* rqst_parms, const char* key)
                          { return new LandsatHlsRaster(L, rqst_parms, key); }

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            BATCH,
            SERIAL,
        } sample_mode_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                 LandsatHlsRaster    (lua_State* L, RequestFields* rqst_parms, const char* key);
                ~LandsatHlsRaster    (void) override;

        void     getIndexFile        (const OGRGeometry* geo, std::string& file) final;
        void     getIndexFile        (const std::vector<point_info_t>* points, std::string& file) final;
        bool     findRasters         (raster_finder_t* finder) final;

        void     getSerialGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>& slist, uint32_t flags) final
                                     { _getGroupSamples(SERIAL, rgroup, &slist, flags);}

        uint32_t getBatchGroupSamples(const rasters_group_t* rgroup, List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx) final
                                     { return _getGroupSamples(BATCH, rgroup, slist, flags, pointIndx);}

        void     getInnerBands       (std::vector<std::string>& bands) final
                                     { bands.clear(); } /* Landsat bands are in seperate rasters */

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool        validateBandNames (void);
        static bool validateBand (band_type_t type, const char* bandName);

        static bool validL8Band   (const char* bandName)  {return validateBand(LANDSAT8, bandName);}
        static bool validS2Band   (const char* bandName)  {return validateBand(SENTINEL2,bandName);}
        static bool validAlgoBand (const char* bandName)  {return validateBand(ALGOBAND, bandName);}
        static bool validAlgoName (const char* bandName)  {return validateBand(ALGONAME, bandName);}

        uint32_t _getGroupSamples(sample_mode_t mode, const rasters_group_t* rgroup,
                                  List<RasterSample*>* slist, uint32_t flags, uint32_t pointIndx=0);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        std::string filePath;
        std::string indexFile;
        std::unordered_map<std::string, bool> bandsDict; /* Bands (rasters) to sample */

        bool ndsi;
        bool ndvi;
        bool ndwi;

};

#endif  /* __landsat_hls_raster__ */
