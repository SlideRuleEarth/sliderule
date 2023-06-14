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
VctRaster::VctRaster(lua_State *L, GeoParms* _parms):
    GeoRaster(L, _parms)
{
    layer = NULL;
}


/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
VctRaster::~VctRaster(void)
{
    for(int i = 0; i < featuresList.length(); i++)
    {
        OGRFeature* feature = featuresList[i];
        OGRFeature::DestroyFeature(feature);
    }
}


/*----------------------------------------------------------------------------
 * openGeoIndex
 *----------------------------------------------------------------------------*/
void VctRaster::openGeoIndex(double lon, double lat)
{
    std::string newVctFile;

    getIndexFile(newVctFile, lon, lat);

    /* Is it already opened with the same file? */
    if (geoIndex.dset != NULL && geoIndex.fileName == newVctFile)
        return;

    try
    {
        /* Cleanup previous */
        if (geoIndex.dset != NULL)
        {
            /* Free cached features for this vector index file */
            for(int i = 0; i < featuresList.length(); i++)
            {
                OGRFeature* feature = featuresList[i];
                OGRFeature::DestroyFeature(feature);
            }
            featuresList.clear();

            GDALClose((GDALDatasetH)geoIndex.dset);
            geoIndex.dset = NULL;
        }

        /* Open new vector data set*/
        geoIndex.dset = (GDALDataset *)GDALOpenEx(newVctFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        if (geoIndex.dset == NULL)
            throw RunTimeException(ERROR, RTE_ERROR, "Failed to open vector index file (%.2lf, %.2lf), file: %s:", lon, lat, newVctFile.c_str());

        geoIndex.fileName = newVctFile;
        layer = geoIndex.dset->GetLayer(0);
        CHECKPTR(layer);

        /* For now assume the first layer has the features we need
         * Clone all features and store them for performance/speed of feature lookup
         */
        layer->ResetReading();
        while(OGRFeature* feature = layer->GetNextFeature())
        {
            OGRFeature* newFeature = feature->Clone();
            featuresList.add(newFeature);
            OGRFeature::DestroyFeature(feature);
        }

        geoIndex.cols = geoIndex.dset->GetRasterXSize();
        geoIndex.rows = geoIndex.dset->GetRasterYSize();

        getIndexBbox(geoIndex.bbox, lon, lat);
        geoIndex.cellSize = 0;

        /* Vector index files should be in geographic CRS, no need to create transform */
        geoIndex.cord.clear(true);

        /*
         * For vector files cellSize is unknown. Cannot validate radiusInPixels
         * Validation is performed when rasters are opened.
        */
        mlog(DEBUG, "Opened: %s", newVctFile.c_str());
    }
    catch (const RunTimeException &e)
    {
        if (geoIndex.dset)
        {
            geoIndex.clear();
            layer = NULL;
        }
        throw;
    }
}


/*----------------------------------------------------------------------------
 * getIndexBbox
 *----------------------------------------------------------------------------*/
void VctRaster::getIndexBbox(bbox_t &bbox, double lon, double lat)
{
    std::ignore = lon = lat;

    OGREnvelope env;
    OGRErr err = layer->GetExtent(&env);
    if(err == OGRERR_NONE )
    {
        bbox.lon_min = env.MinX;
        bbox.lat_min = env.MinY;
        bbox.lon_max = env.MaxX;
        bbox.lat_max = env.MaxY;
        mlog(DEBUG, "Layer extent/bbox: (%.6lf, %.6lf), (%.6lf, %.6lf)", bbox.lon_min, bbox.lat_min, bbox.lon_max, bbox.lat_max);
    }
    else mlog(ERROR, "Failed to get layer extent/bbox");
}


/*----------------------------------------------------------------------------
 * findCachedRaster
 *----------------------------------------------------------------------------*/
bool VctRaster::findCachedRasters(OGRPoint& p)
{
    /*
     * Cached rasters cannot be used at this point.
     * Must get a list of all rasters for POI and only then check which are already cached.
    */
    std::ignore = p;
    return false;
}


