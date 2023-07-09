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

#ifndef __gdal_raster__
#define __gdal_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoParms.h"
#include "RasterSample.h"
#include <ogrsf_frmts.h>

/******************************************************************************
 * Typedef and macros used by GDAL class
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
 * GDAL RASTER CLASS
 ******************************************************************************/

class GdalRaster
{
    public:
        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_SAMPLING_RADIUS_IN_PIXELS = 50;
        static const int SLIDERULE_EPSG                = 7912;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
        * overrideCRS_t
        *     Callback definition for overriding Spatial Reference System.
        *     NOTE: implementation must be thread-safe
        *--------------------------------------------------------------------*/
        typedef OGRErr (*overrideCRS_t)(OGRSpatialReference& crs);

        class Point
        {
        public:
            Point(double _x=0, double _y=0, double _z=0):
              x(_x), y(_y), z(_z) {}

            Point(const Point& p):
              x(p.x), y(p.y), z(p.z) {}

            Point& operator = (const Point& p)
              { x=p.x; y=p.y; z=p.z; return *this; }

            void clear(void)
              { x=0; y=0; z=0; }

            double x;
            double y;
            double z;
        };

        /* import bbox_t into this namespace from GeoParms.h */
        using bbox_t=GeoParms::bbox_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                           GdalRaster     (GeoParms* _parms, const std::string& _fileName, double _gpsTime, bool _dataIsElevation, overrideCRS_t cb);
        virtual           ~GdalRaster     (void);
        void               samplePOI      (const Point& poi);
        const std::string& getFileName    (void) { return fileName;}
        RasterSample&      getSample      (void) { return sample; }
        bool               sampled        (void) { return _sampled; }
        int                getRows        (void) { return rows; }
        int                getCols        (void) { return cols; }
        const bbox_t&      getBbox        (void) { return bbox; }
        double             getCellSize    (void) { return cellSize; }

        /*--------------------------------------------------------------------
         * Static Methods
         *--------------------------------------------------------------------*/

        static void        setCRSfromWkt  (OGRSpatialReference& sref, const char* wkt);
        static std::string getUUID        (void);
        static void        initAwsAccess  (GeoParms* _parms);

    private:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        GeoParms*      parms;
        bool          _sampled;
        double         gpsTime;  /* Time the raster data was collected and/or generated */

        /* Last sample information */
        RasterSample   sample;
        double         verticalShift;  /* Calculated for last POI transformed to target CRS */

        OGRCoordinateTransformation* transf;
        OGRSpatialReference sourceCRS;
        OGRSpatialReference targetCRS;
        overrideCRS_t       overrideCRS;

        std::string     fileName;
        GDALDataset    *dset;
        GDALRasterBand* band;
        bool            dataIsElevation;
        uint32_t        rows;
        uint32_t        cols;
        double          cellSize;
        bbox_t          bbox;
        uint32_t        radiusInPixels;

        double geoTransform[6];
        double invGeoTrnasform[6];

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void        open                (void);
        void        readPixel           (const Point& poi);
        void        resamplePixel       (const Point& poi);
        void        computeZonalStats   (const Point& poi);
        inline bool nodataCheck         (void);
        void        createTransform     (void);
        int         radius2pixels       (int _radius);
        inline bool containsWindow      (int col, int row, int maxCol, int maxRow, int windowSize);
        inline void readRasterWithRetry (int col, int row, int colSize, int rowSize,
                                         void* data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args);
};

#endif  /* __gdal_raster__ */
