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
#include "ArcticDEMRaster.h"

#include <uuid/uuid.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <gdal_priv.h>

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal.h"
#include "ogr_spatialref.h"

/******************************************************************************
 * LOCAL DEFINES AND MACROS
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
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/
/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArcticDEMRaster::LuaMetaName = "ArcticDEMRaster";
const struct luaL_Reg ArcticDEMRaster::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"samples",     luaSamples},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::init( void )
{
    /* Register all gdal drivers */
    GDALAllRegister();
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::deinit( void )
{
    GDALDestroy();
}

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaCreate( lua_State* L )
{
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch( const RunTimeException& e )
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
ArcticDEMRaster* ArcticDEMRaster::create( lua_State* L, int index )
{
    const int radius = getLuaInteger(L, -1);
    lua_pop(L, 1);
    const char* dem_sampling = getLuaString(L, -1);
    lua_pop(L, 1);
    const char* dem_type = getLuaString(L, -1);
    lua_pop(L, 1);
    return new ArcticDEMRaster(L, dem_type, dem_sampling, radius);
}



static void getVrtName( double lon, double lat, std::string& vrtFile )
{
    int ilat = floor(lat);
    int ilon = floor(lon);

    vrtFile = "/data/ArcticDem/strips/n" +
               std::to_string(ilat)      +
               ((ilon < 0) ? "w" : "e")  +
               std::to_string(abs(ilon)) +
               ".vrt";
}


/*----------------------------------------------------------------------------
 * vrtContainsPoint
 *----------------------------------------------------------------------------*/
inline bool ArcticDEMRaster::vrtContainsPoint(OGRPoint *p)
{
    return (vrtDset && p &&
           (p->getX() >= vrtBbox.lon_min) && (p->getX() <= vrtBbox.lon_max) &&
           (p->getY() >= vrtBbox.lat_min) && (p->getY() <= vrtBbox.lat_max));
}


/*----------------------------------------------------------------------------
 * rasterContainsPoint
 *----------------------------------------------------------------------------*/
inline bool ArcticDEMRaster::rasterContainsPoint(raster_t *raster, OGRPoint *p)
{
    return (raster && raster->dset && p &&
           (p->getX() >= raster->bbox.lon_min) && (p->getX() <= raster->bbox.lon_max) &&
           (p->getY() >= raster->bbox.lat_min) && (p->getY() <= raster->bbox.lat_max));
}


/*----------------------------------------------------------------------------
 * samplesMosaic
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::sampleMosaic(double lon, double lat)
{
    OGRPoint p = {lon, lat};
    if (p.transform(transf) == OGRERR_NONE)
    {
        /* Is point in big mosaic VRT dataset? */
        if (vrtContainsPoint(&p))
        {
            bool foundPoint = false;

            /* Is point in one of the rasters in dictionary */
            raster_t *raster = NULL;
            const char *key = rasterDict.first(&raster);
            while (key != NULL)
            {
                assert(raster);
                if (rasterContainsPoint(raster, &p))
                {
                    foundPoint =  true;
                    break;  /* Only one raster with this point in mosaic rasters */
                }
                key = rasterDict.next(&raster);
            }

            if (foundPoint)
            {
                raster->samplingEnabled = true;
                raster->point = &p;
#if 1
                /*
                 * The main thread reading in this case is a bit faster
                 * Reading the same point 10k times (raster cashed in GDAL)
                 * Main   thread took 0.02 sec
                 * Reader thread took 0.08 sec
                 */
                readRaster(raster);  /* Main thread reads raster */
#else
                readRasters();       /* Reader thread reads raster */
#endif
            }
            else
            {
                /* Find raster for point of interest, add to dictionary */
                findRasters(&p);
                updateDictionary(&p);
#if 0
                /* Calculate several points ahead this one, find their rasters, add to dictionary */
                for(int i=0; i<MAX_LOOK_AHEAD_RASTERS; i++)
                {
                    double newLon = lon + i + 0.1;  //For now, use real algo here
                    double newLat = lat + i + 0.1;  //For now, use real algo here
                    OGRPoint newPoint = {newLon, newLat};
                    if (newPoint.transform(transf) == OGRERR_NONE)
                    {
                        findRasters(&newPoint, false);
                        updateDictionary(NULL);  /* NULL point pointer, raster is to be opened but not read */
                    }
                    else throw RunTimeException(CRITICAL, RTE_ERROR, "transform failed for point: lon: %lf, lat: %lf", lon, lat);
                }
#endif
                readRasters();
        }
    }
    else
        throw RunTimeException(CRITICAL, RTE_ERROR, "point: lon: %lf, lat: %lf not in mosaic VRT", lon, lat);
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "transform failed for point: lon: %lf, lat: %lf", lon, lat);
}


/*----------------------------------------------------------------------------
 * samplesStrips
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::sampleStrips(double lon, double lat)
{
    OGRPoint p = {lon, lat};
    if (p.transform(transf) == OGRERR_NONE)
    {
        /* If point is not in current scene VRT dataset, open new scene */
        if (!vrtContainsPoint(&p))
        {
            std::string newVrtFile;
            getVrtName(lon, lat, newVrtFile);

            if (!openVrtDset(newVrtFile.c_str()))
                throw RunTimeException(CRITICAL, RTE_ERROR, "Could not open VRT file for point lon: %lf, lat: %lf", lon, lat);
        }

        findRasters(&p);
        updateDictionary(&p);
        readRasters();
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "transform failed for point: lon: %lf, lat: %lf", lon, lat);
}


/*----------------------------------------------------------------------------
 * samples
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::samples(double lon, double lat)
{
    invalidateCachedRasters();

    if      (demType == MOSAIC) sampleMosaic(lon, lat);
    else if (demType == STRIPS) sampleStrips(lon, lat);
}



/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArcticDEMRaster::~ArcticDEMRaster(void)
{
    /* Terminate all reader threads */
    for (int i = 0; i < readerCount; i++)
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

    /* Close all rasters */
    if (vrtDset) GDALClose((GDALDatasetH)vrtDset);

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

    if (transf) OGRCoordinateTransformation::DestroyCT(transf);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::findRasters(OGRPoint *p, bool clearList)
{
    try
    {
        const int32_t col = static_cast<int32_t>(floor(vrtInvGeot[0] + vrtInvGeot[1] * p->getX() + vrtInvGeot[2] * p->getY()));
        const int32_t row = static_cast<int32_t>(floor(vrtInvGeot[3] + vrtInvGeot[4] * p->getX() + vrtInvGeot[5] * p->getY()));

        if (clearList) tifList.clear();

        if (col >= 0 && row >= 0 && col < vrtDset->GetRasterXSize() && row < vrtDset->GetRasterYSize())
        {
            CPLString str;
            str.Printf("Pixel_%d_%d", col, row);

            const char *mdata = vrtBand->GetMetadataItem(str, "LocationInfo");
            if (mdata)
            {
                CPLXMLNode *root = CPLParseXMLString(mdata);
                if (root && root->psChild && root->eType == CXT_Element && EQUAL(root->pszValue, "LocationInfo"))
                {
                    for (CPLXMLNode *psNode = root->psChild; psNode; psNode = psNode->psNext)
                    {
                        if (psNode->eType == CXT_Element && EQUAL(psNode->pszValue, "File") && psNode->psChild)
                        {
                            char *fname = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
                            CHECKPTR(fname);
                            tifList.add(fname);
                            CPLFree(fname);
                        }
                    }
                }
                CPLDestroyXMLNode(root);
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error finding raster in VRT file: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * invaldiateAllRasters
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::invalidateCachedRasters(void)
{
    raster_t *raster = NULL;
    const char* key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        raster->samplingEnabled = false;
        raster->point = NULL;
        raster->value = INVALID_SAMPLE_VALUE;
        raster->readTime = 0.0;
        key = rasterDict.next(&raster);
    }
}

/*----------------------------------------------------------------------------
 * updateDictionary
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::updateDictionary(OGRPoint* p)
{
    int newRasters, recycledRasters, deletedRasters;
    newRasters = recycledRasters = deletedRasters = 0;

    // print2term("A- rasterDic.len: %d, tifList.len: %d\n", rasterDict.length(), tifList.length());

    /* Check new tif file list against rasters in dictionary */
    raster_t *raster = NULL;
    const char* key = NULL;
    const char* key_marker = "arcticdem/";

    for (int i = 0; i < tifList.length(); i++)
    {
        std::string& fileName = tifList[i];
        key = strstr(fileName.c_str(), key_marker);  /* Let's hope ArcticDem file naming schema does not change... */
        key += strlen(key_marker);
        assert(key);
        raster = NULL;

        if (rasterDict.find(key, &raster))
        {
            /* Update point to be sampled, mark raster samplingEnabled for next sampling */
            assert(raster);
            raster->samplingEnabled = true;
            raster->point = p;
            recycledRasters++;
        }
        else
        {
            /* Create new raster for this tif file since it is not in the dictionary */
            raster_t* raster = new raster_t;
            bzero(raster, sizeof(raster_t));

            raster->samplingEnabled = true;
            raster->obj = this;
            raster->point = p;
            raster->value = INVALID_SAMPLE_VALUE;
            raster->fileName = fileName;
            rasterDict.add(key, raster);
            newRasters++;
        }
    }

    /* Remove no longer needed rasters */
    if(rasterDict.length() > MAX_LOOK_AHEAD_RASTERS)
    {
        key = rasterDict.first(&raster);
        while (key != NULL)
        {
            assert(raster);
            if (!raster->samplingEnabled)
            {
                /* Main thread closing multiple rasters is OK */
                if (raster->dset) GDALClose((GDALDatasetH)raster->dset);
                rasterDict.remove(key);
                delete raster;
                deletedRasters++;
            }
            key = rasterDict.next(&raster);
        }
    }

    // print2term("B- rasterDic.len: %d, tifList.len: %d, newRasters: %d, recycledRasters: %d, deltedRasters: %d\n\n",
    //             rasterDict.length(), tifList.length(), newRasters, recycledRasters, deletedRasters);
}

/*----------------------------------------------------------------------------
 * readRasters
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::readRasters(void)
{
    try
    {
        /* Create additional reader threads if needed */
        int threadsNeeded = rasterDict.length();
        if (threadsNeeded > readerCount)
        {
            int newThreadsCnt = threadsNeeded - readerCount;
            // print2term("Creating %d new threads, readerCount: %d, neededThreads: %d\n", newThreadsCnt, readerCount, threadsNeeded);

            for (int i = 0; i < newThreadsCnt; i++)
            {
                if (readerCount < MAX_READER_THREADS)
                {
                    reader_t *reader = &rasterRreader[readerCount];
                    reader->raster = NULL;
                    reader->run = true;
                    reader->sync = new Cond();
                    reader->sync->lock();
                    {
                        reader->thread = new Thread(readingThread, reader);
                    }
                    reader->sync->unlock();
                    readerCount++;
                }
                else
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "number of rasters to read: %d, is greater than max reading threads %d\n",
                                           rasterDict.length(), MAX_READER_THREADS);
                }
            }
            assert(readerCount == threadsNeeded);
        }

        /* For each raster which is marked to be sampled, give it to the reader thread and signal the thread */
        int signaledReaders = 0;
        raster_t *raster = NULL;
        const char *key = rasterDict.first(&raster);
        int i = 0;
        while (key != NULL)
        {
            assert(raster);
            if (raster->samplingEnabled)
            {
                reader_t* reader = &rasterRreader[i++];
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

        /* Wait for all reader threads to finish sampling */
        if (signaledReaders > 0)
        {
            for (int i = 0; i < readerCount; i++)
            {
                reader_t *reader = &rasterRreader[i];
                raster_t *reaster;
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

#if 0
        i=0;
        key = rasterDict.first(&raster);
        while (key != NULL)
        {
            assert(raster);
            if(raster->samplingEnabled)
            {
                print2term("%03d, sample: %10.3lf, readTime: %10.3lf\n", i++, raster->value, raster->readTime);
            }
            key = rasterDict.next(&raster);
        }
        print2term("\n");
#endif

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading rasters: %s", e.what());
    }
}

/*----------------------------------------------------------------------------
 * readingThread
 *----------------------------------------------------------------------------*/
void* ArcticDEMRaster::readingThread(void *param)
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
                /* Read sample from raster */
                reader->raster->obj->readRaster(reader->raster);

                /* Done with this raster */
                reader->raster = NULL;
            }

            run = reader->run;
        }
        reader->sync->unlock();
    }

    return NULL;
}



/*----------------------------------------------------------------------------
 * readRaster
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::readRaster(raster_t* raster)
{
    try
    {
        double startTime = TimeLib::latchtime();

        raster->value = INVALID_SAMPLE_VALUE;  /* If read fails, return invalid value */

        /* Open raster if first time reading from it */
        if(raster->dset == NULL)
        {
            raster->dset = (GDALDataset *)GDALOpenEx(raster->fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
            if (raster->dset)
            {
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

                /* Get raster block size */
                raster->band = raster->dset->GetRasterBand(1);
                CHECKPTR(raster->band);
                raster->band->GetBlockSize(&raster->xBlockSize, &raster->yBlockSize);
                mlog(DEBUG, "Raster xBlockSize: %d, yBlockSize: %d", raster->xBlockSize, raster->yBlockSize);
            }
            else mlog(CRITICAL, "Failed to open raster: %s", raster->fileName.c_str());
        }


        /*
         * Read raster if it contains point of interest.
         *
         * NOTE: for look ahead logic, some rasters are to be opened only and not read
         *       raster->point == NULL indicates that
         */
        if (rasterContainsPoint(raster, raster->point))
        {
            /* Raster row, col for point */
            const int32_t col = static_cast<int32_t>(floor((raster->point->getX() - raster->bbox.lon_min) / raster->cellSize));
            const int32_t row = static_cast<int32_t>(floor((raster->bbox.lat_max  - raster->point->getY()) / raster->cellSize));

            /* Use fast 'lookup' method for nearest neighbour. */
            if (raster->obj->sampleAlg == GRIORA_NearestNeighbour)
            {
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

                float *fp = (float *)block->GetDataRef();
                if(fp)
                {
                    /* col, row inside of block */
                    uint32_t _col = col % raster->xBlockSize;
                    uint32_t _row = row % raster->yBlockSize;
                    uint32_t offset = _row * raster->xBlockSize + _col;
                    raster->value = fp[offset];

                    mlog(DEBUG, "Elevation: %f, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u\n",
                         raster->value, col, row, xblk, yblk, _col, _row, offset);
                }
                else mlog(CRITICAL, "block->GetDataRef() returned NULL pointer\n");
                block->DropLock();
            }
            else
            {
#if 0
     FROM gdal.h
     What are these kernels? How do I use them to read one pixel with resampling?
     Below is an attempt to do it but I am 99.999% sure it is wrong.

    /*! Nearest neighbour */                            GRIORA_NearestNeighbour = 0,
    /*! Bilinear (2x2 kernel) */                        GRIORA_Bilinear = 1,
    /*! Cubic Convolution Approximation (4x4 kernel) */ GRIORA_Cubic = 2,
    /*! Cubic B-Spline Approximation (4x4 kernel) */    GRIORA_CubicSpline = 3,
    /*! Lanczos windowed sinc interpolation (6x6 kernel) */ GRIORA_Lanczos = 4,
    /*! Average */                                      GRIORA_Average = 5,
    /*! Mode (selects the value which appears most often of all the sampled points) */
                                                        GRIORA_Mode = 6,
    /*! Gauss blurring */                               GRIORA_Gauss = 7
#endif

                float rbuf[1] = {0};
                int _cellsize = raster->cellSize;
                int radius_in_meters = ((raster->obj->radius + _cellsize - 1) / _cellsize) * _cellsize; // Round to multiple of cellSize
                int radius_in_pixels = (radius_in_meters == 0) ? 1 : radius_in_meters / _cellsize;
                int _col = col - radius_in_pixels;
                int _row = row - radius_in_pixels;
                int size = radius_in_pixels + 1 + radius_in_pixels;

                /* If 8 pixels around pixel of interest are not in the raster boundries return pixel value. */
                if (_col < 0 || _row < 0)
                {
                    _col = col;
                    _row = row;
                    size = 1;
                    raster->obj->sampleAlg = GRIORA_NearestNeighbour;
                }

                GDALRasterIOExtraArg args;
                INIT_RASTERIO_EXTRA_ARG(args);
                args.eResampleAlg = raster->obj->sampleAlg;
                CPLErr err = CE_None;
                int cnt = 2;
                do
                {
                    /* Retry read if error */
                    err = raster->band->RasterIO(GF_Read, _col, _row, size, size, rbuf, 1, 1, GDT_Float32, 0, 0, &args);
                } while (err != CE_None && cnt--);
                CHECK_GDALERR(err);
                raster->value = rbuf[0];
                mlog(DEBUG, "Resampled elevation:  %f, radiusMeters: %d, radiusPixels: %d, size: %d\n", rbuf[0], raster->obj->radius, radius_in_pixels, size);
                // print2term("Resampled elevation:  %f, radiusMeters: %d, radiusPixels: %d, size: %d\n", rbuf[0], radius, radius_in_pixels, size);
            }
        }

        raster->readTime = TimeLib::latchtime() - startTime;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading raster: %s", e.what());
    }
}


/*----------------------------------------------------------------------------
 * openVrtDset
 *----------------------------------------------------------------------------*/
bool ArcticDEMRaster::openVrtDset(const char *fileName)
{
    bool objCreated = false;

    try
    {
        /* Cleanup previous vrtDset */
        if (vrtDset)
        {
            GDALClose((GDALDatasetH)vrtDset);
            vrtDset = NULL;
        }

        vrtDset = (VRTDataset *)GDALOpenEx(fileName, GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
        if(vrtDset)
        {
            vrtFileName = fileName;
            vrtBand = vrtDset->GetRasterBand(1);
            CHECKPTR(vrtBand);

            /* Get inverted geo transfer for vrt */
            double geot[6] = {0};
            CPLErr err = GDALGetGeoTransform(vrtDset, geot);
            CHECK_GDALERR(err);
            if (!GDALInvGeoTransform(geot, vrtInvGeot))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
                CHECK_GDALERR(CE_Failure);
            }

            /* Store information about vrt raster */
            vrtCols = vrtDset->GetRasterXSize();
            vrtRows = vrtDset->GetRasterYSize();

            /* Get raster boundry box */
            bzero(geot, sizeof(geot));
            err = vrtDset->GetGeoTransform(geot);
            CHECK_GDALERR(err);
            vrtBbox.lon_min = geot[0];
            vrtBbox.lon_max = geot[0] + vrtCols * geot[1];
            vrtBbox.lat_max = geot[3];
            vrtBbox.lat_min = geot[3] + vrtRows * geot[5];

            vrtCellSize = geot[1];

            OGRErr ogrerr = srcSrs.importFromEPSG(PHOTON_CRS);
            CHECK_GDALERR(ogrerr);
            const char *projref = vrtDset->GetProjectionRef();
            if (projref)
            {
                mlog(DEBUG, "%s", projref);
                ogrerr = trgSrs.importFromProj4(projref);
            }
            else
            {
                /* In case vrt file does not have projection info, use default */
                ogrerr = trgSrs.importFromEPSG(ARCTIC_DEM_CRS);
            }
            CHECK_GDALERR(ogrerr);

            /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
            trgSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            /* Create coordinates transformation */

            /* Get transform for this VRT file */
            OGRCoordinateTransformation *newTransf = OGRCreateCoordinateTransformation(&srcSrs, &trgSrs);
            if (newTransf)
            {
                /* Delete the transform from last vrt file */
                if (transf) OGRCoordinateTransformation::DestroyCT(transf);

                /* Use the new one (they should be the same but just in case they are not...) */
                transf = newTransf;
            }
            else mlog(ERROR, "Failed to create new transform, reusing transform from previous VRT file.\n");

            objCreated = true;
        }
        else mlog(CRITICAL, "Failed to open VRT file: %s", fileName);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating new VRT dataset: %s", e.what());
    }

    if(!objCreated)
    {
        if (vrtDset)
        {
            GDALClose((GDALDatasetH)vrtDset);
            vrtDset = NULL;
            vrtBand = NULL;

            bzero(vrtInvGeot, sizeof(vrtInvGeot));
            vrtRows = vrtCols = vrtCellSize = 0;
            bzero(&vrtBbox, sizeof(vrtBbox));
        }
    }

    return objCreated;
}

/* Utilitiy function to get UUID string */
static const char *getUuid(char *uuid_str)
{
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return uuid_str;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArcticDEMRaster::ArcticDEMRaster(lua_State *L, const char *dem_type, const char *dem_sampling, const int sampling_radius):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    char uuid_str[UUID_STR_LEN] = {0};
    std::string fname;

    CHECKPTR(dem_type);
    CHECKPTR(dem_sampling);
    demType = INVALID;

    if (!strcasecmp(dem_type, "mosaic"))
    {
        // fname = "/data/ArcticDem/mosaic_short.vrt";
        fname = "/data/ArcticDem/mosaic.vrt";
        demType = MOSAIC;
    }
    else if (!strcasecmp(dem_type, "strip"))
    {
        // fname = "/data/ArcticDem/strips/n51w178.vrt";
        fname = "/data/ArcticDem/strips/n51e156.vrt"; /* First strip file in catalog */
        demType = STRIPS;
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid dem_type: %s:", dem_type);

    if      (!strcasecmp(dem_sampling, "NearestNeighbour")) sampleAlg = GRIORA_NearestNeighbour;
    else if (!strcasecmp(dem_sampling, "Bilinear"))         sampleAlg = GRIORA_Bilinear;
    else if (!strcasecmp(dem_sampling, "Cubic"))            sampleAlg = GRIORA_Cubic;
    else if (!strcasecmp(dem_sampling, "CubicSpline"))      sampleAlg = GRIORA_CubicSpline;
    else if (!strcasecmp(dem_sampling, "Lanczos"))          sampleAlg = GRIORA_Lanczos;
    else if (!strcasecmp(dem_sampling, "Average"))          sampleAlg = GRIORA_Average;
    else if (!strcasecmp(dem_sampling, "Mode"))             sampleAlg = GRIORA_Mode;
    else if (!strcasecmp(dem_sampling, "Gauss"))            sampleAlg = GRIORA_Gauss;
    else
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling algorithm: %s:", dem_sampling);

    if (sampling_radius >= 0)
        radius = sampling_radius;
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling radius: %d:", sampling_radius);

    /* Initialize Class Data Members */
    vrtDset = NULL;
    vrtBand = NULL;
    bzero(vrtInvGeot, sizeof(vrtInvGeot));
    vrtRows = vrtCols = vrtCellSize = 0;
    bzero(&vrtBbox, sizeof(vrtBbox));
    tifList.clear();
    rasterDict.clear();
    bzero(rasterRreader, sizeof(rasterRreader));
    readerCount = 0;
    transf = NULL;
    srcSrs.Clear();
    trgSrs.Clear();

    if(!openVrtDset(fname.c_str()))
        throw RunTimeException(CRITICAL, RTE_ERROR, "Constructor failed");
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->vrtRows);
        lua_pushinteger(L, lua_obj->vrtCols);
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
int ArcticDEMRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->vrtBbox.lon_min);
        lua_pushnumber(L, lua_obj->vrtBbox.lat_min);
        lua_pushnumber(L, lua_obj->vrtBbox.lon_max);
        lua_pushnumber(L, lua_obj->vrtBbox.lat_max);
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
int ArcticDEMRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->vrtCellSize);
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

int ArcticDEMRaster::getSampledRastersCount(void)
{
    raster_t *raster = NULL;
    int cnt = 0;

    /* Not all rasters in dictionary were sampled, find out how many were */
    const char *key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        if (raster->samplingEnabled) cnt++;
        key = rasterDict.next(&raster);
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * luaSamples - :samples(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaSamples(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get Elevations */
        lua_obj->samples(lon, lat);

        int sampledRastersCnt = lua_obj->getSampledRastersCount();
        if (sampledRastersCnt > 0)
        {
            raster_t   *raster = NULL;
            const char *key    = NULL;

            /* Create return table */
            lua_createtable(L, sampledRastersCnt, 0);

            /* Populate the return table */
            int i = 0;
            key = lua_obj->rasterDict.first(&raster);
            while (key != NULL)
            {
                assert(raster);
                if (raster->samplingEnabled)
                {
                    lua_createtable(L, 0, 2);
                    LuaEngine::setAttrStr(L, "file", raster->fileName.c_str());
                    LuaEngine::setAttrNum(L, "value", raster->value);
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

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}