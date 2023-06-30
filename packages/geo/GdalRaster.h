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
 * Utility functions
 ******************************************************************************/
const char* getUUID(char* uuid_str);
void initGDALforAWS(GeoParms* _parms);


/******************************************************************************
 * GEO RASTER CLASS
 ******************************************************************************/

class GdalRaster
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int INVALID_SAMPLE_VALUE          = -1000000;
        static const int MAX_SAMPLING_RADIUS_IN_PIXELS = 50;
        static const int SLIDERULE_EPSG                = 7912;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        class Point
        {
        public:
            Point(double _x=0, double _y=0, double _z=0):
              x(_x), y(_y), z(_z) {}

            Point(const Point& p):
              x(p.x), y(p.y), z(p.z) {}

            Point& operator = (const Point& p)
              { x=p.x; y=p.y; z=p.z; return *this; }

            double x;
            double y;
            double z;
        };


        typedef struct
        {
            double lon_min;
            double lat_min;
            double lon_max;
            double lat_max;
        } bbox_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GdalRaster     (GeoParms* _parms, const std::string& _fileName, double _gpsTime, const std::string& _targetWkt);
                       ~GdalRaster     (void);
        void            open           (const std::string& _fileName, double _gpsTime, const std::string& _targetWkt);
        void            open           (void);
        void            setPOI         (const Point& _poi);
        void            samplePOI      (void);
        RasterSample*   getSample      (void) { return sampled ? &sample : NULL; }
        const char*     getFileName    (void) { return fileName.c_str(); }
        int             getRows        (void) { return rows; }
        int             getCols        (void) { return cols; }
        bbox_t&         getBbox        (void) { return bbox; }
        double          getCellSize    (void) { return cellSize; }


        protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

    private:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/
        GeoParms*      parms;
        std::string    targetWkt;

        bool           enabled;
        bool           sampled;
        std::string    groupId;

        double         gpsTime;  /* Time the raster data was collected and/or generated */
        double         useTime;  /* Time the raster was last read/sampled */

        /* Last sample information */
        Point          poi;
        RasterSample   sample;

        double          verticalShift;  /* Calculated for last POI transformed to target CRS */

        OGRCoordinateTransformation* transf;
        OGRSpatialReference sourceCRS;
        OGRSpatialReference targetCRS;

        std::string     fileName;
        GDALDataset    *dset;
        GDALRasterBand* band;
        bool            dataIsElevation;
        uint32_t        rows;
        uint32_t        cols;
        double          cellSize;
        bbox_t          bbox;
        uint32_t        radiusInPixels;

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void        readPixel           (void);
        void        resamplePixel       (void);
        void        computeZonalStats   (void);
        bool        nodataCheck         (void);
        void        createTransform     (void);
        inline bool containsPoint       (void);
        bool        containsWindow      (int col, int row, int maxCol, int maxRow, int windowSize);
        int         radius2pixels       (int _radius);
        void        readRasterWithRetry (int col, int row, int colSize, int rowSize,
                                         void* data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args);

};




#endif  /* __gdal_raster__ */
