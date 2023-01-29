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
VrtRaster::VrtRaster(lua_State *L, const char *dem_sampling, const int sampling_radius, const bool zonal_stats):
    GeoRaster(L, dem_sampling, sampling_radius, zonal_stats)
{
    band = NULL;
    bzero(invGeot, sizeof(invGeot));
}

/*----------------------------------------------------------------------------
 * openRis
 *----------------------------------------------------------------------------*/
bool VrtRaster::openRis(double lon, double lat)
{
    bool objCreated = false;
    std::string newVrtFile;

    getRisFile(newVrtFile, lon, lat);

    /* Is it already open vrt? */
    if (ris.dset != NULL && ris.fileName == newVrtFile)
        return true;

    try
    {
        /* Cleanup previous vrtDset */
        if (ris.dset != NULL)
        {
            GDALClose((GDALDatasetH)ris.dset);
            ris.dset = NULL;
        }

        /* Open new vrtDset */
        ris.dset = (GDALDataset *)GDALOpenEx(newVrtFile.c_str(), GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
        if (ris.dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to open VRT file: %s:", newVrtFile.c_str());


        ris.fileName = newVrtFile;
        band = ris.dset->GetRasterBand(1);
        CHECKPTR(band);

        /* Get inverted geo transfer for vrt */
        double geot[6] = {0};
        CPLErr err = GDALGetGeoTransform(ris.dset, geot);
        CHECK_GDALERR(err);
        if (!GDALInvGeoTransform(geot, invGeot))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            CHECK_GDALERR(CE_Failure);
        }

        /* Store information about vrt raster */
        ris.cols = ris.dset->GetRasterXSize();
        ris.rows = ris.dset->GetRasterYSize();

        /* Get raster boundry box */
        bzero(geot, sizeof(geot));
        err = ris.dset->GetGeoTransform(geot);
        CHECK_GDALERR(err);
        ris.bbox.lon_min = geot[0];
        ris.bbox.lon_max = geot[0] + ris.cols * geot[1];
        ris.bbox.lat_max = geot[3];
        ris.bbox.lat_min = geot[3] + ris.rows * geot[5];
        ris.cellSize     = geot[1];

        int radiusInPixels = radius2pixels(ris.cellSize, samplingRadius);

        /* Limit maximum sampling radius */
        if (radiusInPixels > MAX_SAMPLING_RADIUS_IN_PIXELS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,
                                   "Sampling radius is too big: %d: max allowed %d meters",
                                   samplingRadius, MAX_SAMPLING_RADIUS_IN_PIXELS * static_cast<int>(ris.cellSize));
        }

        /* Create coordinates transformation */
        if (crsConverter.transf == NULL )
        {
            OGRErr ogrerr = crsConverter.source.importFromEPSG(DEFAULT_EPSG);
            CHECK_GDALERR(ogrerr);
            const char *projref = ris.dset->GetProjectionRef();
            CHECKPTR(projref);
            mlog(DEBUG, "%s", projref);
            ogrerr = crsConverter.target.importFromProj4(projref);
            CHECK_GDALERR(ogrerr);

            /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
            crsConverter.target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            crsConverter.source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            crsConverter.transf = OGRCreateCoordinateTransformation(&crsConverter.source, &crsConverter.target);
            if (crsConverter.transf == NULL)
                throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinate transform");
        }

        objCreated = true;
        mlog(DEBUG, "Opened dataSet for %s", newVrtFile.c_str());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating new VRT dataset: %s", e.what());
    }

    if (!objCreated && ris.dset)
    {
        clearRis();
        clearTransform();
        bzero(invGeot, sizeof(invGeot));
        band = NULL;
    }

    return objCreated;
}


/*----------------------------------------------------------------------------
 * transformCrs
 *----------------------------------------------------------------------------*/
bool VrtRaster::transformCRS(OGRPoint &p)
{
    if (crsConverter.transf &&
        (p.transform(crsConverter.transf) == OGRERR_NONE))
    {
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool VrtRaster::findRasters(OGRPoint& p)
{
    bool foundFile = false;

    try
    {
        const int32_t col = static_cast<int32_t>(floor(invGeot[0] + invGeot[1] * p.getX() + invGeot[2] * p.getY()));
        const int32_t row = static_cast<int32_t>(floor(invGeot[3] + invGeot[4] * p.getX() + invGeot[5] * p.getY()));

        tifList->clear();

        bool validPixel = (col >= 0) && (row >= 0) && (col < ris.dset->GetRasterXSize()) && (row < ris.dset->GetRasterYSize());
        if (!validPixel) return false;

        CPLString str;
        str.Printf("Pixel_%d_%d", col, row);

        const char *mdata = band->GetMetadataItem(str, "LocationInfo");
        if (mdata == NULL) return false; /* Pixel not in VRT file */

        CPLXMLNode *root = CPLParseXMLString(mdata);
        if (root == NULL) return false;  /* Pixel is in VRT file, but parser did not find its node  */

        if (root->psChild && (root->eType == CXT_Element) && EQUAL(root->pszValue, "LocationInfo"))
        {
            for (CPLXMLNode *psNode = root->psChild; psNode != NULL; psNode = psNode->psNext)
            {
                if ((psNode->eType == CXT_Element) && EQUAL(psNode->pszValue, "File") && psNode->psChild)
                {
                    char *fname = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
                    CHECKPTR(fname);
                    tifList->add(fname);
                    CPLFree(fname);
                    foundFile = true;
                    break; /* There should be only one raster with point of interest in VRT */
                }
            }
        }
        CPLDestroyXMLNode(root);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error finding raster in VRT file: %s", e.what());
    }

    return foundFile;
}

/*----------------------------------------------------------------------------
 * findCachedRaster
 *----------------------------------------------------------------------------*/
bool VrtRaster::findCachedRasters(OGRPoint& p)
{
    bool foundRaster = false;
    raster_t *raster = NULL;

    const char *key = rasterDict.first(&raster);
    while (key != NULL)
    {
        assert(raster);
        if (rasterContainsPoint(raster, p))
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
 * sampleRasters
 *----------------------------------------------------------------------------*/
void VrtRaster::sampleRasters(void)
{
    /*
     * For VRT based rasters (tiles/mosaics) there is only one raster for each POI.
     */
    raster_t *raster = NULL;
    const char *key = rasterDict.first(&raster);
    int i = 0;
    while (key != NULL)
    {
        assert(raster);
        if (raster->enabled)
        {
            /* Found the only raster with POI */
            processRaster(raster, this);
            break;
        }
        key = rasterDict.next(&raster);
    }
}
/*----------------------------------------------------------------------------
 * readRisData
 *----------------------------------------------------------------------------*/
bool VrtRaster::readRisData(OGRPoint* point, int srcWindowSize, int srcOffset,
                            void *data, int dstWindowSize, GDALRasterIOExtraArg *args)
{
    int  col = static_cast<int>(floor((point->getX() - ris.bbox.lon_min) / ris.cellSize));
    int  row = static_cast<int>(floor((ris.bbox.lat_max - point->getY()) / ris.cellSize));
    int _col = col - srcOffset;
    int _row = row - srcOffset;

    bool validWindow = rasterContainsWindow(_col, _row, ris.cols, ris.rows, srcWindowSize);
    if (validWindow)
    {
        RasterIoWithRetry(band, _col, _row, srcWindowSize, srcWindowSize, data, dstWindowSize, dstWindowSize, args);
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

