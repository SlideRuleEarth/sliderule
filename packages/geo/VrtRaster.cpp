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

#include "VrtRaster.h"
#include <vrtdataset.h>
#include <gdal_utils.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void VrtRaster::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void VrtRaster::deinit (void)
{
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
VrtRaster::VrtRaster(lua_State *L, GeoParms* _parms):
    GeoRaster(L, _parms)
{
    band = NULL;
    bzero(invGeot, sizeof(invGeot));
}

/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
void VrtRaster::openGeoIndex(double lon, double lat)
{
    std::string newVrtFile;

    getIndexFile(newVrtFile, lon, lat);

    /* Is it already with the same file? */
    if (geoIndex.dset != NULL && geoIndex.fileName == newVrtFile)
        return;

    try
    {
        /* Cleanup previous */
        if (geoIndex.dset != NULL)
        {
            GDALClose((GDALDatasetH)geoIndex.dset);
            geoIndex.dset = NULL;
        }

        geoIndex.dset = (GDALDataset *)GDALOpenEx(newVrtFile.c_str(), GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
        if (geoIndex.dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to open VRT index file: %s:", newVrtFile.c_str());


        geoIndex.fileName = newVrtFile;
        band = geoIndex.dset->GetRasterBand(1);
        CHECKPTR(band);

        /* Get inverted geo transfer for vrt */
        double geot[6] = {0};
        CPLErr err = GDALGetGeoTransform(geoIndex.dset, geot);
        CHECK_GDALERR(err);
        if (!GDALInvGeoTransform(geot, invGeot))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            CHECK_GDALERR(CE_Failure);
        }

        /* Store information about vrt raster */
        geoIndex.cols = geoIndex.dset->GetRasterXSize();
        geoIndex.rows = geoIndex.dset->GetRasterYSize();

        /* Get raster boundry box */
        bzero(geot, sizeof(geot));
        err = geoIndex.dset->GetGeoTransform(geot);
        CHECK_GDALERR(err);
        geoIndex.bbox.lon_min = geot[0];
        geoIndex.bbox.lon_max = geot[0] + geoIndex.cols * geot[1];
        geoIndex.bbox.lat_max = geot[3];
        geoIndex.bbox.lat_min = geot[3] + geoIndex.rows * geot[5];
        geoIndex.cellSize     = geot[1];

        int radiusInPixels = radius2pixels(geoIndex.cellSize, samplingRadius);

        /* Limit maximum sampling radius */
        if (radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,
                                   "Sampling radius is too big: %d: max allowed %d meters",
                                   samplingRadius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(geoIndex.cellSize));
        }

        /* Create coordinates transformation */
        if (cord.transf == NULL )
        {
            OGRErr ogrerr = cord.source.importFromEPSG(DEFAULT_EPSG);
            CHECK_GDALERR(ogrerr);
            const char *projref = geoIndex.dset->GetProjectionRef();
            CHECKPTR(projref);
            mlog(DEBUG, "%s", projref);
            ogrerr = cord.target.importFromProj4(projref);
            CHECK_GDALERR(ogrerr);

            /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
            cord.target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            cord.source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            cord.transf = OGRCreateCoordinateTransformation(&cord.source, &cord.target);
            if (cord.transf == NULL)
                throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinates transform");
        }

        mlog(DEBUG, "Opened: %s", newVrtFile.c_str());
    }
    catch (const RunTimeException &e)
    {
        if (geoIndex.dset)
        {
            geoIndex.clear();
            cord.clear();
            bzero(invGeot, sizeof(invGeot));
            band = NULL;
        }
        throw;
    }
}


/*----------------------------------------------------------------------------
 * transformCRS
 *----------------------------------------------------------------------------*/
void VrtRaster::transformCRS(OGRPoint &p)
{
    if (cord.transf &&
        (p.transform(cord.transf) == OGRERR_NONE))
    {
        return;
    }

    throw RunTimeException(DEBUG, RTE_ERROR, "Coordinates Transform failed");
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool VrtRaster::findRasters(OGRPoint& p)
{
    bool foundFile = false;

    const int32_t col = static_cast<int32_t>(floor(invGeot[0] + invGeot[1] * p.getX() + invGeot[2] * p.getY()));
    const int32_t row = static_cast<int32_t>(floor(invGeot[3] + invGeot[4] * p.getX() + invGeot[5] * p.getY()));

    rastersList->clear();

    bool validPixel = (col >= 0) && (row >= 0) && (col < geoIndex.dset->GetRasterXSize()) && (row < geoIndex.dset->GetRasterYSize());
    if (!validPixel) return false;

    CPLString str;
    str.Printf("Pixel_%d_%d", col, row);

    const char *mdata = band->GetMetadataItem(str, "LocationInfo");
    if (mdata == NULL) return false; /* Pixel not in VRT file */

    CPLXMLNode *root = CPLParseXMLString(mdata);
    if (root && root->psChild && (root->eType == CXT_Element) && EQUAL(root->pszValue, "LocationInfo"))
    {
        for (CPLXMLNode *psNode = root->psChild; psNode != NULL; psNode = psNode->psNext)
        {
            if ((psNode->eType == CXT_Element) && EQUAL(psNode->pszValue, "File") && psNode->psChild)
            {
                char *fname = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
                if (fname)
                {
                    raster_info_t rinfo;
                    rinfo.fileName = fname;
                    rinfo.auxFileName.clear();

                    /* Get the date this raster was created */
                    getRasterDate(rinfo);

                    rastersList->add(rinfo);
                    CPLFree(fname);
                    foundFile = true;
                    /*
                     * VRT file can have many rasters in it with the same point of interest.
                     * This is not how GDAL VRT is inteded to be used.
                     * GDAL utilities in such a case use only one raster with POI (most likely the first found)
                     * Do the same - if one raster has been found stop searching for more.
                     *
                     * NOTE: VRT dataset is used for resampling and zonal statistics.
                     * Multiple raster threads are not allowed to read from the same dataset pointer.
                     * Must break here so only one raster is sampled for given POI.
                     */
                    break;
                }
            }
        }
    }
    if (root) CPLDestroyXMLNode(root);

    if(!foundFile) mlog(DEBUG, "Failed to find raster in VRT index file");

    return foundFile;
}

/*----------------------------------------------------------------------------
 * findCachedRaster
 *----------------------------------------------------------------------------*/
bool VrtRaster::findCachedRasters(OGRPoint& p)
{
    bool foundRaster = false;
    Raster *raster = NULL;

    const char *key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        if (containsPoint(raster, p))
        {
            raster->enabled = true;
            raster->point = p;
            foundRaster = true;
            break; /* Only one raster with this point in VRT */
        }
        key = rasterDict.next(&raster);
    }
    return foundRaster;
}


/*----------------------------------------------------------------------------
 * readGeoIndexData
 *----------------------------------------------------------------------------*/
bool VrtRaster::readGeoIndexData(OGRPoint *point, int srcWindowSize, int srcOffset,
                                 void *data, int dstWindowSize, GDALRasterIOExtraArg *args)
{
    int  col = static_cast<int>(floor((point->getX() - geoIndex.bbox.lon_min) / geoIndex.cellSize));
    int  row = static_cast<int>(floor((geoIndex.bbox.lat_max - point->getY()) / geoIndex.cellSize));
    int _col = col - srcOffset;
    int _row = row - srcOffset;

    bool validWindow = containsWindow(_col, _row, geoIndex.cols, geoIndex.rows, srcWindowSize);
    if (validWindow)
    {
        readRasterWithRetry(band, _col, _row, srcWindowSize, srcWindowSize, data, dstWindowSize, dstWindowSize, args);
    }

    return validWindow;
}

/*----------------------------------------------------------------------------
 * buildVRT
 *----------------------------------------------------------------------------*/
void VrtRaster::buildVRT(std::string& vrtFile, List<std::string>& rlist)
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
 * PRIVATE METHODS
 ******************************************************************************/

