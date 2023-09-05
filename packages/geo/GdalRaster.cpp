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

#include "RasterSample.h"
#include "GdalRaster.h"

#ifdef __aws__
#include "aws.h"
#endif

#include <algorithm>
#include <uuid/uuid.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GdalRaster::GdalRaster(GeoParms* _parms, const std::string& _fileName, double _gpsTime, bool _dataIsElevation, overrideCRS_t cb) :
   parms      (_parms),
  _sampled    (false),
   gpsTime    (_gpsTime),
   sample     (),
   subset     (),
   transf     (NULL),
   overrideCRS(cb),
   fileName   (_fileName),
   dset       (NULL),
   band       (NULL),
   dataIsElevation(_dataIsElevation),
   rows       (0),
   cols       (0),
   cellSize   (0),
   bbox       (),
   radiusInPixels(0)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GdalRaster::~GdalRaster(void)
{
    if(dset) GDALClose((GDALDatasetH)dset);
    if(transf) OGRCoordinateTransformation::DestroyCT(transf);
}

/*----------------------------------------------------------------------------
 * open
 *----------------------------------------------------------------------------*/
void GdalRaster::open(void)
{
    if(dset)
    {
        mlog(DEBUG, "Raster already opened: %s", fileName.c_str());
        return;
    }

    dset = (GDALDataset*)GDALOpenEx(fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
    if(dset == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to opened raster: %s:", fileName.c_str());

    mlog(DEBUG, "Opened %s", fileName.c_str());

    /* Store information about raster */
    cols = dset->GetRasterXSize();
    rows = dset->GetRasterYSize();

    double geoTransform[6];
    CPLErr err = dset->GetGeoTransform(geoTransform);
    CHECK_GDALERR(err);
    if(!GDALInvGeoTransform(geoTransform, invGeoTrans))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to get inverted geo transform: %s:", fileName.c_str());
    }

    /* Get raster boundry box */
    CHECK_GDALERR(err);
    bbox.lon_min = geoTransform[0];
    bbox.lon_max = geoTransform[0] + cols * geoTransform[1];
    bbox.lat_max = geoTransform[3];
    bbox.lat_min = geoTransform[3] + rows * geoTransform[5];

    cellSize       = geoTransform[1];
    radiusInPixels = radius2pixels(parms->sampling_radius);

    /* Limit maximum sampling radius */
    if(radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR,
                               "Sampling radius is too big: %d: max allowed %d meters",
                               parms->sampling_radius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(cellSize));
    }

    band = dset->GetRasterBand(1);
    CHECKPTR(band);

    /* Create coordinates transform for raster */
    createTransform();
}

/*----------------------------------------------------------------------------
 * samplePOI
 *----------------------------------------------------------------------------*/
void GdalRaster::samplePOI(OGRPoint* poi)
{
    try
    {
        if(dset == NULL)
            open();

        _sampled = false;
        sample.clear();

        double z = poi->getZ();
        mlog(DEBUG, "Before transform x,y,z: (%.4lf, %.4lf, %.4lf)", poi->getX(), poi->getY(), poi->getZ());
        if(poi->transform(transf) != OGRERR_NONE)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Coordinates Transform failed for x,y,z (%lf, %lf, %lf)", poi->getX(), poi->getY(), poi->getZ());
        mlog(DEBUG, "After  transform x,y,z: (%.4lf, %.4lf, %.4lf)", poi->getX(), poi->getY(), poi->getZ());
        verticalShift = z - poi->getZ();

        /*
         * Attempt to read raster only if it contains the point of interest.
         */
        if((poi->getX() >= bbox.lon_min) && (poi->getX() <= bbox.lon_max) &&
           (poi->getY() >= bbox.lat_min) && (poi->getY() <= bbox.lat_max))
        {
            if(parms->sampling_algo == GRIORA_NearestNeighbour)
                readPixel(poi);
            else
                resamplePixel(poi);

            if(parms->zonal_stats)
                computeZonalStats(poi);

            _sampled    = true;
            sample.time = gpsTime;
        }
    }
    catch (const RunTimeException &e)
    {
        _sampled = false;
        mlog(e.level(), "Error sampling: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * subsetAOI
 *----------------------------------------------------------------------------*/
void GdalRaster::subsetAOI(OGRPolygon* poly)
{
    /*
     * Notes on extent format:
     * gdalwarp uses '-te xmin ymin xmax ymax'
     * gdalbuildvrt uses '-te xmin ymin xmax ymax'
     * gdal_translate uses '-projwin ulx uly lrx lry' or '-projwin xmin ymax xmax ymin'
     *
     * This function uses 'xmin ymin xmax ymax' for extent
     */

    try
    {
        if(dset == NULL)
            open();

        _sampled = false;
        subset.clear();

        GDALDataType dtype = band->GetRasterDataType();
        if(dtype == GDT_Unknown)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unknown data type");

        OGRErr err = poly->transform(transf);
        CHECK_GDALERR(err);

        OGREnvelope env;
        poly->getEnvelope(&env);

        double minx = env.MinX;
        double miny = env.MinY;
        double maxx = env.MaxX;
        double maxy = env.MaxY;

        mlog(DEBUG, "minx: %.4lf", minx);
        mlog(DEBUG, "miny: %.4lf", miny);
        mlog(DEBUG, "maxx: %.4lf", maxx);
        mlog(DEBUG, "maxy: %.4lf", maxy);

        /* Get AOI window in pixels */
        int ulx = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * minx + invGeoTrans[2] * maxy));
        int uly = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * maxy + invGeoTrans[5] * maxy));

        int lrx = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * maxx + invGeoTrans[2] * miny));
        int lry = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * miny + invGeoTrans[5] * miny));

        /* Get raster extent in pixels */
        const int extulx = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * bbox.lon_min + invGeoTrans[2] * bbox.lat_max));
        const int extuly = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * bbox.lat_max + invGeoTrans[5] * bbox.lat_max));
        const int extlrx = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * bbox.lon_max + invGeoTrans[2] * bbox.lat_min));
        const int extlry = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * bbox.lat_min + invGeoTrans[5] * bbox.lat_min));

        if(extulx != 0 || extuly != 0)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Pixel (0, 0) geoTransformed as (%d, %d)", extulx, extuly);

        if(ulx < extulx)
        {
            mlog(DEBUG, "Clipping ulx %d to raster's extulx %d", ulx, extulx);
            ulx = extulx;
        }
        if(uly < extuly)
        {
            mlog(DEBUG, "Clipping uly %d to raster's extuly %d", uly, extuly);
            uly = extuly;
        }
        if(lrx > extlrx)
        {
            mlog(DEBUG, "Clipping lrx %d to raster's extlrx %d", lrx, extlrx);
            lrx = extlrx;
        }
        if(lry > extlry)
        {
            mlog(DEBUG, "Clipping lry %d to raster's extlry %d", lry, extlry);
            lry = extlry;
        }

        uint64_t cols2read = lrx - ulx;
        uint64_t rows2read = lry - uly;

        uint64_t size = cols2read * rows2read * GDALGetDataTypeSizeBytes(dtype);
        uint64_t maxperThread = subset.getmaxMem() / 2;
        if(size > maxperThread)
            throw RunTimeException(CRITICAL, RTE_ERROR, "subset thread requested too much memory: %ldMB, max perthread allowed: %ldMB",
                  size/(1024*1024), maxperThread/(1024*1024));

        if(!subset.memreserve(size))
            throw RunTimeException(CRITICAL, RTE_ERROR, "mempool depleted, request: %.2f MB", (float)size/(1024*1024));

        uint8_t* data = new uint8_t[size];

        mlog(DEBUG, "reading %ld bytes (%.1fMB), ulx: %d, uly: %d, cols2read: %ld, rows2read: %ld, datatype %s",
             size, (float)size/(1024*1024), ulx, uly, cols2read, rows2read, GDALGetDataTypeName(dtype));

        int cnt = 1;
        err = CE_None;
        do
        {
            err = band->RasterIO(GF_Read, ulx, uly, cols2read, rows2read, data, cols2read, rows2read, dtype, 0, 0, NULL);
        } while(err != CE_None && cnt--);

        if(err != CE_None)
        {
            subset.memrelese(size, data);
            throw RunTimeException(CRITICAL, RTE_ERROR, "RasterIO call failed");
        }

        _sampled = true;

        /* Update subset info returned to caller */
        subset.data     = data;
        subset.datatype = dtype;
        subset.cols     = cols2read;
        subset.rows     = rows2read;
        subset.time     = gpsTime;
    }
    catch (const RunTimeException &e)
    {
        _sampled = false;
        mlog(e.level(), "Error subsetting: %s", e.what());
    }
}

/*----------------------------------------------------------------------------
 * setCRSfromWkt
 *----------------------------------------------------------------------------*/
void GdalRaster::setCRSfromWkt(OGRSpatialReference& sref, const char* wkt)
{
    // mlog(DEBUG, "%s", wkt.c_str());
    OGRErr ogrerr = sref.importFromWkt(wkt);
    CHECK_GDALERR(ogrerr);
}


/*----------------------------------------------------------------------------
 * getUUID
 *----------------------------------------------------------------------------*/
std::string GdalRaster::getUUID(void)
{
    char uuid_str[UUID_STR_LEN];
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}


/*----------------------------------------------------------------------------
 * initAwsAccess
 *----------------------------------------------------------------------------*/
void GdalRaster::initAwsAccess(GeoParms* _parms)
{
    if(_parms->asset)
    {
#ifdef __aws__
        const char* path = _parms->asset->getPath();
        const char* identity = _parms->asset->getIdentity();
        CredentialStore::Credential credentials = CredentialStore::get(identity);
        if(credentials.provided)
        {
            VSISetPathSpecificOption(path, "AWS_ACCESS_KEY_ID", credentials.accessKeyId);
            VSISetPathSpecificOption(path, "AWS_SECRET_ACCESS_KEY", credentials.secretAccessKey);
            VSISetPathSpecificOption(path, "AWS_SESSION_TOKEN", credentials.sessionToken);
        }
        else
        {
            /* same as AWS CLI option '--no-sign-request' */
            VSISetPathSpecificOption(path, "AWS_NO_SIGN_REQUEST", "YES");
        }
#endif
    }
}


/*----------------------------------------------------------------------------
 * makeRectangle
 *----------------------------------------------------------------------------*/
OGRPolygon GdalRaster::makeRectangle(double minx, double miny, double maxx, double maxy)
{
    OGRPolygon poly;
    OGRLinearRing lr;

    /* Clockwise for interior of polygon */
    lr.addPoint(minx, miny);
    lr.addPoint(minx, maxy);
    lr.addPoint(maxx, maxy);
    lr.addPoint(maxx, miny);
    lr.addPoint(minx, miny);
    poly.addRing(&lr);
    return poly;
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
void GdalRaster::readPixel(const OGRPoint* poi)
{
    /* Use fast method recomended by GDAL docs to read individual pixel */
    try
    {
        const int col = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * poi->getX() + invGeoTrans[2] * poi->getY()));
        const int row = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * poi->getY() + invGeoTrans[5] * poi->getY()));

        // mlog(DEBUG, "%dP, %dL\n", col, row);

        int xBlockSize = 0;
        int yBlockSize = 0;
        band->GetBlockSize(&xBlockSize, &yBlockSize);

        /* Raster offsets to block of interest */
        int xblk = col / xBlockSize;
        int yblk = row / yBlockSize;

        GDALRasterBlock *block = NULL;
        int cnt = 1;
        do
        {
            /* Retry read if error */
            block = band->GetLockedBlockRef(xblk, yblk, false);
        } while (block == NULL && cnt--);
        CHECKPTR(block);

        /* Get data block pointer, no memory copied but block is locked */
        void *data = block->GetDataRef();
        if (data == NULL)
        {
            /* Before bailing release the block... */
            block->DropLock();
            CHECKPTR(data);
        }

        /* Calculate col, row inside of block */
        int _col = col % xBlockSize;
        int _row = row % yBlockSize;
        int offset = _row * xBlockSize + _col;

        /* Be carefull using offset based on the pixel data type */
        switch(band->GetRasterDataType())
        {
            case GDT_Byte:
            {
                uint8_t* p   = static_cast<uint8_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_UInt16:
            {
                uint16_t* p  = static_cast<uint16_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_Int16:
            {
                int16_t* p   = static_cast<int16_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_UInt32:
            {
                uint32_t* p  = static_cast<uint32_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_Int32:
            {
                int32_t* p   = static_cast<int32_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_Int64:
            {
                int64_t* p   = static_cast<int64_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_UInt64:
            {
                uint64_t* p  = static_cast<uint64_t*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_Float32:
            {
                float* p     = static_cast<float*>(data);
                sample.value = p[offset];
            }
            break;

            case GDT_Float64:
            {
                double* p    = static_cast<double*>(data);
                sample.value = p[offset];
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
void GdalRaster::resamplePixel(const OGRPoint* poi)
{
    try
    {
        const int col = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * poi->getX() + invGeoTrans[2] * poi->getY()));
        const int row = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * poi->getY() + invGeoTrans[5] * poi->getY()));

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

        bool validWindow = containsWindow(_col, _row, cols, rows, windowSize);
        if (validWindow)
        {
            readRasterWithRetry(_col, _row, windowSize, windowSize, &sample.value, 1, 1, &args);
            if(nodataCheck() && dataIsElevation)
            {
                sample.value += verticalShift;
            }
        }
        else
        {
            /* At least return pixel value if unable to resample raster */
            readPixel(poi);
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
void GdalRaster::computeZonalStats(const OGRPoint* poi)
{
    double *samplesArray = NULL;

    try
    {
        const int col = static_cast<int>(floor(invGeoTrans[0] + invGeoTrans[1] * poi->getX() + invGeoTrans[2] * poi->getY()));
        const int row = static_cast<int>(floor(invGeoTrans[3] + invGeoTrans[4] * poi->getY() + invGeoTrans[5] * poi->getY()));

        int windowSize = radiusInPixels * 2 + 1; // Odd window size around pixel

        int _col = col - radiusInPixels;
        int _row = row - radiusInPixels;

        GDALRasterIOExtraArg args;
        INIT_RASTERIO_EXTRA_ARG(args);
        args.eResampleAlg = parms->sampling_algo;
        samplesArray = new double[windowSize*windowSize];

        bool validWindow = containsWindow(_col, _row, cols, rows, windowSize);
        if (validWindow)
        {
            readRasterWithRetry(_col, _row, windowSize, windowSize, samplesArray, windowSize, windowSize, &args);

            /* One of the windows (raster or index data set) was valid. Compute zonal stats */
            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::min();
            double sum = 0;
            double nodata = band->GetNoDataValue();
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
                    if (value == nodata) continue;

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
                        sum += value;
                        validSamples.push_back(value);
                    }
                }
            }

            int validSamplesCnt = validSamples.size();
            if(validSamplesCnt > 0)
            {
                double stdev = 0;  /* Standard deviation */
                double mad   = 0;  /* Median absolute deviation (MAD) */
                double mean  = sum / validSamplesCnt;

                for(int i = 0; i < validSamplesCnt; i++)
                {
                    double value = validSamples[i];
                    stdev += std::pow(value - mean, 2);
                    mad   += std::fabs(value - mean);
                }

                stdev = std::sqrt(stdev / validSamplesCnt);
                mad   = mad / validSamplesCnt;

                /*
                 * Calculate median
                 * For performance use nth_element algorithm from std library since it sorts only part of the vector
                 * NOTE: (vector will be reordered by nth_element)
                 */
                std::size_t n = validSamplesCnt / 2;
                std::nth_element(validSamples.begin(), validSamples.begin() + n, validSamples.end());
                double median = validSamples[n];
                if(!(validSamplesCnt & 0x1))
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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error computing zonal stats: %s", e.what());
    }

    if(samplesArray) delete[] samplesArray;
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
    OGRErr ogrerr = sourceCRS.importFromEPSG(SLIDERULE_EPSG);
    CHECK_GDALERR(ogrerr);

    const char* projref = dset->GetProjectionRef();
    CHECKPTR(projref);
    ogrerr = targetCRS.importFromWkt(projref);
    CHECK_GDALERR(ogrerr);
    // mlog(DEBUG, "CRS from raster: %s", projref);

    if(overrideCRS)
    {
        ogrerr = overrideCRS(targetCRS);
        CHECK_GDALERR(ogrerr);
        // mlog(DEBUG, "CRS from raster: %s", projref);
    }

    OGRCoordinateTransformationOptions options;
    if(parms->proj_pipeline)
    {
        /* User specified proj pipeline */
        if(!options.SetCoordinateOperation(parms->proj_pipeline, false))
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to set user PROJ pipeline");
        mlog(DEBUG, "Set PROJ pipeline: %s", parms->proj_pipeline);
    }

    /* Limit to area of interest if AOI was set */
    bbox_t* aoi = &parms->aoi_bbox;
    bbox_t empty = {0, 0, 0, 0};
    if(memcmp(aoi, &empty, sizeof(bbox_t)))
    {
        if(!options.SetAreaOfInterest(aoi->lon_min, aoi->lat_min, aoi->lon_max, aoi->lat_max))
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to set AOI");

        mlog(DEBUG, "Limited AOI to: lon/lat Min (%.2lf, %.2lf), lon/lat Max (%.2lf, %.2lf)",
             aoi->lon_min, aoi->lat_min, aoi->lon_max, aoi->lat_max);
    }

    /* Force traditional axis order (lon, lat) */
    targetCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    sourceCRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    transf = OGRCreateCoordinateTransformation(&sourceCRS, &targetCRS, options);
    if(transf == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinates transform");
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

    if(_radius == 0) return 0;
    if(csize == 0) csize = 1;

    int radiusInMeters = ((_radius + csize - 1) / csize) * csize; // Round up to multiples of cell size
    return radiusInMeters / csize;
}

/*----------------------------------------------------------------------------
 * containsWindow
 *----------------------------------------------------------------------------*/
bool GdalRaster::containsWindow(int col, int row, int maxCol, int maxRow, int windowSize)
{
    if(col < 0 || row < 0)
        return false;

    if((col + windowSize >= maxCol) || (row + windowSize >= maxRow))
        return false;

    return true;
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
    int cnt = 1;
    CPLErr err = CE_None;
    do
    {
        err = band->RasterIO(GF_Read, col, row, colSize, rowSize, data, dataColSize, dataRowSize, GDT_Float64, 0, 0, args);
    } while (err != CE_None && cnt--);

    if (err != CE_None) throw RunTimeException(CRITICAL, RTE_ERROR, "RasterIO call failed");
}
