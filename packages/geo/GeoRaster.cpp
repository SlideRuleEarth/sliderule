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
#include "core.h"
#include "GeoRaster.h"

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

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoRaster::FLAGS_RASTER_TAG   = "Fmask";
const char* GeoRaster::SAMPLES_RASTER_TAG = "Dem";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * getSamples
 *----------------------------------------------------------------------------*/
void GeoRaster::getSamples(double lon, double lat, double height, int64_t gps, List<sample_t>& slist, void* param)
{
    std::ignore = gps;
    std::ignore = param;

    samplingMutex.lock();
    try
    {
        slist.clear();

        OGRPoint point(lon, lat, height);

        /* Get sample, if none found, return */
        _raster.point   = point;
        _raster.sampled = false;
        _raster.enabled = true;
        bzero(&_raster.sample, sizeof(sample_t));
        readPOI(&_raster);

        if(_raster.sampled)
        {
            /* Update dictionary of used raster files */
            _raster.sample.fileId = fileDictAdd(_raster.fileName);
            _raster.sample.flags  = 0;
            slist.add(_raster.sample);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
        samplingMutex.unlock();
        throw;  // rethrow exception
    }
    samplingMutex.unlock();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoRaster::~GeoRaster(void)
{
}


/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoRaster::GeoRaster(lua_State *L, GeoParms* _parms, const char* _fileName, double _gpsTime):
    RasterObject(L, _parms)
{
    /* Initialize Class Data Members */
    _raster.fileName = _fileName;
    _raster.gpsTime  = _gpsTime;

    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "dim", luaDimensions);
    LuaEngine::setAttrFunc(L, "bbox", luaBoundingBox);
    LuaEngine::setAttrFunc(L, "cell", luaCellSize);

    /* Establish Credentials */
    #ifdef __aws__
    if(_parms->asset)
    {
        const char* identity = _parms->asset->getIdentity();
        CredentialStore::Credential credentials = CredentialStore::get(identity);
        if(credentials.provided)
        {
            const char* path = _parms->asset->getPath();
            VSISetPathSpecificOption(path, "AWS_ACCESS_KEY_ID", credentials.accessKeyId);
            VSISetPathSpecificOption(path, "AWS_SECRET_ACCESS_KEY", credentials.secretAccessKey);
            VSISetPathSpecificOption(path, "AWS_SESSION_TOKEN", credentials.sessionToken);
        }
    }
    #endif
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
 * radius2pixels
 *----------------------------------------------------------------------------*/
int GeoRaster::radius2pixels(double cellSize, int _radius)
{
    /*
     * Code supports only rasters with units in meters (cellSize and radius must be in meters).
     */
    int csize = static_cast<int>(cellSize);

    if (_radius == 0) return 0;
    if (csize == 0) csize = 1;

    int radiusInMeters = ((_radius + csize - 1) / csize) * csize; // Round up to multiples of cell size
    int radiusInPixels = radiusInMeters / csize;
    return radiusInPixels;
}

/*----------------------------------------------------------------------------
 * openRaster
 *----------------------------------------------------------------------------*/
void GeoRaster::openRaster(const char* _fileName, double _gpsTime)
{
    if(_raster.dset == NULL)
    {
        if(strlen(_fileName) > 0)
            _raster.fileName = _fileName;

        if(_gpsTime > 0)
            _raster.gpsTime = _gpsTime;

        openRaster(&_raster);
    }
    else
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to opened raster, already opened with file: %s", _raster.fileName.c_str());
}


/*----------------------------------------------------------------------------
 * openRaster
 *----------------------------------------------------------------------------*/
void GeoRaster::openRaster(Raster* raster)
{
    if(raster->dset == NULL)
    {
        if(raster->gpsTime == 0.0)
            raster->gpsTime = getRasterDate();

        raster->dset = (GDALDataset*)GDALOpenEx(raster->fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
        if(raster->dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to opened raster: %s:", raster->fileName.c_str());

        mlog(DEBUG, "Opened %s", raster->fileName.c_str());

        /* Store information about raster */
        raster->cols = raster->dset->GetRasterXSize();
        raster->rows = raster->dset->GetRasterYSize();

        /* Get raster boundry box */
        double geot[6] = {0};
        CPLErr err     = raster->dset->GetGeoTransform(geot);
        CHECK_GDALERR(err);
        raster->bbox.lon_min = geot[0];
        raster->bbox.lon_max = geot[0] + raster->cols * geot[1];
        raster->bbox.lat_max = geot[3];
        raster->bbox.lat_min = geot[3] + raster->rows * geot[5];

        raster->cellSize       = geot[1];
        raster->radiusInPixels = radius2pixels(raster->cellSize, parms->sampling_radius);

        /* Limit maximum sampling radius */
        if(raster->radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,
                                   "Sampling radius is too big: %d: max allowed %d meters",
                                   parms->sampling_radius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(raster->cellSize));
        }

        /* Get raster block size */
        raster->band = raster->dset->GetRasterBand(1);
        CHECKPTR(raster->band);
        raster->band->GetBlockSize(&raster->xBlockSize, &raster->yBlockSize);

        /* Get raster data type */
        raster->dataType = raster->band->GetRasterDataType();

        /* Create coordinates transform for raster */
        createTransform(raster->cord, raster->dset);
    }
}


/*----------------------------------------------------------------------------
 * readPOI
 * Thread-safe, can be called directly from main thread or reader thread
 *----------------------------------------------------------------------------*/
void GeoRaster::readPOI(Raster* raster)
{
    try
    {
        if (raster->dset == NULL)
            openRaster(raster);

        double z0 = raster->point.getZ();
        mlog(DEBUG, "Before transform x,y,z: (%.4lf, %.4lf, %.4lf)", raster->point.getX(), raster->point.getY(), raster->point.getZ());

        if(raster->point.transform(raster->cord.transf) != OGRERR_NONE)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Coordinates Transform failed for x,y,z (%lf, %lf, %lf)", raster->point.getX(), raster->point.getY(), raster->point.getZ());

        mlog(DEBUG, "After  transform x,y,z: (%.4lf, %.4lf, %.4lf)", raster->point.getX(), raster->point.getY(), raster->point.getZ());
        raster->verticalShift = z0 - raster->point.getZ();

        /*
         * Attempt to read raster only if it contains the point of interest.
         */
        if (!raster->containsPoint(raster->point))
            return;

        if (parms->sampling_algo == GRIORA_NearestNeighbour)
            readPixel(raster);
        else
            resamplePixel(raster);

        raster->sampled = true;
        raster->useTime = TimeLib::gpstime();
        raster->sample.time = raster->gpsTime;

        if (parms->zonal_stats)
            computeZonalStats(raster);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading raster: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
int64_t GeoRaster::getRasterDate(void)
{
    return 0;
}


/*----------------------------------------------------------------------------
 * RasterIoWithRetry
 *----------------------------------------------------------------------------*/
void GeoRaster::readRasterWithRetry(GDALRasterBand *band, int col, int row, int colSize, int rowSize, void *data, int dataColSize, int dataRowSize, GDALRasterIOExtraArg *args)
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



/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * readPixel
 *----------------------------------------------------------------------------*/
void GeoRaster::readPixel(Raster *raster)
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

            case GDT_Int64:
            {
                int64_t *p = (int64_t *)data;
                raster->sample.value = (double)p[offset];
            }
            break;

            case GDT_UInt64:
            {
                uint64_t *p = (uint64_t *)data;
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
                 * Complex numbers are supported but not needed at this point.
                 */
                block->DropLock();
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unsuported dataType in raster: %s:", raster->fileName.c_str());
        }

        /* Done reading, release block lock */
        block->DropLock();

        if(nodataCheck(raster) && raster->dataIsElevation)
        {
            raster->sample.value += raster->verticalShift;
        }

        // mlog(DEBUG, "Value: %.2lf, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u",
        //      raster->sample.value, col, row, xblk, yblk, _col, _row, offset);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading from raster: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * createTransform
 *----------------------------------------------------------------------------*/
void GeoRaster::createTransform(CoordTransform& cord, GDALDataset* dset)
{
    CHECKPTR(dset);
    cord.clear(true);

    OGRErr ogrerr = cord.source.importFromEPSG(SLIDERULE_EPSG);
    CHECK_GDALERR(ogrerr);

    const char* projref = dset->GetProjectionRef();
    CHECKPTR(projref);
    // mlog(DEBUG, "%s", projref);

    ogrerr = cord.target.importFromWkt(projref);
    CHECK_GDALERR(ogrerr);

    overrideTargetCRS(cord.target);

    /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
    cord.target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    cord.source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    cord.transf = OGRCreateCoordinateTransformation(&cord.source, &cord.target);
    if(cord.transf == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinates transform");
}


/*----------------------------------------------------------------------------
 * overrideTargetCRS
 *----------------------------------------------------------------------------*/
void GeoRaster::overrideTargetCRS(OGRSpatialReference& target)
{
    std::ignore = target;
}


/*----------------------------------------------------------------------------
 * setCRSfromWkt
 *----------------------------------------------------------------------------*/
void GeoRaster::setCRSfromWkt(OGRSpatialReference& sref, const std::string& wkt)
{
    // mlog(DEBUG, "%s", wkt.c_str());
    OGRErr ogrerr = sref.importFromWkt(wkt.c_str());
    CHECK_GDALERR(ogrerr);
}


/*----------------------------------------------------------------------------
 * containsWindow
 *----------------------------------------------------------------------------*/
bool GeoRaster::containsWindow(int col, int row, int maxCol, int maxRow, int windowSize )
{
    if (col < 0 || row < 0)
        return false;

    if ((col + windowSize >= maxCol) || (row + windowSize >= maxRow))
        return false;

    return true;
}


/*----------------------------------------------------------------------------
 * Raster constructor
 *----------------------------------------------------------------------------*/
GeoRaster::Raster::Raster(const char* _fileName, double _gpsTime)
{
    fileName = _fileName;
    enabled = false;
    sampled = false;
    gpsTime = _gpsTime;
    useTime = 0;
    bzero(&sample, sizeof(sample_t));
}


/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
void GeoRaster::GeoDataSet::clear(bool close)
{
    cord.clear(close);
    fileName.clear();
    if (close && dset) GDALClose((GDALDatasetH)dset);
    dset = NULL;
    band = NULL;
    dataType = GDT_Unknown;
    dataIsElevation = false;
    rows = 0;
    cols = 0;
    cellSize = 0;
    bzero(&bbox, sizeof(bbox_t));
    verticalShift = 0;
    xBlockSize = 0;
    yBlockSize = 0;
    radiusInPixels = 0;
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
void GeoRaster::CoordTransform::clear(bool close)
{
    if (close && transf) OGRCoordinateTransformation::DestroyCT(transf);
    transf = NULL;
    source.Clear();
    target.Clear();
}

/*----------------------------------------------------------------------------
 * resamplePixel
 *----------------------------------------------------------------------------*/
void GeoRaster::resamplePixel(Raster* raster)
{
    try
    {
        int col = static_cast<int>(floor((raster->point.getX() - raster->bbox.lon_min) / raster->cellSize));
        int row = static_cast<int>(floor((raster->bbox.lat_max - raster->point.getY()) / raster->cellSize));

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
            windowSize = raster->radiusInPixels * 2 + 1;  // Odd window size around pixel
            offset = raster->radiusInPixels;
        }

        int _col = col - offset;
        int _row = row - offset;

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = parms->sampling_algo;
        double  rbuf[1] = {INVALID_SAMPLE_VALUE};

        bool validWindow = containsWindow(_col, _row, raster->cols, raster->rows, windowSize);
        if (validWindow)
        {
            readRasterWithRetry(raster->band, _col, _row, windowSize, windowSize, rbuf, 1, 1, &args);

            raster->sample.value = rbuf[0];
            if(nodataCheck(raster) && raster->dataIsElevation)
            {
                raster->sample.value += raster->verticalShift;
            }
        }
        else
        {
            /* At least return pixel value if unable to resample raster and/or raster index data set */
            readPixel(raster);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error resampling pixel: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * parms->zonal_stats
 *----------------------------------------------------------------------------*/
void GeoRaster::computeZonalStats(Raster *raster)
{
    double *samplesArray = NULL;

    try
    {
        int col = static_cast<int32_t>(floor((raster->point.getX() - raster->bbox.lon_min) / raster->cellSize));
        int row = static_cast<int32_t>(floor((raster->bbox.lat_max - raster->point.getY()) / raster->cellSize));

        int radiusInPixels = raster->radiusInPixels;
        int windowSize = radiusInPixels * 2 + 1; // Odd window size around pixel

        int _col = col - radiusInPixels;
        int _row = row - radiusInPixels;

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = parms->sampling_algo;
        samplesArray = new double[windowSize*windowSize];
        CHECKPTR(samplesArray);

        double noDataValue = raster->band->GetNoDataValue();
        bool validWindow = containsWindow(_col, _row, raster->cols, raster->rows, windowSize);
        if (validWindow)
        {
            readRasterWithRetry(raster->band, _col, _row, windowSize, windowSize, samplesArray, windowSize, windowSize, &args);

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

                    if(raster->dataIsElevation)
                        value += raster->verticalShift;

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
 * nodataCheck
 *----------------------------------------------------------------------------*/
bool GeoRaster::nodataCheck(Raster* raster)
{
    /*
     * Replace nodata with NAN
     */
    const double a = raster->band->GetNoDataValue();
    const double b = raster->sample.value;
    const double epsilon = 0.000001;

    if(std::fabs(a-b) < epsilon)
    {
        raster->sample.value = std::nanf("");
        return false;
    }

    return true;
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
        lua_pushinteger(L, lua_obj->_raster.rows);
        lua_pushinteger(L, lua_obj->_raster.cols);
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
        lua_pushnumber(L, lua_obj->_raster.bbox.lon_min);
        lua_pushnumber(L, lua_obj->_raster.bbox.lat_min);
        lua_pushnumber(L, lua_obj->_raster.bbox.lon_max);
        lua_pushnumber(L, lua_obj->_raster.bbox.lat_max);
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
        lua_pushnumber(L, lua_obj->_raster.cellSize);
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
