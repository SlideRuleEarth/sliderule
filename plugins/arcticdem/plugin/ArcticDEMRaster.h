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

#ifndef __arcticdem_raster__
#define __arcticdem_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "OsApi.h"
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <vrtdataset.h>

/******************************************************************************
 * GEOJSON RASTER CLASS
 ******************************************************************************/

class ArcticDEMRaster: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/
        static const int   ARCTIC_DEM_INVALID_ELELVATION = -1000000;
        static const int   RASTER_BLOCK_SIZE = 25000;
        static const int   RASTER_PHOTON_CRS = 4326;
        static const int   RASTER_ARCTIC_DEM_CRS = 3413;

        static const int   MAX_READER_THREADS = 100;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            double lon_min;
            double lat_min;
            double lon_max;
            double lat_max;
        } bbox_t;


        typedef struct {
            bool             isValid;
            ArcticDEMRaster* obj;
            GDALDataset*     dset;
            GDALRasterBand*  band;
            std::string      fileName;

            uint32_t        rows;
            uint32_t        cols;
            bbox_t          bbox;
            double          cellSize;
            int32_t         xBlockSize;
            int32_t         yBlockSize;

            /* Last sample information */
            OGRPoint*       point;
            double          value;
            double          readTime;
        } raster_t;


        typedef struct {
            pthread_t  handle;
            bool       isRunning;
        } thread_t;


        typedef enum {
            MOSAIC  = 0,
            STRIPS  = 1,
            INVALID = 3
        } dem_type_t;


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void             init           (void);
        static void             deinit         (void);
        static int              luaCreate      (lua_State* L);
        static ArcticDEMRaster* create         (lua_State* L, int index);

        void                    samples        (double lon, double lat);
        virtual ~ArcticDEMRaster(void);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        ArcticDEMRaster      (lua_State* L, const char* dem_type, const char* dem_sampling, const int sampling_radius);
        double sampleMosaic   (double lon, double lat);
        void   sampleStrips   (double lon, double lat);

    private:
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        std::string         vrtFileName;
        VRTDataset*         vrtDset;
        GDALRasterBand*     vrtBand;
        List<std::string>   tifList;
        List<raster_t>      rasterList;
        thread_t            rasterRreader[MAX_READER_THREADS];
        int                 threadCount;
        Mutex               threadMut;
        dem_type_t          demType;
        double              invgeot[6];

        uint32_t vrtRows;
        uint32_t vrtCols;
        double   vrtCellSize;
        bbox_t   vrtBbox;

        OGRCoordinateTransformation *transf;
        OGRSpatialReference srcSrs;
        OGRSpatialReference trgSrs;
        GDALRIOResampleAlg  sampleAlg;
        int32_t radius;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaDimensions(lua_State *L);
        static int luaBoundingBox(lua_State *L);
        static int luaCellSize(lua_State *L);
        static int luaSamples(lua_State *L);

        static bool  containsPoint  (GDALDataset *dset, bbox_t *bbox, OGRPoint *p);
        static void* readingThread  (void *param);

        bool  findRasters     (OGRPoint* p);
        bool  readRasters     (OGRPoint* p);
        bool  updateRasterList(OGRPoint* p);
        bool  readRaster      (raster_t* raster);
        bool  openVrtDset     (const char *fileName);
};

#endif  /* __arcticdem_raster__ */
