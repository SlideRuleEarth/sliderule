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

#ifndef __vrt_raster__
#define __vrt_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "OsApi.h"
#include "GeoRaster.h"
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <vrtdataset.h>

/******************************************************************************
 * VRT RASTER CLASS
 ******************************************************************************/

class VrtRaster: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   INVALID_SAMPLE_VALUE = -1000000;
        static const int   MAX_READER_THREADS = 200;
        static const int   MAX_CACHED_RASTERS = 10;

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            double value;
            double time;
        } sample_t;


        typedef struct {
            double min;
            double max;
            double mean;
            double standardDeviaton;
        } zonal_stats_t;


        typedef struct {
            std::string     fileName;
            VRTDataset*     dset;
            GDALRasterBand* band;
            double          invGeot[6];
            uint32_t        rows;
            uint32_t        cols;
            double          cellSize;
            bbox_t          bbox;

            OGRCoordinateTransformation *transf;
            OGRSpatialReference          srcSrs;
            OGRSpatialReference          trgSrs;
        } vrt_t;


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

            double          gpsTime;

            /* Last sample information */
            OGRPoint        point;
            sample_t        sample;
            zonal_stats_t   zstats;
        } raster_t;


        typedef struct {
            VrtRaster*      obj;
            Thread*         thread;
            raster_t*       raster;
            Cond*           sync;
            bool            run;
        } reader_t;


        typedef VrtRaster* (*factory_t) (lua_State* L, const char* dem_sampling, const int sampling_radius);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void             init            (void);
        static void             deinit          (void);
        static int              luaCreate       (lua_State* L);
        static bool             registerRaster  (const char* _name, factory_t create);
        int                     sample          (double lon, double lat, List<sample_t> &slist, void* param=NULL);
        virtual                ~VrtRaster       (void);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        VrtRaster     (lua_State* L, const char* dem_sampling, const int sampling_radius);
        bool            openVrtDset   (double lon=0, double lat=0);
        virtual void    getVrtFileName(std::string& vrtFile, double lon=0, double lat=0) = 0;
        virtual int64_t getRasterDate (std::string& tifFile) = 0;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool checkCacheFirst;
        bool allowVrtDataSetSampling;
        int  samplingRadius;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex factoryMut;
        static Dictionary<factory_t> factories;

        Mutex samplingMutex;
        vrt_t vrt;

        List<std::string>*    tifList;
        Dictionary<raster_t*> rasterDict;
        reader_t*             rasterRreader;
        uint32_t              readerCount;

        GDALRIOResampleAlg    sampleAlg;
        bool                  zonalStats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaDimensions(lua_State *L);
        static int luaBoundingBox(lua_State *L);
        static int luaCellSize(lua_State *L);
        static int luaSamples(lua_State *L);

        static void* readingThread (void *param);

        void    createReaderThreads      (void);
        void    processRaster            (raster_t* raster, VrtRaster* obj);
        bool    findTIFfilesWithPoint    (OGRPoint &p);
        void    updateRastersCache       (OGRPoint &p);
        bool    vrtContainsPoint         (OGRPoint &p);
        bool    rasterContainsPoint      (raster_t *raster, OGRPoint &p);
        bool    findCachedRasterWithPoint(OGRPoint &p, raster_t **raster);
        int     sample                   (double lon, double lat);
        void    sampleRasters            (void);
        void    invalidateRastersCache   (void);
        int     getSampledRastersCount   (void);
        void    clearRaster              (raster_t *raster);
        void    clearVrt                 (vrt_t *_vrt);
        void    readPixel                (raster_t *raster);
        int     radius2pixels            (raster_t *raster, int _radius);
        void    resamplePixel            (raster_t *raster, VrtRaster *obj);
        void    computeZonalStats        (raster_t *raster, VrtRaster* obj);
        void    RasterIoWithRetry        (GDALRasterBand *band, int col, int row, int colSize, int rowSize,
                                          void *data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args);
};

#endif  /* __vrt_raster__ */
