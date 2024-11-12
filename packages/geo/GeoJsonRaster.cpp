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

#include "GeoJsonRaster.h"
#include <gdalwarper.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoJsonRaster::FILEDATA_KEY   = "data";
const char* GeoJsonRaster::CELLSIZE_KEY   = "cellsize";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - file(
 *  {
 *      file=<file>,
 *      filelength=<filelength>,
 *  })
 *----------------------------------------------------------------------------*/
int GeoJsonRaster::luaCreate (lua_State* L)
{
    RequestFields* rqst_parms = NULL;
    try
    {
        const char* geojstr = getLuaString(L, 1);
        const double cellsize = getLuaFloat(L, 2);
        rqst_parms = new RequestFields(L, 0, {});
        GeoFields* geo_fields = new GeoFields();
        if(!rqst_parms->samplers.add(GeoFields::DEFAULT_KEY, geo_fields))
        {
            delete geo_fields;
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to add default geo fields");
        }
        LuaObject::referenceLuaObject(rqst_parms); // GeoJsonRaster expects a LuaObject created from a Lua script
        return createLuaObject(L, new GeoJsonRaster(L, rqst_parms, GeoFields::DEFAULT_KEY, geojstr, cellsize));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
        delete rqst_parms;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoJsonRaster* GeoJsonRaster::create (const string& geojson, double cellsize)
{
    RequestFields* rqst_parms = NULL;
    try
    {
        rqst_parms = new RequestFields(NULL, 0, {});
        GeoFields* geo_fields = new GeoFields();
        if(!rqst_parms->samplers.add(GeoFields::DEFAULT_KEY, geo_fields))
        {
            delete geo_fields;
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to add default geo fields");
        }
        LuaObject::referenceLuaObject(rqst_parms); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
        return new GeoJsonRaster(NULL, rqst_parms, GeoFields::DEFAULT_KEY, geojson.c_str(), cellsize);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
        delete rqst_parms;
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * includes
 *----------------------------------------------------------------------------*/
bool GeoJsonRaster::includes(double lon, double lat, double height)
{
    static_cast<void>(height);
    bool pixel_on = false;

    /*
     * Skip transforming POI since geojsons should be in geographic coordinates.
     * Raster created from geojson is also in geo.
     *
     * Don't need a mutex, multiple threads can read the same data.
     */

    if((lon >= bbox.lon_min) && (lon <= bbox.lon_max) &&
       (lat >= bbox.lat_min) && (lat <= bbox.lat_max))
    {
        const uint32_t row = (bbox.lat_max - lat) / cellsize;
        const uint32_t col = (lon - bbox.lon_min) / cellsize;

        if((row < rows) && (col < cols))
        {
            pixel_on = rawPixel(row, col);
        }
    }

    return pixel_on;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::~GeoJsonRaster(void)
{
    delete [] geojstr;
    delete [] data;
    VSIUnlink(rasterFileName.c_str());
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::GeoJsonRaster(lua_State* L, RequestFields* rqst_parms, const char* key, const char* _geojstr, double _cellsize):
 GeoRaster(L, rqst_parms, key, std::string("/vsimem/" + GdalRaster::getUUID() + ".tif"), TimeLib::gpstime(), false /* not elevation*/),
 data(NULL),
 cellsize(_cellsize),
 cols(0),
 rows(0),
 bbox({0, 0, 0, 0})
{
    bool rasterCreated = false;
    GDALDataset* rasterDset = NULL;
    GDALDataset* jsonDset   = NULL;
    const std::string jsonFile = "/vsimem/" + GdalRaster::getUUID() + ".geojson";
    rasterFileName = getFileName();
    geojstr = StringLib::duplicate(_geojstr);

    if (geojstr == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid file pointer (NULL)");

    if (cellsize <= 0.0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid cellSize: %.2lf:", cellsize);

    try
    {
        /* Create raster from geojson file */
        const vsi_l_offset len = strlen(geojstr);
        GByte* bytes = const_cast<GByte*>(reinterpret_cast<const GByte*>(geojstr));
        VSILFILE* fp = VSIFileFromMemBuffer(jsonFile.c_str(), bytes, len, FALSE);
        CHECKPTR(fp);
        VSIFCloseL(fp);

        jsonDset = static_cast<GDALDataset *>(GDALOpenEx(jsonFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL));
        CHECKPTR(jsonDset);
        OGRLayer *srcLayer = jsonDset->GetLayer(0);
        CHECKPTR(srcLayer);

        OGREnvelope e;
        OGRErr ogrerr = srcLayer->GetExtent(&e);
        CHECK_GDALERR(ogrerr);

        cols = static_cast<int>((e.MaxX - e.MinX) / cellsize);
        rows = static_cast<int>((e.MaxY - e.MinY) / cellsize);

        char **options = NULL;
        options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");

        GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        CHECKPTR(driver);
        rasterDset = driver->Create(rasterFileName.c_str(), cols, rows, 1, GDT_Byte, options);
        CSLDestroy(options);
        CHECKPTR(rasterDset);
        double geot[6] = {e.MinX, cellsize, 0, e.MaxY, 0, -cellsize};
        rasterDset->SetGeoTransform(geot);

        OGRSpatialReference *srcSrs = srcLayer->GetSpatialRef();
        CHECKPTR(srcSrs);

        char *wkt;
        ogrerr = srcSrs->exportToWkt(&wkt);
        CHECK_GDALERR(ogrerr);
        rasterDset->SetProjection(wkt);
        CPLFree(wkt);

        GDALRasterBand *rb = rasterDset->GetRasterBand(1);
        CHECKPTR(rb);
        rb->SetNoDataValue(RASTER_NODATA_VALUE);

        /*
         * Build params for GDALRasterizeLayers
         * Raster with 1 band, using first layer from json vector
         */
        const int BANDCNT = 1;

        int bandlist[BANDCNT];
        bandlist[0] = 1;

        OGRLayer *layers[BANDCNT];
        layers[0] = srcLayer;

        double burnValues[BANDCNT];
        burnValues[0] = RASTER_PIXEL_ON;

        const CPLErr cplerr = GDALRasterizeLayers(rasterDset, 1, bandlist, 1, reinterpret_cast<OGRLayerH*>(&layers[0]), NULL, NULL, burnValues, NULL, NULL, NULL);
        CHECK_GDALERR(cplerr);
        mlog(DEBUG, "Rasterized geojson into raster %s", rasterFileName.c_str());

#if 0
       /* For debugging, save raster to local file */
       GDALDriver* localDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
       if(localDriver)
       {
           const char* localFileName = "/tmp/local_raster.tif";
           remove(localFileName);
           GDALDataset* dsetcopy = localDriver->CreateCopy(localFileName, rasterDset, FALSE, NULL, NULL, NULL);
           GDALClose(reinterpret_cast<GDALDatasetH>(dsetcopy));
           mlog(INFO, "Raster saved to local file: %s", localFileName);
       }
       else mlog(ERROR, "Failed to get GDAL driver for local file");
#endif

        /* Must close raster to flush it into file in vsimem */
        GDALClose((GDALDatasetH)rasterDset);
        rasterDset = NULL;

        /* Get all pixels in raster */
        data = getPixels(0,0);

        /* Sanity check for cols/rows/cellsize */
        if((cols != getCols()) || (rows != getRows()))
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid raster dimensions: %d x %d", cols, rows);
        if(cellsize != getCellSize())
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid cellsize: %.2lf", cellsize);

        bbox = getBbox();
        rasterCreated = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
        VSIUnlink(rasterFileName.c_str());
        delete[] data;
        data = NULL;
    }

   /* Cleanup */
   VSIUnlink(jsonFile.c_str());
   GDALClose(reinterpret_cast<GDALDatasetH>(jsonDset));
   GDALClose(reinterpret_cast<GDALDatasetH>(rasterDset));

   if(!rasterCreated)
       throw RunTimeException(CRITICAL, RTE_ERROR, "GeoJsonRaster failed");
}