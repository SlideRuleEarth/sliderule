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

#ifndef __geo_indexed_raster__
#define __geo_indexed_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RasterObject.h"
#include "OsApi.h"
#include "RasterSample.h"
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

class GeoIndexedRaster: public RasterObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   INVALID_SAMPLE_VALUE = -1000000;
        static const int   MAX_SAMPLING_RADIUS_IN_PIXELS = 50;
        static const int   MAX_READER_THREADS = 200;
        static const int   MAX_CACHED_RASTERS = 50;
        static const int   SLIDERULE_EPSG = 7912;

        static const char* FLAGS_RASTER_TAG;
        static const char* SAMPLES_RASTER_TAG;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            std::string         tag;
            std::string         fileName;
            TimeLib::gmt_time_t gmtDate;
            int64_t             gpsTime;
        } raster_info_t;

        typedef struct {
            std::string             id;
            Ordering<raster_info_t> list;
            TimeLib::gmt_time_t     gmtDate;
            int64_t                 gpsTime;
        } rasters_group_t;


        typedef struct {
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
            inline bool containsPoint(OGRPoint& p);

            GeoDataSet(void) { clear(false); }
           ~GeoDataSet(void) { clear(); }
        };


        class GeoIndex: public GeoDataSet {};


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
            RasterSample        sample;

            Raster(void);
        };


        typedef struct {
            GeoIndexedRaster*  obj;
            Thread*     thread;
            Raster*     raster;
            Cond*       sync;
            bool        run;
        } reader_t;


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual         ~GeoIndexedRaster  (void);
        void            getSamples  (double lon, double lat, double height, int64_t gps, List<RasterSample>& slist, void* param=NULL) override;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GeoIndexedRaster             (lua_State* L, GeoParms* _parms);
        virtual void    getGroupSamples       (const rasters_group_t& rgroup, List<RasterSample>& slist, uint32_t flags);
        double          getGmtDate            (const OGRFeature* feature, const char* field,  TimeLib::gmt_time_t& gmtDate);
        const char*     getUUID               (char* uuid_str);
        virtual void    openGeoIndex          (double lon = 0, double lat = 0) = 0;
        virtual bool    findRasters           (OGRPoint& p) = 0;
        void            createTransform       (CoordTransform& cord, GDALDataset* dset);
        virtual void    overrideTargetCRS     (OGRSpatialReference& target);
        void            setCRSfromWkt         (OGRSpatialReference& sref, const std::string& wkt);
        bool            containsWindow        (int col, int row, int maxCol, int maxRow, int windowSize);
        virtual bool    findCachedRasters     (OGRPoint& p) = 0;
        int             radius2pixels         (double cellSize, int _radius);
        void            sampleRasters         (void);
        void            processRaster         (Raster* raster);
        void            readRasterWithRetry   (GDALRasterBand* band, int col, int row, int colSize, int rowSize,
                                               void* data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args);

        int             sample                (double lon, double lat, double height, int64_t gps);

        inline bool containsPoint (Raster* raster, OGRPoint& p)
        {
            return (raster && raster->dset &&
                (p.getX() >= raster->bbox.lon_min) && (p.getX() <= raster->bbox.lon_max) &&
                (p.getY() >= raster->bbox.lat_min) && (p.getY() <= raster->bbox.lat_max));
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Ordering<rasters_group_t>*  rasterGroupList;
        bool                        useGeoIndex;
        GeoIndex                    geoIndex;
        Dictionary<Raster*>         rasterDict;
        Mutex                       samplingMutex;
        bool                        forceNotElevation;

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DATA_TO_SAMPLE = 0;
        static const int DATA_SAMPLED = 1;
        static const int NUM_SYNC_SIGNALS = 2;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        reader_t*    rasterRreader;
        uint32_t     readerCount;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaDimensions(lua_State* L);
        static int luaBoundingBox(lua_State* L);
        static int luaCellSize(lua_State* L);

        static void* readingThread (void *param);

        bool       filterRasters           (int64_t gps);
        void       createThreads           (void);
        void       updateCache             (OGRPoint& p);
        void       invalidateCache         (void);
        int        getSampledRastersCount  (void);
        void       readPixel               (Raster* raster);
        void       resamplePixel           (Raster* raster);
        void       computeZonalStats       (Raster* raster);
        bool       nodataCheck             (Raster* raster);
        uint32_t   removeOldestRasterGroup (void);
};




#endif  /* __geo_indexed_raster__ */
