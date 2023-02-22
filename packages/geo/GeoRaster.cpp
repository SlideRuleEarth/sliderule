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

#include <uuid/uuid.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdalwarper.h>
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
    GeoParms* _parms = NULL;
    try
    {
        /* Get Parameters */
        _parms = (GeoParms*)getLuaObject(L, 1, GeoParms::OBJECT_TYPE);

        /* Get Factory */
        factory_t _create = NULL;
        factoryMut.lock();
        {
            factories.find(_parms->asset_name, &_create);
        }
        factoryMut.unlock();

        /* Check Factory */
        if(_create == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to find registered raster for %s", _parms->asset_name);

        /* Create Raster */
        GeoRaster* _raster = _create(L, _parms);
        if(_raster == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create raster of type: %s", _parms->asset_name);

        /* Return Object */
        return createLuaObject(L, _raster);
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
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
int GeoRaster::sample(double lon, double lat, List<sample_t>& slist, void* param)
{
    std::ignore = param;
    slist.clear();

    samplingMutex.lock(); /* Serialize sampling on the same object */

    try
    {
        /* Get samples */
        if (sample(lon, lat) > 0)
        {
            Raster *raster = NULL;
            const char *key = rasterDict.first(&raster);
            while (key != NULL)
            {
                assert(raster);
                if (!raster->isAuxuliary && raster->enabled && raster->sampled)
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
                reader->sync->signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
            }
            reader->sync->unlock();

            delete reader->thread;  /* delte thread waits on thread to join */
            delete reader->sync;
        }
    }

    delete [] rasterRreader;

    /* Close all rasters */
    Raster* raster = NULL;
    const char* key  = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        delete raster;
        raster = NULL;
        key = rasterDict.next(&raster);
    }

    if (rastersList) delete rastersList;

    /* Release GeoParms LuaObject */
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
int GeoRaster::sample(double lon, double lat)
{
    invalidateCache();

    /* Initial call, open raster index data set if not already opened */
    if (geoIndex.dset == NULL)
        openGeoIndex(lon, lat);

    OGRPoint p(lon, lat);
    transformCRS(p);

    /* If point is not in current geoindex, find a new one */
    if (!geoIndex.containsPoint(p))
    {
        openGeoIndex(lon, lat);

        /* Check against newly opened geoindex */
        if (!geoIndex.containsPoint(p))
            return 0;
    }

    if (findCachedRasters(p))
    {
        /* Rasters alredy in cache have been previously filtered  */
        sampleRasters();
    }
    else
    {
        if(findRasters(p) && filterRasters())
        {
            updateCache(p);
            sampleRasters();
        }
    }

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


/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoRaster::GeoRaster(lua_State *L, GeoParms* _parms):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    parms(_parms)
{
    /* Initialize Class Data Members */
    rastersList = new Ordering<raster_info_t>;
    rasterRreader = new reader_t[MAX_READER_THREADS];
    bzero(rasterRreader, sizeof(reader_t)*MAX_READER_THREADS);
    readerCount = 0;
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
    if (csize == 0) csize = 1;

    int radiusInMeters = ((_radius + csize - 1) / csize) * csize; // Round up to multiples of cell size
    int radiusInPixels = radiusInMeters / csize;
    return radiusInPixels;
}


/*----------------------------------------------------------------------------
 * processRaster
 * Thread-safe, can be called directly from main thread or reader thread
 *----------------------------------------------------------------------------*/
void GeoRaster::processRaster(Raster* raster)
{
    try
    {
        /* Open raster if first time reading from it */
        if (raster->dset == NULL)
        {
            raster->dset = (GDALDataset *)GDALOpenEx(raster->fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
            if (raster->dset == NULL)
                throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to opened index raster: %s:", raster->fileName.c_str());

            mlog(DEBUG, "Opened %s", raster->fileName.c_str());

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
            raster->radiusInPixels = radius2pixels(raster->cellSize, parms->sampling_radius);

            /* Limit maximum sampling radius */
            if (raster->radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR,
                                       "Sampling radius is too big: %d: max allowed %d meters",
                                       parms->sampling_radius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(geoIndex.cellSize));
            }

            /* Get raster block size */
            raster->band = raster->dset->GetRasterBand(1);
            CHECKPTR(raster->band);
            raster->band->GetBlockSize(&raster->xBlockSize, &raster->yBlockSize);

            /* Get raster data type */
            raster->dataType = raster->band->GetRasterDataType();

            /* Get raster spatial reference */
            raster->sref = const_cast<OGRSpatialReference*>(raster->dset->GetSpatialRef());
            CHECKPTR(raster->sref);
        }

        /* Make sure point of interest and raster are in the same CRS */
        OGRSpatialReference *psref = raster->point.getSpatialReference();
        CHECKPTR(psref);
        if (!raster->sref->IsSame(psref))
        {
            if (cord.source.IsSame(psref) && !psref->IsProjected())
            {
                /* Transform point to raster's CRS */
                double lon = raster->point.getX();
                double lat = raster->point.getY();
                if (raster->point.transform(cord.transf) != OGRERR_NONE)
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Coordinates Transform failed for (%.2lf, %.2lf)", lon, lat);
            }
            else throw RunTimeException(CRITICAL, RTE_ERROR, "POI and raster CRS are not the same");
        }

        /*
         * Attempt to read raster only if it contains the point of interest.
         */
        if (!containsPoint(raster, raster->point))
            return;

        if (parms->sampling_algo == GRIORA_NearestNeighbour)
            readPixel(raster);
        else
            resamplePixel(raster);

        raster->sample.time = raster->gpsTime;
        raster->sampled = true;

        if (parms->zonal_stats)
            computeZonalStats(raster);

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading raster: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * sampleRasters
 *----------------------------------------------------------------------------*/
void GeoRaster::sampleRasters(void)
{
    /* Create additional reader threads if needed */
    createThreads();

    /* For each raster which is marked to be sampled, give it to the reader thread */
    int signaledReaders = 0;
    Raster *raster = NULL;
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
                reader->sync->signal(DATA_TO_SAMPLE, Cond::NOTIFY_ONE);
                signaledReaders++;
            }
            reader->sync->unlock();
        }
        key = rasterDict.next(&raster);
    }

    /* Did not signal any reader threads, don't wait */
    if (signaledReaders == 0) return;

    /* Wait and update each raster when it is done */
    for (int j=0; j<signaledReaders; j++)
    {
        /* Wait for reader to finish sampling */
        reader_t *reader = &rasterRreader[j];
        reader->sync->lock();
        {
            while (reader->raster != NULL)
                reader->sync->wait(DATA_SAMPLED, SYS_TIMEOUT);
        }
        reader->sync->unlock();
    }

    /* Update dictionary of used raster files and auxualiary data */
    key = rasterDict.first(&raster);
    while (key != NULL)
    {
        if (!raster->isAuxuliary && raster->enabled && raster->sampled)
        {
            std::string fileName = raster->fileName.substr(strlen("/vsis3/"));
            raster->sample.fileId = fileDictAdd(fileName);
            raster->sample.flags = raster->getPeerValue();
        }
        key = rasterDict.next(&raster);
    }
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
 * getPeerValue
 *----------------------------------------------------------------------------*/
uint32_t GeoRaster::Raster::getPeerValue(void)
{
    uint32_t peerValue = 0;

    if (peerRaster && peerRaster->enabled && peerRaster->sampled)
    {
        peerValue = static_cast<uint32_t>(peerRaster->sample.value);
    }

    return peerValue;
}

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

        // mlog(DEBUG, "Value: %.2lf, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u",
        //      raster->sample.value, col, row, xblk, yblk, _col, _row, offset);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading from raster: %s", e.what());
    }
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
 * readGeoIndexData
 *----------------------------------------------------------------------------*/
bool GeoRaster::readGeoIndexData(OGRPoint *point, int srcWindowSize, int srcOffset,
                                 void *data, int dstWindowSize, GDALRasterIOExtraArg *args)
{
    std::ignore = point;
    std::ignore = srcWindowSize;
    std::ignore = srcOffset;
    std::ignore = data;
    std::ignore = dstWindowSize;
    std::ignore = args;
    return false;
}

/*----------------------------------------------------------------------------
 * containsPoint
 *----------------------------------------------------------------------------*/
inline bool GeoRaster::GeoIndex::containsPoint(OGRPoint& p)
{
    return (dset &&
            (p.getX() >= bbox.lon_min) && (p.getX() <= bbox.lon_max) &&
            (p.getY() >= bbox.lat_min) && (p.getY() <= bbox.lat_max));
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
void GeoRaster::GeoIndex::clear(bool close)
{
    if (close && dset) GDALClose((GDALDatasetH)dset);
    dset = NULL;
    fileName.clear();
    rows = 0;
    cols = 0;
    cellSize = 0;
    bzero(&bbox, sizeof(bbox_t));
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
void GeoRaster::resamplePixel(Raster *raster)
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
        }
        else
        {
            validWindow = readGeoIndexData(&raster->point, windowSize, offset, rbuf, 1, &args);
        }

        if(validWindow)
        {
            raster->sample.value = rbuf[0];
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
        }
        else
        {
            validWindow = readGeoIndexData(&raster->point, windowSize, radiusInPixels, samplesArray, windowSize, &args);
        }

        if(validWindow)
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
 * clear
 *----------------------------------------------------------------------------*/
void GeoRaster::Raster::clear(bool close)
{
    if(close && dset) GDALClose((GDALDatasetH)dset);
    dset = NULL;
    band = NULL;
    sref = NULL;
    peerRaster = NULL;
    isAuxuliary = false;
    enabled = false;
    sampled = false;
    fileName.clear();
    dataType = GDT_Unknown;

    rows = 0;
    cols = 0;
    bzero(&bbox, sizeof(bbox_t));
    cellSize = 0;
    xBlockSize = 0;
    yBlockSize = 0;
    radiusInPixels = 0;
    gpsTime = 0;
    point.empty();
    bzero(&sample, sizeof(sample_t));
}

/*----------------------------------------------------------------------------
 * invaldiateCache
 *----------------------------------------------------------------------------*/
void GeoRaster::invalidateCache(void)
{
    Raster *raster = NULL;
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
 * updateCache
 *----------------------------------------------------------------------------*/
void GeoRaster::updateCache(OGRPoint& p)
{
    if (rastersList->length() == 0)
        return;

    const char *key  = NULL;
    Raster *raster = NULL;

    /* Check new tif file list against rasters in dictionary */
    Ordering<raster_info_t>::Iterator raster_iter(*rastersList);
    for (int i = 0; i < raster_iter.length; i++)
    {
        const raster_info_t &rinfo   = raster_iter[i].value;
        const char* rasterFile = rinfo.fileName.c_str();
        const char* auxFile    = rinfo.auxFileName.c_str();;

        /* For now code supports only one auxiliary raster */
        const char* keys[2] = {rasterFile, auxFile};
        const int CNT = parms->auxiliary_files ? 2 : 1;

        Raster* demRaster = NULL;
        Raster* auxRaster = NULL;

        for (int j = 0; j < CNT; j++)
        {
            key = keys[j];
            if(strlen(key) == 0) break;

            if (rasterDict.find(key, &raster))
            {
                /* Update point to be sampled, mark raster enabled for next sampling */
                assert(raster);
                raster->enabled = true;
                raster->point = p;
            }
            else
            {
                /* Create new raster and add to dictionary */
                raster = new Raster;
                assert(raster);

                if(j == 0) {demRaster = raster; raster->isAuxuliary = false;}
                if(j == 1) {auxRaster = raster; raster->isAuxuliary = true; }

                raster->enabled = true;
                raster->point = p;
                raster->sample.value = INVALID_SAMPLE_VALUE;
                raster->fileName = key;
                raster->gpsTime = static_cast<double>(TimeLib::gmt2gpstime(rinfo.gmtDate) / 1000);
                rasterDict.add(key, raster);
            }
        }

        /* Set up pointers between dem and auxiliary rasters */
        if(demRaster && auxRaster)
        {
            demRaster->peerRaster = auxRaster;
            auxRaster->peerRaster = demRaster;
        }
    }

    /* Maintain cache from getting too big */
    key = rasterDict.first(&raster);
    while (key != NULL)
    {
        if (rasterDict.length() <= MAX_CACHED_RASTERS)
            break;

        assert(raster);
        if (raster->enabled)
        {
            /* If raster has a peer, remove them both */
            if(raster->peerRaster)
            {
                const char* peerKey = raster->peerRaster->fileName.c_str();
                if (rasterDict.find(peerKey, &raster->peerRaster))
                {
                    rasterDict.remove(peerKey);
                    delete raster->peerRaster;
                }
            }

            rasterDict.remove(key);
            delete raster;
        }
        key = rasterDict.next(&raster);
    }
}

/*----------------------------------------------------------------------------
 * filterRasters
 *----------------------------------------------------------------------------*/
bool GeoRaster::filterRasters(void)
{
    /* Temporal and URL filters - remove unwanted rasters from the list */
    if(parms->url_substring || parms->filter_time )
    {
        Ordering<raster_info_t>::Iterator raster_iter(*rastersList);
        for(int i = 0; i < raster_iter.length; i++)
        {
            const raster_info_t& rinfo = raster_iter[i].value;
            bool removeRaster    = false;

            /* URL filter */
            if(parms->url_substring)
            {
                if(rinfo.fileName.find(parms->url_substring) == std::string::npos)
                    removeRaster = true;
            }

            /* Temporal filter */
            if(!removeRaster && parms->filter_time)
            {
                if(!TimeLib::gmtinrange(rinfo.gmtDate, parms->start_time, parms->stop_time))
                    removeRaster = true;
            }

            if(removeRaster)
            {
                rastersList->remove(raster_iter[i].key);
            }
        }
    }

    /* Closest time filter */
    if(parms->filter_closest_time)
    {
        int64_t closestGps = TimeLib::gmt2gpstime(parms->closest_time);
        int64_t minDelta   = abs(std::numeric_limits<int64_t>::max() - closestGps);
        unsigned long closest_key = INVALID_KEY;

        Ordering<raster_info_t>::Iterator raster_iter(*rastersList);
        for(int i = 0; i < raster_iter.length; i++)
        {
            int64_t gps   = TimeLib::gmt2gpstime(raster_iter[i].value.gmtDate);
            int64_t delta = abs(closestGps - gps);

            if(delta < minDelta)
            {
                /* Keep track of raster with closest time */
                minDelta = delta;
                closest_key  = raster_iter[i].key;
            }
        }

        if(closest_key != INVALID_KEY)
        {
            /* Return list with only one (closest) raster */
            raster_info_t ri = rastersList->get(closest_key);
            rastersList->clear();
            rastersList->add(0, ri);
        }
    }

    return (rastersList->length() > 0);
}

/*----------------------------------------------------------------------------
 * createThreads
 *----------------------------------------------------------------------------*/
void GeoRaster::createThreads(void)
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
            reader->sync = new Cond(NUM_SYNC_SIGNALS);
            reader->obj = this;
            reader->thread = new Thread(readingThread, reader);
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
                reader->sync->wait(DATA_TO_SAMPLE, SYS_TIMEOUT);

            if(reader->raster != NULL)
            {
                reader->obj->processRaster(reader->raster);
                reader->raster = NULL; /* Done with this raster */
                reader->sync->signal(DATA_SAMPLED, Cond::NOTIFY_ONE);
            }

            run = reader->run;
        }
        reader->sync->unlock();
    }

    return NULL;
}


/*----------------------------------------------------------------------------
 * getSampledRastersCount
 *----------------------------------------------------------------------------*/
int GeoRaster::getSampledRastersCount(void)
{
    Raster *raster = NULL;
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
 * fileDictAdd
 *----------------------------------------------------------------------------*/
uint64_t GeoRaster::fileDictAdd(const std::string& fileName)
{
    uint64_t id;

    if(!fileDict.find(fileName.c_str(), &id))
    {
        id = (parms->key_space << 32) | fileDict.length();
        fileDict.add(fileName.c_str(), id);
    }

    return id;
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
        lua_pushinteger(L, lua_obj->geoIndex.rows);
        lua_pushinteger(L, lua_obj->geoIndex.cols);
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
        lua_pushnumber(L, lua_obj->geoIndex.bbox.lon_min);
        lua_pushnumber(L, lua_obj->geoIndex.bbox.lat_min);
        lua_pushnumber(L, lua_obj->geoIndex.bbox.lon_max);
        lua_pushnumber(L, lua_obj->geoIndex.bbox.lat_max);
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
        lua_pushnumber(L, lua_obj->geoIndex.cellSize);
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
            Raster   *raster = NULL;
            const char *key    = NULL;

            /* Create return table */
            lua_createtable(L, sampledRasters, 0);

            /* Populate the return table */
            int i = 0;
            key = lua_obj->rasterDict.first(&raster);
            while (key != NULL)
            {
                assert(raster);
                if (!raster->isAuxuliary && raster->enabled && raster->sampled)
                {
                    std::string fileName = raster->fileName.substr(strlen("/vsis3/"));

                    lua_createtable(L, 0, 2);
                    LuaEngine::setAttrStr(L, "file", fileName.c_str());

                    if (lua_obj->parms->zonal_stats) /* Include all zonal stats */
                    {
                        LuaEngine::setAttrNum(L, "mad",   raster->sample.stats.mad);
                        LuaEngine::setAttrNum(L, "stdev", raster->sample.stats.stdev);
                        LuaEngine::setAttrNum(L, "median",raster->sample.stats.median);
                        LuaEngine::setAttrNum(L, "mean",  raster->sample.stats.mean);
                        LuaEngine::setAttrNum(L, "max",   raster->sample.stats.max);
                        LuaEngine::setAttrNum(L, "min",   raster->sample.stats.min);
                        LuaEngine::setAttrNum(L, "count", raster->sample.stats.count);
                    }

                    if (lua_obj->parms->auxiliary_files) /* Include auxuliary raster value (flags) */
                    {
                        LuaEngine::setAttrNum(L, "flags", raster->sample.flags);
                    }

                    LuaEngine::setAttrInt(L, "fileid",raster->sample.fileId);
                    LuaEngine::setAttrNum(L, "time",  raster->sample.time);
                    LuaEngine::setAttrNum(L, "value", raster->sample.value);
                    lua_rawseti(L, -2, ++i);
                }
                key = lua_obj->rasterDict.next(&raster);
            }

            num_ret++;
            status = true;
        } else mlog(DEBUG, "No samples read for (%.2lf, %.2lf)", lon, lat);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to read samples: %s", e.what());
    }

    if (lua_obj) lua_obj->samplingMutex.unlock();

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}
