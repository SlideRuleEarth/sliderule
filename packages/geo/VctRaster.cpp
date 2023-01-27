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

#include "VctRaster.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void VctRaster::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void VctRaster::deinit (void)
{
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
VctRaster::VctRaster(lua_State *L, const char *dem_sampling, const int sampling_radius, const bool zonal_stats, const int target_crs):
    GeoRaster(L, dem_sampling, sampling_radius, zonal_stats, target_crs)
{
    setCheckCacheFirst(false);
}

/*----------------------------------------------------------------------------
 * openRis
 *----------------------------------------------------------------------------*/
bool VctRaster::openRis(double lon, double lat)
{
    bool objCreated = false;
    std::string newVctFile;

    getRisFile(newVctFile, lon, lat);

    /* Is it already open ? */
    if (ris.dset != NULL && ris.fileName == newVctFile)
        return true;

    try
    {
        /* Cleanup previous vctDset */
        if (ris.dset != NULL)
        {
            GDALClose((GDALDatasetH)ris.dset);
            ris.dset = NULL;
        }

        /* Open new vctDset */
        ris.dset = (GDALDataset *)GDALOpenEx(newVctFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        if (ris.dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to open vector file: %s:", newVctFile.c_str());


        ris.fileName = newVctFile;
        ris.band = ris.dset->GetRasterBand(1);
        CHECKPTR(ris.band);

        /* Get inverted geo transfer for vector file */
        double geot[6] = {0};
        CPLErr err = GDALGetGeoTransform(ris.dset, geot);
        CHECK_GDALERR(err);
        if (!GDALInvGeoTransform(geot, ris.invGeot))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            CHECK_GDALERR(CE_Failure);
        }

        /* Store information about vector data set */
        ris.cols = ris.dset->GetRasterXSize();
        ris.rows = ris.dset->GetRasterYSize();

        /* Get vector boundry box */
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


        OGRErr ogrerr = ris.srcSrs.importFromEPSG(ICESAT2_PHOTON_EPSG);
        CHECK_GDALERR(ogrerr);
        const char *projref = ris.dset->GetProjectionRef();
        CHECKPTR(projref);
        mlog(DEBUG, "%s", projref);
        ogrerr = ris.trgSrs.importFromProj4(projref);
        CHECK_GDALERR(ogrerr);

        /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
        ris.trgSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        ris.srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        /* Create coordinates transformation */

        /* Get transform for this vector index file */
        OGRCoordinateTransformation *newTransf = OGRCreateCoordinateTransformation(&ris.srcSrs, &ris.trgSrs);
        if (newTransf)
        {
            /* Delete the transform from last vector index file */
            if (ris.transf) OGRCoordinateTransformation::DestroyCT(ris.transf);

            /* Use the new one (they should be the same but just in case they are not...) */
            ris.transf = newTransf;
        }
        else mlog(ERROR, "Failed to create new transform, reusing transform from previous index file.\n");

        objCreated = true;
        mlog(DEBUG, "Opened dataSet for %s", newVctFile.c_str());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating new vector index dataset: %s", e.what());
    }

    if (!objCreated && ris.dset)
    {
        GDALClose((GDALDatasetH)ris.dset);
        ris.dset = NULL;
        ris.band = NULL;

        bzero(ris.invGeot, sizeof(ris.invGeot));
        ris.rows = ris.cols = ris.cellSize = 0;
        bzero(&ris.bbox, sizeof(ris.bbox));
    }

    return objCreated;
}


/*----------------------------------------------------------------------------
 * findRastersWithPoint
 *----------------------------------------------------------------------------*/
bool VctRaster::findRastersWithPoint(OGRPoint& p)
{
    bool foundFile = false;

    try
    {
        const int col = static_cast<int>(floor(ris.invGeot[0] + ris.invGeot[1] * p.getX() + ris.invGeot[2] * p.getY()));
        const int row = static_cast<int>(floor(ris.invGeot[3] + ris.invGeot[4] * p.getX() + ris.invGeot[5] * p.getY()));

        tifList->clear();
#warning IMPLEMENT ME!!!

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error finding raster in vector index file: %s", e.what());
    }

    return foundFile;
}

