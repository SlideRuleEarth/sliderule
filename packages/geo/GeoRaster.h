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

#ifndef __geo_raster__
#define __geo_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RasterObject.h"
#include "OsApi.h"
#include "TimeLib.h"
#include "GeoParms.h"
#include "Ordering.h"
#include "List.h"
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>

/******************************************************************************
 * Typedef and macros used by geo package
 ******************************************************************************/

#define CHECKPTR(p)                                                           \
do                                                                            \
{                                                                             \
    if ((p) == NULL)                                                          \
    {                                                                         \
        throw RunTimeException(CRITICAL, RTE_ERROR,                           \
              "NULL pointer detected (%s():%d)", __FUNCTION__, __LINE__);     \
    }                                                                         \
} while (0)


#define CHECK_GDALERR(e)                                                      \
do                                                                            \
{                                                                             \
    if ((e))   /* CPLErr and OGRErr types have 0 for no error  */             \
    {                                                                         \
        throw RunTimeException(CRITICAL, RTE_ERROR,                           \
              "GDAL ERROR detected: %d (%s():%d)", e, __FUNCTION__, __LINE__);\
    }                                                                         \
} while (0)




/******************************************************************************
 * GEO RASTER CLASS
 ******************************************************************************/

class GeoRaster: public RasterObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int INVALID_SAMPLE_VALUE          = -1000000;
        static const int MAX_SAMPLING_RADIUS_IN_PIXELS = 50;
        static const int SLIDERULE_EPSG                = 7912;

        static const char* FLAGS_RASTER_TAG;
        static const char* SAMPLES_RASTER_TAG;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct
        {
            double lon_min;
            double lat_min;
            double lon_max;
            double lat_max;
        } bbox_t;

        class CoordTransform
        {
        public:
            OGRCoordinateTransformation *transf;
            OGRSpatialReference source;
            OGRSpatialReference target;

            void clear(bool close = true);
            CoordTransform(void) { clear(false); }
           ~CoordTransform(void) { clear(); }
        };

        class GeoDataSet
        {
        public:
            CoordTransform  cord;
            double          verticalShift;  /* Calculated for last POI transformed to target CRS */
            std::string     fileName;
            GDALDataset    *dset;
            GDALRasterBand* band;
            GDALDataType    dataType;
            bool            dataIsElevation;
            uint32_t        rows;
            uint32_t        cols;
            double          cellSize;
            bbox_t          bbox;
            int32_t         xBlockSize;
            int32_t         yBlockSize;
            uint32_t        radiusInPixels;

            void clear(bool close = true);

            GeoDataSet(void) { clear(false); }
           ~GeoDataSet(void) { clear(); }
        };

        class Raster: public GeoDataSet
        {
        public:
            bool            enabled;
            bool            sampled;
            std::string     groupId;

            double          gpsTime;
            double          useTime;

            /* Last sample information */
            OGRPoint        point;
            sample_t        sample;

            Raster(const char* _fileName, double _gpsTime=0);

            inline bool containsPoint(OGRPoint& p)
            {
                return (dset &&
                        (p.getX() >= bbox.lon_min) && (p.getX() <= bbox.lon_max) &&
                        (p.getY() >= bbox.lat_min) && (p.getY() <= bbox.lat_max));
            }

        };


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual         ~GeoRaster  (void);
        virtual void    getSamples  (double lon, double lat, double height, int64_t gps, List<sample_t>& slist, void* param=NULL) override;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GeoRaster             (lua_State* L, GeoParms* _parms);
        void            open                  (Raster* raster);
        virtual int64_t getRasterDate         (void);

        const char*     getUUID               (char* uuid_str);
        void            createTransform       (CoordTransform& cord, GDALDataset* dset);
        virtual void    overrideTargetCRS     (OGRSpatialReference& target);
        void            setCRSfromWkt         (OGRSpatialReference& sref, const std::string& wkt);
        bool            containsWindow        (int col, int row, int maxCol, int maxRow, int windowSize);
        int             radius2pixels         (double cellSize, int _radius);
        void            readRasterWithRetry   (GDALRasterBand* band, int col, int row, int colSize, int rowSize,
                                               void* data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Mutex   samplingMutex;

    private:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/
        std::atomic<Raster*> _raster;

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        static int luaDimensions(lua_State* L);
        static int luaBoundingBox(lua_State* L);
        static int luaCellSize(lua_State* L);

        void       readPOI           (Raster* raster);
        void       readPixel         (Raster* raster);
        void       resamplePixel     (Raster* raster);
        void       computeZonalStats (Raster* raster);
        bool       nodataCheck       (Raster* raster);
};




#endif  /* __geo_raster__ */
