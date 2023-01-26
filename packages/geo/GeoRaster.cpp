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

#include "core.h"
#include "GeoRaster.h"
#include "TimeLib.h"

#include <uuid/uuid.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdal_utils.h>
#include <gdalwarper.h>
#include <vrtdataset.h>
#include <ogr_spatialref.h>
#include <gdal_priv.h>
#include <algorithm>

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal.h"
#include "ogr_spatialref.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoRaster::NEARESTNEIGHBOUR_ALGO = "NearestNeighbour";
const char* GeoRaster::BILINEAR_ALGO = "Bilinear";
const char* GeoRaster::CUBIC_ALGO = "Cubic";
const char* GeoRaster::CUBICSPLINE_ALGO = "CubicSpline";
const char* GeoRaster::LANCZOS_ALGO = "Lanczos";
const char* GeoRaster::AVERAGE_ALGO = "Average";
const char* GeoRaster::MODE_ALGO = "Mode";
const char* GeoRaster::GAUSS_ALGO = "Gauss";

const char* GeoRaster::OBJECT_TYPE = "GeoRaster";
const char* GeoRaster::LuaMetaName = "GeoRaster";
const struct luaL_Reg GeoRaster::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"sample",      luaSamples},
    {NULL,          NULL}
};

Mutex GeoRaster::factoryMut;
Dictionary<GeoRaster::factory_t> GeoRaster::factories;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void GeoRaster::init( void )
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void GeoRaster::deinit( void )
{
}

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int GeoRaster::luaCreate( lua_State* L )
{
    try
    {
        /* Get Parameters */
        const char* raster_name     = getLuaString(L, 1);
        const char* dem_sampling    = getLuaString(L, 2, true, NEARESTNEIGHBOUR_ALGO);
        const int   sampling_radius = getLuaInteger(L, 3, true, 0);
        const int   zonal_stats     = getLuaBoolean(L, 4, true, false);

        /* Get Factory */
        factory_t _create = NULL;
        factoryMut.lock();
        {
            factories.find(raster_name, &_create);
        }
        factoryMut.unlock();

        /* Check Factory */
        if(_create == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to find registered raster for %s", raster_name);

        /* Create Raster */
        GeoRaster* _raster = _create(L, dem_sampling, sampling_radius, zonal_stats);
        if(_raster == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create raster of type: %s", raster_name);

        /* Return Object */
        return createLuaObject(L, _raster);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * registerDriver
 *----------------------------------------------------------------------------*/
bool GeoRaster::registerRaster (const char* _name, factory_t create)
{
    bool status;

    factoryMut.lock();
    {
        status = factories.add(_name, create);
    }
    factoryMut.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
int GeoRaster::sample (double lon, double lat, List<sample_t> &slist, void* param)
{
    std::ignore = param;  /* Keep compiler happy, param not used for now */
    slist.clear();

    samplingMutex.lock(); /* Serialize sampling on the same object */

    try
    {
        /* Get samples */
        if (sample(lon, lat) > 0)
        {
            raster_t *raster = NULL;
            const char *key = rasterDict.first(&raster);
            while (key != NULL)
            {
                assert(raster);
                if (raster->enabled && raster->sampled)
                {
                    slist.add(raster->sample);
                }
                key = rasterDict.next(&raster);
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }

    samplingMutex.unlock();

    return slist.length();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoRaster::~GeoRaster(void)
{
    /* Terminate all reader threads */
    for (uint32_t i=0; i < readerCount; i++)
    {
        reader_t *reader = &rasterRreader[i];
        if (reader->thread != NULL)
        {
            reader->sync->lock();
            {
                reader->raster = NULL; /* No raster to read     */
                reader->run = false;   /* Set run flag to false */
                reader->sync->signal(0, Cond::NOTIFY_ONE);
            }
            reader->sync->unlock();

            delete reader->thread;  /* delte thread waits on thread to join */
            delete reader->sync;
        }
    }

    delete [] rasterRreader;

    /* Close all rasters */
    raster_t* raster = NULL;
    const char* key  = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        if (raster->dset) GDALClose((GDALDatasetH)raster->dset);
        delete raster;
        raster = NULL;
        key = rasterDict.next(&raster);
    }

    /* Close raster index data set and transform */
    if (ris.dset) GDALClose((GDALDatasetH)ris.dset);
    if (ris.transf) OGRCoordinateTransformation::DestroyCT(ris.transf);
    if (tifList) delete tifList;
}

/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
int GeoRaster::sample(double lon, double lat)
{
    invalidateRastersCache();

    /* Initial call, open raster index file if not already opened */
    if (ris.dset == NULL)
    {
        if (!openRasterIndexSet(lon, lat))
            throw RunTimeException(CRITICAL, RTE_ERROR, "Could not open raster index data set file for point lon: %lf, lat: %lf", lon, lat);
    }

    OGRPoint p = {lon, lat};
    if (p.transform(ris.transf) != OGRERR_NONE)
        throw RunTimeException(CRITICAL, RTE_ERROR, "transform failed for point: lon: %lf, lat: %lf", lon, lat);

    /* If point is not in current index dataset, open new one for this lon, lat */
    if (!rasterIndexContainsPoint(p))
    {
        if (!openRasterIndexSet(lon, lat))
            throw RunTimeException(CRITICAL, RTE_ERROR, "Could not open raster index file data set for point lon: %lf, lat: %lf", lon, lat);
    }

    bool findNewTifFiles = true;

    if( checkCacheFirst )
    {
        raster_t *raster = NULL;
        if (findCachedRasterWithPoint(p, &raster))
        {
            raster->enabled = true;
            raster->point = p;

            /* Found raster with point in cache, no need to look for new tif file */
            findNewTifFiles = false;
        }
    }

    if (findNewTifFiles && findRasterFilesWithPoint(p))
    {
        updateRastersCache(p);
    }

    sampleRasters();

    return getSampledRastersCount();
}


/*----------------------------------------------------------------------------
 * getUUID
 *----------------------------------------------------------------------------*/
const char* GeoRaster::getUUID(char *uuid_str)
{
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return uuid_str;
}


/*----------------------------------------------------------------------------
 * buildVRT
 *----------------------------------------------------------------------------*/
void GeoRaster::buildVRT(std::string& vrtFile, List<std::string>& rlist)
{
    GDALDataset* vrtDset = NULL;
    std::vector<const char*> rasters;

    for (int i = 0; i < rlist.length(); i++)
    {
        rasters.push_back(rlist[i].c_str());
    }

    vrtDset = (GDALDataset*) GDALBuildVRT(vrtFile.c_str(), rasters.size(), NULL, rasters.data(), NULL, NULL);
    CHECKPTR(vrtDset);
    GDALClose(vrtDset);
    mlog(DEBUG, "Created %s", vrtFile.c_str());
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoRaster::GeoRaster(lua_State *L, const char *dem_sampling, const int sampling_radius, const bool zonal_stats):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    CHECKPTR(dem_sampling);

    allowIndexDataSetSampling = false;
    zonalStats = false;
    sampleAlg  = (GDALRIOResampleAlg) -1;

    if      (!strcasecmp(dem_sampling, NEARESTNEIGHBOUR_ALGO)) sampleAlg = GRIORA_NearestNeighbour;
    else if (!strcasecmp(dem_sampling, BILINEAR_ALGO))         sampleAlg = GRIORA_Bilinear;
    else if (!strcasecmp(dem_sampling, CUBIC_ALGO))            sampleAlg = GRIORA_Cubic;
    else if (!strcasecmp(dem_sampling, CUBICSPLINE_ALGO))      sampleAlg = GRIORA_CubicSpline;
    else if (!strcasecmp(dem_sampling, LANCZOS_ALGO))          sampleAlg = GRIORA_Lanczos;
    else if (!strcasecmp(dem_sampling, AVERAGE_ALGO))          sampleAlg = GRIORA_Average;
    else if (!strcasecmp(dem_sampling, MODE_ALGO))             sampleAlg = GRIORA_Mode;
    else if (!strcasecmp(dem_sampling, GAUSS_ALGO))            sampleAlg = GRIORA_Gauss;
    else
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling algorithm: %s:", dem_sampling);

    zonalStats = zonal_stats;

    if (sampling_radius >= 0)
    {
        samplingRadius = sampling_radius;
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling radius: %d:", sampling_radius);

    /* Initialize Class Data Members */
    clearRasterIndexSet( &ris );

    tifList = new List<std::string>;
    tifList->clear();
    rasterDict.clear();
    rasterRreader = new reader_t[MAX_READER_THREADS];
    bzero(rasterRreader, sizeof(reader_t)*MAX_READER_THREADS);
    readerCount = 0;
    checkCacheFirst = false;
}


/*----------------------------------------------------------------------------
 * radius2pixels
 *----------------------------------------------------------------------------*/
int GeoRaster::radius2pixels(double cellSize, int _radius)
{
    /*
     * Code supports rasters with units in meters (cellSize and radius must be in meters).
     *
     * ArcticDEM - EPSG:3413 - WGS 84 / NSIDC Sea Ice Polar Stereographic North (units meters - supported)
     *             EPSG:4326 - WGS 84 / Geographic (units degrees, coordinates in lat/lon - not supported)
     *
     * TODO: if needed, add support for rasters with units other than meters
     */
    int csize = static_cast<int>(cellSize);

    if (_radius == 0) return 0;
    if ( csize  == 0) csize = 1;

    int radiusInMeters = ((_radius + csize - 1) / csize) * csize; // Round up to multiples of cell size
    int radiusInPixels = radiusInMeters / csize;
    return radiusInPixels;
}



/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * RasterIoWithRetry
 *----------------------------------------------------------------------------*/
void GeoRaster::RasterIoWithRetry(GDALRasterBand *band, int col, int row, int colSize, int rowSize, void *data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args)
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
 * readPixel
 *----------------------------------------------------------------------------*/
void GeoRaster::readPixel(raster_t *raster)
{
    /* Use fast method recomended by GDAL docs to read individual pixel */
    try
    {
        const int32_t col = static_cast<int32_t>(floor((raster->point.getX() - raster->bbox.lon_min) / raster->cellSize));
        const int32_t row = static_cast<int32_t>(floor((raster->bbox.lat_max - raster->point.getY()) / raster->cellSize));

        /* Raster offsets to block of interest */
        uint32_t xblk = col / raster->xBlockSize;
        uint32_t yblk = row / raster->yBlockSize;

        GDALRasterBlock *block = NULL;
        int cnt = 2;
        do
        {
            /* Retry read if error */
            block = raster->band->GetLockedBlockRef(xblk, yblk, false);
        } while (block == NULL && cnt--);
        CHECKPTR(block);

        /* Get data block pointer, no copy but block is locked */
        void *data = block->GetDataRef();
        if (data == NULL) block->DropLock();
        CHECKPTR(data);

        /* Calculate col, row inside of block */
        uint32_t _col = col % raster->xBlockSize;
        uint32_t _row = row % raster->yBlockSize;
        uint32_t offset = _row * raster->xBlockSize + _col;

        switch(raster->dataType)
        {
            case GDT_Byte:
            {
                uint8_t *p = (uint8_t *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_UInt16:
            {
                uint16_t *p = (uint16_t *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_Int16:
            {
                int16_t *p = (int16_t *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_UInt32:
            {
                uint32_t *p = (uint32_t *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_Int32:
            {
                int32_t *p = (int32_t *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_Float32:
            {
                float *p = (float *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_Float64:
            {
                double *p = (double *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            default:
                /*
                 * This version of GDAL does not support 64bit integers
                 * Complex numbers are supported but not needed at this point.
                 */
                block->DropLock();
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unsuported dataType in raster: %s:", raster->fileName.c_str());
        }

        /* Done reading, release block lock */
        block->DropLock();

        // mlog(DEBUG, "Value: %lf, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u",
        //      raster->sample.value, col, row, xblk, yblk, _col, _row, offset);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading from raster: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * rasterContainsWindow
 *----------------------------------------------------------------------------*/
bool rasterContainsWindow(int col, int row, int maxCol, int maxRow, int windowSize )
{
    if (col < 0 || row < 0)
        return false;

    if ((col + windowSize >= maxCol) || (row + windowSize >= maxRow))
        return false;

    return true;
}


/*----------------------------------------------------------------------------
 * resamplePixel
 *----------------------------------------------------------------------------*/
void GeoRaster::resamplePixel(raster_t *raster, GeoRaster* obj)
{
    try
    {
        int col = static_cast<int32_t>(floor((raster->point.getX() - raster->bbox.lon_min) / raster->cellSize));
        int row = static_cast<int32_t>(floor((raster->bbox.lat_max - raster->point.getY()) / raster->cellSize));

        int _col, _row, windowSize;

        /* If zero radius provided, use defaul kernels for each sampling algorithm */
        if (obj->samplingRadius == 0)
        {
            int kernel = 0;

            if (sampleAlg == GRIORA_Bilinear)
                kernel = 2; /* 2x2 kernel */
            else if (sampleAlg == GRIORA_Cubic)
                kernel = 4; /* 4x4 kernel */
            else if (sampleAlg == GRIORA_CubicSpline)
                kernel = 4; /* 4x4 kernel */
            else if (sampleAlg == GRIORA_Lanczos)
                kernel = 6; /* 6x6 kernel */
            else if (sampleAlg == GRIORA_Average)
                kernel = 6; /* No default kernel, pick something */
            else if (sampleAlg == GRIORA_Mode)
                kernel = 6; /* No default kernel, pick something */
            else if (sampleAlg == GRIORA_Gauss)
                kernel = 6; /* No default kernel, pick something */

            windowSize = kernel + 1;    // Odd window size around pixel
            _col = col - (kernel / 2);
            _row = row - (kernel / 2);
        }
        else
        {
            windowSize = raster->radiusInPixels * 2 + 1;  // Odd window size around pixel
            _col = col - raster->radiusInPixels;
            _row = row - raster->radiusInPixels;
        }

        bool windowIsValid = rasterContainsWindow(_col, _row, raster->cols, raster->rows, windowSize);

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = obj->sampleAlg;
        double rbuf[1] = {INVALID_SAMPLE_VALUE};

        if (windowIsValid)
        {
            RasterIoWithRetry(raster->band, _col, _row, windowSize, windowSize, rbuf, 1, 1, &args);
            raster->sample.value = rbuf[0];
            return;
        }

        if (allowIndexDataSetSampling)
        {
             /* Use raster index data set instead of current raster */
            col = static_cast<int32_t>(floor((raster->point.getX() - ris.bbox.lon_min) / ris.cellSize));
            row = static_cast<int32_t>(floor((ris.bbox.lat_max - raster->point.getY()) / ris.cellSize));

            windowSize = ris.radiusInPixels * 2 + 1;  // Odd window size around pixel
            _col = col - ris.radiusInPixels;
            _row = row - ris.radiusInPixels;

            /* Make sure sampling window is valid for raster index data set */
            windowIsValid = rasterContainsWindow(_col, _row, ris.cols, ris.rows, windowSize);

            if (windowIsValid)
            {
                RasterIoWithRetry(ris.band, _col, _row, windowSize, windowSize, rbuf, 1, 1, &args);
                raster->sample.value = rbuf[0];
            }
            return;
        }

        /* At least return pixel value if unable to resample raster and/or raster index data set */
        obj->readPixel(raster);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error resampling pixel: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * zonalStats
 *----------------------------------------------------------------------------*/
void GeoRaster::computeZonalStats(raster_t *raster, GeoRaster *obj)
{
    double *samplesArray = NULL;

    try
    {
        int col = static_cast<int32_t>(floor((raster->point.getX() - raster->bbox.lon_min) / raster->cellSize));
        int row = static_cast<int32_t>(floor((raster->bbox.lat_max - raster->point.getY()) / raster->cellSize));

        int radiusInPixels = raster->radiusInPixels;
        int _col, _row, windowSize;

        windowSize = radiusInPixels * 2 + 1; // Odd window size around pixel
        _col = col - radiusInPixels;
        _row = row - radiusInPixels;

        bool windowIsValid = rasterContainsWindow(_col, _row, raster->cols, raster->rows, windowSize);

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = obj->sampleAlg;
        samplesArray = new double[windowSize*windowSize];
        CHECKPTR(samplesArray);

        double noDataValue = raster->band->GetNoDataValue();

        if (windowIsValid)
        {
            RasterIoWithRetry(raster->band, _col, _row, windowSize, windowSize, samplesArray, windowSize, windowSize, &args);
        }
        else if (allowIndexDataSetSampling)
        {
             /* Use raster index data set instead of current raster */
            col = static_cast<int32_t>(floor((raster->point.getX() - ris.bbox.lon_min) / ris.cellSize));
            row = static_cast<int32_t>(floor((ris.bbox.lat_max - raster->point.getY()) / ris.cellSize));

            radiusInPixels = ris.radiusInPixels;
            _col = col - radiusInPixels;
            _row = row - radiusInPixels;

            /* Make sure sampling window is valid for raster index data set */
            windowIsValid = rasterContainsWindow(_col, _row, ris.cols, ris.rows, windowSize);

            if (windowIsValid)
            {
                /* Get nodata value from raster index data set, it shoudl be the same as raster but just in case it is not...*/
                noDataValue = ris.band->GetNoDataValue();
                RasterIoWithRetry(ris.band, _col, _row, windowSize, windowSize, samplesArray, windowSize, windowSize, &args);
            }
        }

        if(windowIsValid)
        {
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
                    if (value == noDataValue) continue;

                    double x2 = x + _col;  /* Current pixel in buffer */
                    double y2 = y + _row;
                    double xd = std::pow(x2 - x1, 2);
                    double yd = std::pow(y2 - y1, 2);
                    double d  = sqrtf64(xd + yd);

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
                    mad += fabsf64(value - mean);
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
                raster->sample.stats.count  = validSamplesCnt;
                raster->sample.stats.min    = min;
                raster->sample.stats.max    = max;
                raster->sample.stats.mean   = mean;
                raster->sample.stats.median = median;
                raster->sample.stats.stdev  = stdev;
                raster->sample.stats.mad    = mad;
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
 * clearRasterIndexSet
 *----------------------------------------------------------------------------*/
void GeoRaster::clearRasterIndexSet(raster_index_set_t *iset)
{
    iset->fileName.clear();
    iset->dset = NULL;
    iset->band = NULL;
    bzero(iset->invGeot, sizeof(iset->invGeot));
    iset->rows = 0;
    iset->cols = 0;
    iset->cellSize = 0;
    bzero(&iset->bbox, sizeof(bbox_t));
    iset->radiusInPixels = 0;
    iset->transf = NULL;
    iset->srcSrs.Clear();
    iset->trgSrs.Clear();
}

/*----------------------------------------------------------------------------
 * clearRaster
 *----------------------------------------------------------------------------*/
void GeoRaster::clearRaster(raster_t *raster)
{
    raster->enabled = false;
    raster->sampled = false;
    raster->dset = NULL;
    raster->band = NULL;
    raster->fileName.clear();
    raster->dataType = GDT_Unknown;

    raster->rows = 0;
    raster->cols = 0;
    bzero(&raster->bbox, sizeof(bbox_t));
    raster->cellSize = 0;
    raster->xBlockSize = 0;
    raster->yBlockSize = 0;
    raster->radiusInPixels = 0;
    raster->gpsTime = 0;
    raster->point.empty();
    bzero(&raster->sample, sizeof(sample_t));
}

/*----------------------------------------------------------------------------
 * invaldiateAllRasters
 *----------------------------------------------------------------------------*/
void GeoRaster::invalidateRastersCache(void)
{
    raster_t *raster = NULL;
    const char* key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        raster->enabled = false;
        raster->sampled = false;
        raster->point.empty();
        bzero(&raster->sample, sizeof(sample_t));
        raster->sample.value = INVALID_SAMPLE_VALUE;
        key = rasterDict.next(&raster);
    }
}

/*----------------------------------------------------------------------------
 * updateRastersCache
 *----------------------------------------------------------------------------*/
void GeoRaster::updateRastersCache(OGRPoint& p)
{
    if (tifList->length() == 0)
        return;

    const char *key  = NULL;
    raster_t *raster = NULL;

    /* Check new tif file list against rasters in dictionary */
    for (int i = 0; i < tifList->length(); i++)
    {
        std::string& fileName = tifList->get(i);
        key = fileName.c_str();

        if (rasterDict.find(key, &raster))
        {
            /* Update point to be sampled, mark raster enabled for next sampling */
            assert(raster);
            raster->enabled = true;
            raster->point = p;
        }
        else
        {
            /* Create new raster for this tif file since it is not in the dictionary */
            raster = new raster_t;
            assert(raster);
            clearRaster(raster);
            raster->enabled = true;
            raster->point = p;
            raster->sample.value = INVALID_SAMPLE_VALUE;
            raster->fileName = fileName;
            rasterDict.add(key, raster);
        }
    }

    /* Maintain cache from getting too big */
    key = rasterDict.first(&raster);
    while (key != NULL)
    {
        if (rasterDict.length() <= MAX_CACHED_RASTERS)
            break;

        assert(raster);
        if (!raster->enabled)
        {
            /* Main thread closing multiple rasters is OK */
            if (raster->dset) GDALClose((GDALDatasetH)raster->dset);
            rasterDict.remove(key);
            delete raster;
        }
        key = rasterDict.next(&raster);
    }
}


/*----------------------------------------------------------------------------
 * createReaderThreads
 *----------------------------------------------------------------------------*/
void GeoRaster::createReaderThreads(void)
{
    uint32_t threadsNeeded = rasterDict.length();
    if (threadsNeeded <= readerCount)
        return;

    uint32_t newThreadsCnt = threadsNeeded - readerCount;
    // print2term("Creating %d new threads, readerCount: %d, neededThreads: %d\n", newThreadsCnt, readerCount, threadsNeeded);

    for (uint32_t i=0; i < newThreadsCnt; i++)
    {
        if (readerCount < MAX_READER_THREADS)
        {
            reader_t *reader = &rasterRreader[readerCount];
            reader->raster = NULL;
            reader->run = true;
            reader->sync = new Cond();
            reader->obj = this;
            reader->sync->lock();
            {
                reader->thread = new Thread(readingThread, reader);
            }
            reader->sync->unlock();
            readerCount++;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,
                                   "number of rasters to read: %d, is greater than max reading threads %d\n",
                                   rasterDict.length(), MAX_READER_THREADS);
        }
    }
    assert(readerCount == threadsNeeded);
}


/*----------------------------------------------------------------------------
 * sampleRasters
 *----------------------------------------------------------------------------*/
void GeoRaster::sampleRasters(void)
{
    if (rasterDict.length() == 1)
    {
        /*
         * Threaded raster reader code can read multiple rasters in parallel.
         * For optimization, if there is only one raster in the dictionary use this thread to read it.
         */
        raster_t *raster = NULL;
        rasterDict.first(&raster);
        assert(raster);
        if (raster->enabled) processRaster(raster, this);
        return;
    }

    /* Create additional reader threads if needed */
    createReaderThreads();

    /* For each raster which is marked to be sampled, give it to the reader thread */
    int signaledReaders = 0;
    raster_t *raster = NULL;
    const char *key = rasterDict.first(&raster);
    int i = 0;
    while (key != NULL)
    {
        assert(raster);
        if (raster->enabled)
        {
            reader_t *reader = &rasterRreader[i++];
            reader->sync->lock();
            {
                reader->raster = raster;
                reader->sync->signal(0, Cond::NOTIFY_ONE);
                signaledReaders++;
            }
            reader->sync->unlock();
        }
        key = rasterDict.next(&raster);
    }

    /* Did not signal any reader threads, don't wait */
    if (signaledReaders == 0) return;

    /* Wait for all reader threads to finish sampling */
    for (uint32_t j=0; j<readerCount; j++)
    {
        reader_t *reader = &rasterRreader[j];
        raster = NULL;
        do
        {
            reader->sync->lock();
            {
                raster = reader->raster;
            }
            reader->sync->unlock();
        } while (raster != NULL);
    }
}

/*----------------------------------------------------------------------------
 * readingThread
 *----------------------------------------------------------------------------*/
void* GeoRaster::readingThread(void *param)
{
    reader_t *reader = (reader_t*)param;
    bool run = true;

    while (run)
    {
        reader->sync->lock();
        {
            /* Wait for raster to work on */
            while ((reader->raster == NULL) && reader->run)
                reader->sync->wait(0, SYS_TIMEOUT);

            if(reader->raster != NULL)
            {
                reader->obj->processRaster(reader->raster, reader->obj);
                reader->raster = NULL; /* Done with this raster */
            }

            run = reader->run;
        }
        reader->sync->unlock();
    }

    return NULL;
}



/*----------------------------------------------------------------------------
 * processRaster
 * Thread-safe, can be called directly from main thread or reader thread
 *----------------------------------------------------------------------------*/
void GeoRaster::processRaster(raster_t* raster, GeoRaster* obj)
{
    try
    {
        /* Open raster if first time reading from it */
        if (raster->dset == NULL)
        {
            raster->dset = (GDALDataset *)GDALOpenEx(raster->fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
            if (raster->dset == NULL)
                throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to open raster: %s:", raster->fileName.c_str());

            mlog(DEBUG, "Opened dataSet for %s", raster->fileName.c_str());

            /* Store information about raster */
            raster->cols = raster->dset->GetRasterXSize();
            raster->rows = raster->dset->GetRasterYSize();

            /* Get raster boundry box */
            double geot[6] = {0};
            CPLErr err = raster->dset->GetGeoTransform(geot);
            CHECK_GDALERR(err);
            raster->bbox.lon_min = geot[0];
            raster->bbox.lon_max = geot[0] + raster->cols * geot[1];
            raster->bbox.lat_max = geot[3];
            raster->bbox.lat_min = geot[3] + raster->rows * geot[5];

            raster->cellSize = geot[1];
            raster->radiusInPixels = radius2pixels(raster->cellSize, samplingRadius);

            /* Get raster block size */
            raster->band = raster->dset->GetRasterBand(1);
            CHECKPTR(raster->band);
            raster->band->GetBlockSize(&raster->xBlockSize, &raster->yBlockSize);
            // mlog(DEBUG, "Raster xBlockSize: %d, yBlockSize: %d", raster->xBlockSize, raster->yBlockSize);

            /* Get raster data type */
            raster->dataType = raster->band->GetRasterDataType();

            /* Get raster date as gps time in seconds */
            raster->gpsTime = static_cast<double>(obj->getRasterDate(raster->fileName) / 1000);
        }

        /*
         * Attempt to read raster only if it contains the point of interest.
         */
        if (!rasterContainsPoint(raster, raster->point))
            return;

        if (obj->sampleAlg == GRIORA_NearestNeighbour)
            obj->readPixel(raster);
        else
            resamplePixel(raster, obj);

        raster->sample.time = raster->gpsTime;
        raster->sampled = true;

        if (zonalStats)
            computeZonalStats(raster, obj);

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading raster: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * rasterIndexContainsPoint
 *----------------------------------------------------------------------------*/
inline bool GeoRaster::rasterIndexContainsPoint(OGRPoint& p)
{
    return (ris.dset &&
           (p.getX() >= ris.bbox.lon_min) && (p.getX() <= ris.bbox.lon_max) &&
           (p.getY() >= ris.bbox.lat_min) && (p.getY() <= ris.bbox.lat_max));
}


/*----------------------------------------------------------------------------
 * rasterContainsPoint
 *----------------------------------------------------------------------------*/
inline bool GeoRaster::rasterContainsPoint(raster_t *raster, OGRPoint& p)
{
    return (raster && raster->dset &&
           (p.getX() >= raster->bbox.lon_min) && (p.getX() <= raster->bbox.lon_max) &&
           (p.getY() >= raster->bbox.lat_min) && (p.getY() <= raster->bbox.lat_max));
}


/*----------------------------------------------------------------------------
 * findCachedRasterWithPoint
 *----------------------------------------------------------------------------*/
bool GeoRaster::findCachedRasterWithPoint(OGRPoint& p, GeoRaster::raster_t** raster)
{
    bool foundRaster = false;
    const char *key = rasterDict.first(raster);
    while (key != NULL)
    {
        assert(*raster);
        if (rasterContainsPoint(*raster, p))
        {
            foundRaster = true;
            break; /* Only one raster with this point in mosaic rasters */
        }
        key = rasterDict.next(raster);
    }
    return foundRaster;
}


/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GeoRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoRaster *lua_obj = (GeoRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->ris.rows);
        lua_pushinteger(L, lua_obj->ris.cols);
        num_ret += 2;

        /* Set Return Status */
        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting dimensions: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaBoundingBox - :bbox() --> (lon_min, lat_min, lon_max, lat_max)
 *----------------------------------------------------------------------------*/
int GeoRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoRaster *lua_obj = (GeoRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->ris.bbox.lon_min);
        lua_pushnumber(L, lua_obj->ris.bbox.lat_min);
        lua_pushnumber(L, lua_obj->ris.bbox.lon_max);
        lua_pushnumber(L, lua_obj->ris.bbox.lat_max);
        num_ret += 4;

        /* Set Return Status */
        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting bounding box: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaCellSize - :cell() --> cell size
 *----------------------------------------------------------------------------*/
int GeoRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoRaster *lua_obj = (GeoRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->ris.cellSize);
        num_ret += 1;

        /* Set Return Status */
        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting cell size: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}


/*----------------------------------------------------------------------------
 * getSampledRastersCount
 *----------------------------------------------------------------------------*/
int GeoRaster::getSampledRastersCount(void)
{
    raster_t *raster = NULL;
    int cnt = 0;

    /* Not all rasters in dictionary were sampled, find out how many were */
    const char *key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        if (raster->enabled && raster->sampled) cnt++;
        key = rasterDict.next(&raster);
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * luaSamples - :sample(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int GeoRaster::luaSamples(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    GeoRaster *lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (GeoRaster *)getLuaSelf(L, 1);

        lua_obj->samplingMutex.lock(); /* Serialize sampling on the same object */

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get samples */
        int sampledRasters = lua_obj->sample(lon, lat);
        if (sampledRasters > 0)
        {
            raster_t   *raster = NULL;
            const char *key    = NULL;

            /* Create return table */
            lua_createtable(L, sampledRasters, 0);

            /* Populate the return table */
            int i = 0;
            key = lua_obj->rasterDict.first(&raster);
            while (key != NULL)
            {
                assert(raster);
                if (raster->enabled && raster->sampled)
                {
                    lua_createtable(L, 0, 2);
                    LuaEngine::setAttrStr(L, "file", raster->fileName.c_str());

                    if (lua_obj->zonalStats) /* Include all zonal stats */
                    {
                        LuaEngine::setAttrNum(L, "mad",   raster->sample.stats.mad);
                        LuaEngine::setAttrNum(L, "stdev", raster->sample.stats.stdev);
                        LuaEngine::setAttrNum(L, "median",raster->sample.stats.median);
                        LuaEngine::setAttrNum(L, "mean",  raster->sample.stats.mean);
                        LuaEngine::setAttrNum(L, "max",   raster->sample.stats.max);
                        LuaEngine::setAttrNum(L, "min",   raster->sample.stats.min);
                        LuaEngine::setAttrNum(L, "count", raster->sample.stats.count);
                    }

                    LuaEngine::setAttrNum(L, "value", raster->sample.value);
                    lua_rawseti(L, -2, ++i);
                }
                key = lua_obj->rasterDict.next(&raster);
            }

            num_ret++;
            status = true;
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }

    if (lua_obj) lua_obj->samplingMutex.unlock();

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}


