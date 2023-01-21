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

#include "LuaObject.h"
#include "OsApi.h"
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



/* CRS used by ICESat2 pthotons */
#define PHOTON_CRS 4326


typedef struct
{
    double lon_min;
    double lat_min;
    double lon_max;
    double lat_max;
} bbox_t;


/******************************************************************************
 * GEO RASTER CLASS
 ******************************************************************************/

class GeoRaster: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   INVALID_SAMPLE_VALUE = -1000000;
        static const int   MAX_SAMPLING_RADIUS_IN_PIXELS = 50;
        static const int   MAX_READER_THREADS = 200;
        static const int   MAX_CACHED_RASTERS = 10;

        static const char* NEARESTNEIGHBOUR_ALGO;
        static const char* BILINEAR_ALGO;
        static const char* CUBIC_ALGO;
        static const char* CUBICSPLINE_ALGO;
        static const char* LANCZOS_ALGO;
        static const char* AVERAGE_ALGO;
        static const char* MODE_ALGO;
        static const char* GAUSS_ALGO;
        static const char* ZONALSTATS_ALGO;

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            double value;
            double time;

            struct {
                uint32_t count;
                double   min;
                double   max;
                double   mean;
                double   median;
                double   stdev;
                double   mad;
            } stats;
        } sample_t;


        typedef struct {
            std::string     fileName;
            GDALDataset*    dset;
            GDALRasterBand* band;
            double          invGeot[6];
            uint32_t        rows;
            uint32_t        cols;
            double          cellSize;
            bbox_t          bbox;
            uint32_t        radiusInPixels;

            OGRCoordinateTransformation *transf;
            OGRSpatialReference          srcSrs;
            OGRSpatialReference          trgSrs;
        } raster_index_set_t;


        typedef struct {
            bool            enabled;
            bool            sampled;
            GDALDataset*    dset;
            GDALRasterBand* band;
            std::string     fileName;
            GDALDataType    dataType;

            uint32_t        rows;
            uint32_t        cols;
            bbox_t          bbox;
            double          cellSize;
            int32_t         xBlockSize;
            int32_t         yBlockSize;
            uint32_t        radiusInPixels;

            double          gpsTime;

            /* Last sample information */
            OGRPoint        point;
            sample_t        sample;
        } raster_t;


        typedef struct {
            GeoRaster*      obj;
            Thread*         thread;
            raster_t*       raster;
            Cond*           sync;
            bool            run;
        } reader_t;


        typedef GeoRaster* (*factory_t) (lua_State* L, const char* dem_sampling, const int sampling_radius, const bool zonal_stats);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void    init            (void);
        static void    deinit          (void);
        static int     luaCreate       (lua_State* L);
        static bool    registerRaster  (const char* _name, factory_t create);
        int            sample          (double lon, double lat, List<sample_t>& slist, void* param=NULL);
        inline bool    hasZonalStats   (void) { return zonalStats; }
        inline void    setCheckCacheFirst(bool value) { checkCacheFirst = value; }
        inline void    setAllowIndexDataSetSampling(bool value) { allowIndexDataSetSampling = value; }
        const char*    getUUID         (char* uuid_str);
        void           buildVRT        (std::string& vrtFile, List<std::string>& rlist);
        virtual       ~GeoRaster       (void);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GeoRaster               (lua_State* L, const char* dem_sampling, const int sampling_radius, const bool zonal_stats=false);
        virtual bool    openRasterIndexSet      (double lon=0, double lat=0) = 0;
        virtual bool    findRasterFilesWithPoint(OGRPoint &p) = 0;
        virtual int64_t getRasterDate           (std::string& tifFile) = 0;
        int             radius2pixels           (double cellSize, int _radius);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<std::string>*    tifList;
        raster_index_set_t    ris;
        uint32_t              samplingRadius;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex factoryMut;
        static Dictionary<factory_t> factories;

        Mutex                 samplingMutex;
        Dictionary<raster_t*> rasterDict;
        reader_t*             rasterRreader;
        uint32_t              readerCount;

        GDALRIOResampleAlg    sampleAlg;
        bool                  zonalStats;

        bool                  checkCacheFirst;
        bool                  allowIndexDataSetSampling;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaDimensions(lua_State *L);
        static int luaBoundingBox(lua_State *L);
        static int luaCellSize(lua_State *L);
        static int luaSamples(lua_State *L);

        static void* readingThread (void *param);

        void    createReaderThreads      (void);
        void    processRaster            (raster_t* raster, GeoRaster* obj);
        void    updateRastersCache       (OGRPoint &p);
        bool    rasterIndexContainsPoint (OGRPoint &p);
        bool    rasterContainsPoint      (raster_t *raster, OGRPoint &p);
        bool    findCachedRasterWithPoint(OGRPoint &p, raster_t **raster);
        int     sample                   (double lon, double lat);
        void    sampleRasters            (void);
        void    invalidateRastersCache   (void);
        int     getSampledRastersCount   (void);
        void    clearRasterIndexSet      (raster_index_set_t* iset);
        void    clearRaster              (raster_t *raster);
        void    readPixel                (raster_t *raster);
        void    resamplePixel            (raster_t *raster, GeoRaster *obj);
        void    computeZonalStats        (raster_t *raster, GeoRaster* obj);
        void    RasterIoWithRetry        (GDALRasterBand *band, int col, int row, int colSize, int rowSize,
                                          void *data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args);
};




#endif  /* __geo_raster__ */
