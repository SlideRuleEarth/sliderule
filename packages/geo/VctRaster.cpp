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
VctRaster::VctRaster(lua_State *L, const char *dem_sampling, const int sampling_radius,
                     const bool zonal_stats, const int target_crs):
    GeoRaster(L, dem_sampling, sampling_radius, zonal_stats)
{
    targetCrs = target_crs;
    layer = NULL;
}

/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
bool VctRaster::openGeoIndex(double lon, double lat)
{
    bool objCreated = false;
    std::string newVctFile;

    getIndexFile(newVctFile, lon, lat);

    /* Is it already opened with the same file? */
    if (geoIndex.dset != NULL && geoIndex.fileName == newVctFile)
        return true;

    try
    {
        /*
         * Create CRS transform. One transform for all vector index files.
         * When individual rasters are opened their CRS is validated against this transform.
         * It is a critical error if they are different.
         */
        if (cord.transf == NULL )
        {
            OGRErr ogrerr = cord.source.importFromEPSG(DEFAULT_EPSG);
            CHECK_GDALERR(ogrerr);
            ogrerr = cord.target.importFromEPSG(targetCrs);
            CHECK_GDALERR(ogrerr);

            /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
            cord.target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            cord.source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            /* Create coordinates transformation */
            cord.transf = OGRCreateCoordinateTransformation(&cord.source, &cord.target);
            if (cord.transf == NULL)
                throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create coordinate transform");
        }

        /* Cleanup previous */
        if (geoIndex.dset != NULL)
        {
            GDALClose((GDALDatasetH)geoIndex.dset);
            geoIndex.dset = NULL;
        }

        /* Open new vector data set*/
        geoIndex.dset = (GDALDataset *)GDALOpenEx(newVctFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        if (geoIndex.dset == NULL)
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to open vector file: %s:", newVctFile.c_str());

        geoIndex.fileName = newVctFile;
        layer = geoIndex.dset->GetLayer(0);
        CHECKPTR(layer);

        OGRSpatialReference *sref = layer->GetSpatialRef();
        CHECKPTR(sref);
        if(!cord.source.IsSame(sref))
            throw RunTimeException(CRITICAL, RTE_ERROR, "Vector index file has wrong CRS: %s", newVctFile.c_str());

        geoIndex.cols = geoIndex.dset->GetRasterXSize();
        geoIndex.rows = geoIndex.dset->GetRasterYSize();

        getIndexBbox(geoIndex.bbox, lon, lat);
        geoIndex.cellSize = 0;

        /*
         * For vector files cellSize is unknown. Cannot validate radiusInPixels
         * Validation is performed when rasters are opened.
        */

        objCreated = true;
        mlog(DEBUG, "Opened dataSet for %s", newVctFile.c_str());
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating new vector index dataset: %s", e.what());
    }

    if (!objCreated && geoIndex.dset)
    {
        geoIndex.clear();
        /* Do NOT clearTransform() - it is created once for all scenes */
        layer = NULL;
    }

    return objCreated;
}


/*----------------------------------------------------------------------------
 * transformCrs
 *----------------------------------------------------------------------------*/
bool VctRaster::transformCRS(OGRPoint &p)
{
    /* Do not convert to target CRS. Vector files are in geographic coordinates */
    p.assignSpatialReference(&cord.source);
    return (cord.transf != NULL);
}


/*----------------------------------------------------------------------------
 * findCachedRaster
 *----------------------------------------------------------------------------*/
bool VctRaster::findCachedRasters(OGRPoint& p)
{
    std::ignore = p;
    return false;
}


