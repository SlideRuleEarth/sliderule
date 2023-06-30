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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <cmath>
#include <string>
#include "OsApi.h"
#include "RasterSample.h"
#include "core.h"
#include "GdalRaster.h"

#ifdef __aws__
#include "aws.h"
#endif

#include <uuid/uuid.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <gdal_priv.h>
#include <algorithm>
#include <cstring>
#include <tuple>

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "ogr_spatialref.h"

/*----------------------------------------------------------------------------
 * getUUID
 *----------------------------------------------------------------------------*/
const char* getUUID(char *uuid_str)
{
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return uuid_str;
}


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GdalRaster::GdalRaster(GeoParms* _parms, const std::string& _fileName, double _gpsTime, const std::string& _targetWkt)
{
    /* Initialize Class Data Members */
    parms    = _parms;
    fileName = _fileName;
    gpsTime  = _gpsTime;
    useTime  = 0;
    targetWkt= _targetWkt;

    enabled = false;
    sampled = false;

    dset = NULL;
    band = NULL;
    dataIsElevation = false;
    rows = 0;
    cols = 0;
    cellSize = 0;
    bzero(&bbox, sizeof(bbox_t));
    verticalShift = 0;
    radiusInPixels = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GdalRaster::~GdalRaster(void)
{
    if(dset) GDALClose((GDALDatasetH)dset);
}

/*----------------------------------------------------------------------------
 * open
 *----------------------------------------------------------------------------*/
void GdalRaster::open(const std::string& _fileName, double _gpsTime, const std::string& _targetWkt)
{
    if(dset == NULL)
    {
        if(_fileName.length() > 0)
            fileName = _fileName;

        if(_gpsTime > 0)
            gpsTime = _gpsTime;

        if(_targetWkt.length() > 0)
            targetWkt = _targetWkt;

        open();
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to opened raster, already opened with file: %s", fileName.c_str());
}

/*----------------------------------------------------------------------------
 * open
 *----------------------------------------------------------------------------*/
void GdalRaster::open(void)
{
    if(dset == NULL)
    {
        dset = (GDALDataset*)GDALOpenEx(fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
        if(dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to opened raster: %s:", fileName.c_str());

        mlog(DEBUG, "Opened %s", fileName.c_str());

        /* Store information about raster */
        cols = dset->GetRasterXSize();
        rows = dset->GetRasterYSize();

        /* Get raster boundry box */
        double geot[6] = {0};
        CPLErr err     = dset->GetGeoTransform(geot);
        CHECK_GDALERR(err);
        bbox.lon_min = geot[0];
        bbox.lon_max = geot[0] + cols * geot[1];
        bbox.lat_max = geot[3];
        bbox.lat_min = geot[3] + rows * geot[5];

        cellSize       = geot[1];
        radiusInPixels = radius2pixels(parms->sampling_radius);

        /* Limit maximum sampling radius */
        if(radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,
                                   "Sampling radius is too big: %d: max allowed %d meters",
                                   parms->sampling_radius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(cellSize));
        }

        /* Get raster block size */
        band = dset->GetRasterBand(1);
        CHECKPTR(band);

        /* Create coordinates transform for raster */
        createTransform();
    }
}

/*----------------------------------------------------------------------------
 * setPOI
 *----------------------------------------------------------------------------*/
void GdalRaster::setPOI(OGRPoint& _point)
{
    point   = _point;
    sampled = false;
    enabled = true;
    sample.clear();
}

/*----------------------------------------------------------------------------
 * readPOI
 *----------------------------------------------------------------------------*/
void GdalRaster::readPOI(void)
{
    try
    {
        if (dset == NULL) open();

        double z0 = point.getZ();
        mlog(DEBUG, "Before transform x,y,z: (%.4lf, %.4lf, %.4lf)", point.getX(), point.getY(), point.getZ());

        if(point.transform(cord.transf) != OGRERR_NONE)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Coordinates Transform failed for x,y,z (%lf, %lf, %lf)", point.getX(), point.getY(), point.getZ());

        mlog(DEBUG, "After  transform x,y,z: (%.4lf, %.4lf, %.4lf)", point.getX(), point.getY(), point.getZ());
        verticalShift = z0 - point.getZ();

        /*
         * Attempt to read raster only if it contains the point of interest.
         */
        if(containsPoint())
        {
            if(parms->sampling_algo == GRIORA_NearestNeighbour)
                readPixel();
            else
                resamplePixel();

            sampled = true;
            useTime = TimeLib::gpstime();
            sample.time = gpsTime;

            if(parms->zonal_stats)
                computeZonalStats();
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading raster: %s", e.what());
    }
}



/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * readPixel
 *----------------------------------------------------------------------------*/
void GdalRaster::readPixel(void)
{
    /* Use fast method recomended by GDAL docs to read individual pixel */
    try
    {
        const int32_t col = static_cast<int32_t>(floor((point.getX() - bbox.lon_min) / cellSize));
        const int32_t row = static_cast<int32_t>(floor((bbox.lat_max - point.getY()) / cellSize));

        int xBlockSize = 0;
        int yBlockSize = 0;
        band->GetBlockSize(&xBlockSize, &yBlockSize);

        /* Raster offsets to block of interest */
        uint32_t xblk = col / xBlockSize;
        uint32_t yblk = row / yBlockSize;

        GDALRasterBlock *block = NULL;
        int cnt = 2;
        do
        {
            /* Retry read if error */
            block = band->GetLockedBlockRef(xblk, yblk, false);
        } while (block == NULL && cnt--);
        CHECKPTR(block);

        /* Get data block pointer, no copy but block is locked */
        void *data = block->GetDataRef();
        if (data == NULL) block->DropLock();
        CHECKPTR(data);

        /* Calculate col, row inside of block */
        uint32_t _col = col % xBlockSize;
        uint32_t _row = row % yBlockSize;
        uint32_t offset = _row * xBlockSize + _col;

        switch(band->GetRasterDataType())
        {
            case GDT_Byte:
            {
                uint8_t *p = (uint8_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_UInt16:
            {
                uint16_t *p = (uint16_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_Int16:
            {
                int16_t *p = (int16_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_UInt32:
            {
                uint32_t *p = (uint32_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_Int32:
            {
                int32_t *p = (int32_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_Int64:
            {
                int64_t *p = (int64_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_UInt64:
            {
                uint64_t *p = (uint64_t *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_Float32:
            {
                float *p = (float *)data;
                sample.value = (double)p[offset];
            }
            break;

            case GDT_Float64:
            {
                double *p = (double *)data;
                sample.value = (double)p[offset];
            }
            break;

            default:
                /*
                 * Complex numbers are supported but not needed at this point.
                 */
                block->DropLock();
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unsuported data type in raster: %s:", fileName.c_str());
        }

        /* Done reading, release block lock */
        block->DropLock();

        if(nodataCheck() && dataIsElevation)
        {
            sample.value += verticalShift;
        }

        // mlog(DEBUG, "Value: %.2lf, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u",
        //      sample.value, col, row, xblk, yblk, _col, _row, offset);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading from raster: %s", e.what());
    }
}

/*----------------------------------------------------------------------------
 * resamplePixel
 *----------------------------------------------------------------------------*/
void GdalRaster::resamplePixel(void)
{
    try
    {
        int col = static_cast<int>(floor((point.getX() - bbox.lon_min) / cellSize));
        int row = static_cast<int>(floor((bbox.lat_max - point.getY()) / cellSize));

        int windowSize, offset;

        /* If zero radius provided, use defaul kernels for each sampling algorithm */
        if (parms->sampling_radius == 0)
        {
            int kernel = 0;

            if (parms->sampling_algo == GRIORA_Bilinear)
                kernel = 2; /* 2x2 kernel */
            else if (parms->sampling_algo == GRIORA_Cubic)
                kernel = 4; /* 4x4 kernel */
            else if (parms->sampling_algo == GRIORA_CubicSpline)
                kernel = 4; /* 4x4 kernel */
            else if (parms->sampling_algo == GRIORA_Lanczos)
                kernel = 6; /* 6x6 kernel */
            else if (parms->sampling_algo == GRIORA_Average)
                kernel = 6; /* No default kernel, pick something */
            else if (parms->sampling_algo == GRIORA_Mode)
                kernel = 6; /* No default kernel, pick something */
            else if (parms->sampling_algo == GRIORA_Gauss)
                kernel = 6; /* No default kernel, pick something */

            windowSize = kernel + 1;    // Odd window size around pixel
            offset = (kernel / 2);
        }
        else
        {
            windowSize = radiusInPixels * 2 + 1;  // Odd window size around pixel
            offset = radiusInPixels;
        }

        int _col = col - offset;
        int _row = row - offset;

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = parms->sampling_algo;
        double  rbuf[1] = {INVALID_SAMPLE_VALUE};

        bool validWindow = containsWindow(_col, _row, cols, rows, windowSize);
        if (validWindow)
        {
            readRasterWithRetry(_col, _row, windowSize, windowSize, rbuf, 1, 1, &args);

            sample.value = rbuf[0];
            if(nodataCheck() && dataIsElevation)
            {
                sample.value += verticalShift;
            }
        }
        else
        {
            /* At least return pixel value if unable to resample raster */
            readPixel();
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error resampling pixel: %s", e.what());
    }
}

/*----------------------------------------------------------------------------
 * computeZonalStats
 *----------------------------------------------------------------------------*/
void GdalRaster::computeZonalStats(void)
{
    double *samplesArray = NULL;

    try
    {
        int col = static_cast<int32_t>(floor((point.getX() - bbox.lon_min) / cellSize));
        int row = static_cast<int32_t>(floor((bbox.lat_max - point.getY()) / cellSize));

        int windowSize = radiusInPixels * 2 + 1; // Odd window size around pixel

        int _col = col - radiusInPixels;
        int _row = row - radiusInPixels;

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = parms->sampling_algo;
        samplesArray = new double[windowSize*windowSize];
        CHECKPTR(samplesArray);

        bool validWindow = containsWindow(_col, _row, cols, rows, windowSize);
        if (validWindow)
        {
            readRasterWithRetry(_col, _row, windowSize, windowSize, samplesArray, windowSize, windowSize, &args);

            /* One of the windows (raster or index data set) was valid. Compute zonal stats */
            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::min();
            double sum = 0;
            std::vector<double> validSamples;

            /*
             * Only use pixels within radius from pixel containing point of interest.
             * Ignore nodata values.
             */
            const double x1 = col; /* Pixel of interest */
            const double y1 = row;

            for (int y = 0; y < windowSize; y++)
            {
                for (int x = 0; x < windowSize; x++)
                {
                    double value = samplesArray[y*windowSize + x];
                    if (value == band->GetNoDataValue()) continue;

                    if(dataIsElevation)
                        value += verticalShift;

                    double x2 = x + _col;  /* Current pixel in buffer */
                    double y2 = y + _row;
                    double xd = std::pow(x2 - x1, 2);
                    double yd = std::pow(y2 - y1, 2);
                    double d  = std::sqrt(xd + yd);

                    if(std::islessequal(d, radiusInPixels))
                    {
                        if (value < min) min = value;
                        if (value > max) max = value;
                        sum += value; /* double may lose precision on overflows, should be ok...*/
                        validSamples.push_back(value);
                    }
                }
            }

            int validSamplesCnt = validSamples.size();
            if (validSamplesCnt > 0)
            {
                double stdev = 0;
                double mad   = 0;
                double mean  = sum / validSamplesCnt;

                for (int i = 0; i < validSamplesCnt; i++)
                {
                    double value = validSamples[i];

                    /* Standard deviation */
                    stdev += std::pow(value - mean, 2);

                    /* Median absolute deviation (MAD) */
                    mad += std::fabs(value - mean);
                }

                stdev = std::sqrt(stdev / validSamplesCnt);
                mad   = mad / validSamplesCnt;

                /*
                 * Median
                 * For performance use nth_element algorithm since it sorts only part of the vector
                 * NOTE: (vector will be reordered by nth_element)
                 */
                std::size_t n = validSamplesCnt / 2;
                std::nth_element(validSamples.begin(), validSamples.begin() + n, validSamples.end());
                double median = validSamples[n];
                if (!(validSamplesCnt & 0x1))
                {
                    /* Even number of samples, calculate average of two middle samples */
                    std::nth_element(validSamples.begin(), validSamples.begin() + n-1, validSamples.end());
                    median = (median + validSamples[n-1]) / 2;
                }

                /* Store calculated zonal stats */
                sample.stats.count  = validSamplesCnt;
                sample.stats.min    = min;
                sample.stats.max    = max;
                sample.stats.mean   = mean;
                sample.stats.median = median;
                sample.stats.stdev  = stdev;
                sample.stats.mad    = mad;
            }
        }
        else mlog(WARNING, "Cannot compute zonal stats, sampling window outside of raster bbox");
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error computing zonal stats: %s", e.what());
    }

    if (samplesArray) delete[] samplesArray;
}

/*----------------------------------------------------------------------------
 * nodataCheck
 *----------------------------------------------------------------------------*/
bool GdalRaster::nodataCheck(void)
{
    /*
     * Replace nodata with NAN
     */
    const double a = band->GetNoDataValue();
    const double b = sample.value;
    const double epsilon = 0.000001;

    if(std::fabs(a-b) < epsilon)
    {
        sample.value = std::nanf("");
        return false;
    }

    return true;
}


/*----------------------------------------------------------------------------
 * createTransform
 *----------------------------------------------------------------------------*/
void GdalRaster::createTransform(void)
{
    CHECKPTR(dset);

    OGRErr ogrerr = cord.source.importFromEPSG(SLIDERULE_EPSG);
    CHECK_GDALERR(ogrerr);

    if(targetWkt.length() > 0)
    {
        ogrerr = cord.target.importFromWkt(targetWkt.c_str());
        mlog(DEBUG, "CRS from caller: %s", targetWkt.c_str());
    }
    else
    {
        const char* projref = dset->GetProjectionRef();
        CHECKPTR(projref);
        ogrerr = cord.target.importFromWkt(projref);
        mlog(DEBUG, "CRS from raster: %s", projref);
    }
    CHECK_GDALERR(ogrerr);

    /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
    cord.target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    cord.source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    cord.transf = OGRCreateCoordinateTransformation(&cord.source, &cord.target);
    if(cord.transf == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinates transform");
}

/*----------------------------------------------------------------------------
 * containsPoint
 *----------------------------------------------------------------------------*/
bool GdalRaster::containsPoint(void)
{
    return (dset &&
            (point.getX() >= bbox.lon_min) && (point.getX() <= bbox.lon_max) &&
            (point.getY() >= bbox.lat_min) && (point.getY() <= bbox.lat_max));
}

/*----------------------------------------------------------------------------
 * containsWindow
 *----------------------------------------------------------------------------*/
bool GdalRaster::containsWindow(int col, int row, int maxCol, int maxRow, int windowSize )
{
    if (col < 0 || row < 0)
        return false;

    if ((col + windowSize >= maxCol) || (row + windowSize >= maxRow))
        return false;

    return true;
}

/*----------------------------------------------------------------------------
 * radius2pixels
 *----------------------------------------------------------------------------*/
int GdalRaster::radius2pixels(int _radius)
{
    /*
     * Code supports only rasters with units in meters (cellSize and radius must be in meters).
     */
    int csize = static_cast<int>(cellSize);

    if (_radius == 0) return 0;
    if (csize == 0) csize = 1;

    int radiusInMeters = ((_radius + csize - 1) / csize) * csize; // Round up to multiples of cell size
    return radiusInMeters / csize;
}

/*----------------------------------------------------------------------------
 * RasterIoWithRetry
 *----------------------------------------------------------------------------*/
void GdalRaster::readRasterWithRetry(int col, int row, int colSize, int rowSize, void *data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args)
{
    /*
     * On AWS reading from S3 buckets may result in failed reads due to network issues/timeouts.
     * There is no way to detect this condition based on the error code returned.
     * Because of it, always retry failed read.
     */
    int cnt = 2;
    CPLErr err = CE_None;
    do
    {
        err = band->RasterIO(GF_Read, col, row, colSize, rowSize, data, dataColSize, dataRowSize, GDT_Float64, 0, 0, args);
    } while (err != CE_None && cnt--);

    if (err != CE_None) throw RunTimeException(CRITICAL, RTE_ERROR, "RasterIO call failed");
}




/*----------------------------------------------------------------------------
 * CoordTransform constructor
 *----------------------------------------------------------------------------*/
GdalRaster::CoordTransform::CoordTransform(void)
{
    transf = NULL;
    source.Clear();
    target.Clear();
}

/*----------------------------------------------------------------------------
 * CoordTransform destructor
 *----------------------------------------------------------------------------*/
GdalRaster::CoordTransform::~CoordTransform(void)
{
    if(transf) OGRCoordinateTransformation::DestroyCT(transf);
}

