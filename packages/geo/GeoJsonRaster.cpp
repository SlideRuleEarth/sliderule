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
#include "GeoJsonRaster.h"

#include <uuid/uuid.h>
#include <gdalwarper.h>


/******************************************************************************
 * LOCAL DEFINES AND MACROS
 ******************************************************************************/
/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/
/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoJsonRaster::LuaMetaName = "GeoJsonRaster";

const char* GeoJsonRaster::FILEDATA_KEY   = "data";
const char* GeoJsonRaster::FILELENGTH_KEY = "length";
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
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}


/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoJsonRaster* GeoJsonRaster::create (lua_State* L, int index)
{
    /* Get geojson file */
    lua_getfield(L, index, FILEDATA_KEY);
    const char* file = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get geojson file Length */
    lua_getfield(L, index, FILELENGTH_KEY);
    size_t filelength = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Get cellsize */
    lua_getfield(L, index, CELLSIZE_KEY);
    double _cellsize = getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Get Geo Parameters */
    lua_getfield(L, index, GeoParms::SELF);
    GeoParms* _parms = new GeoParms(L, index + 1);
    lua_pop(L, 1);

    /* Create GeoJsonRaster */
    return new GeoJsonRaster(L, _parms, file, filelength, _cellsize);
}


/*----------------------------------------------------------------------------
 * contains
 *----------------------------------------------------------------------------*/
bool GeoJsonRaster::includes(double lon, double lat)
{
    List<sample_t> slist;
    int sampleCnt = sample (lon, lat, slist);

    if( sampleCnt == 0 ) return false;
    if( sampleCnt > 1  ) mlog(ERROR, "Multiple samples returned for lon: %.2lf, lat: %.2lf, using first sample", lon, lat);

    return (static_cast<int>(slist[0].value) == RASTER_PIXEL_ON);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::~GeoJsonRaster(void)
{
    VSIUnlink(rasterFile.c_str());
    VSIUnlink(vrtFile.c_str());
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/* Utilitiy function to check constructor's params */
static void validatedParams(const char *file, long filelength, double _cellsize)
{
    if (file == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid file pointer (NULL)");

    if (filelength <= 0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid filelength: %ld:", filelength);

    if (_cellsize <= 0.0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid cellSize: %.2lf:", _cellsize);
}


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::GeoJsonRaster(lua_State *L, GeoParms* _parms, const char *file, long filelength, double _cellsize):
    VrtRaster(L, _parms)
{
    char uuid_str[UUID_STR_LEN] = {0};
    bool rasterCreated = false;
    GDALDataset *rasterDset = NULL;
    GDALDataset *jsonDset   = NULL;
    std::string  jsonFile;

    jsonFile   = "/vsimem/" + std::string(getUUID(uuid_str)) + ".geojson";
    rasterFile = "/vsimem/" + std::string(getUUID(uuid_str)) + ".tif";
    vrtFile    = "/vsimem/" + std::string(getUUID(uuid_str)) + ".vrt";

    /* Initialize Class Data Members */
    bzero(&gmtDate, sizeof(TimeLib::gmt_time_t));

    validatedParams(file, filelength, _cellsize);

    try
    {
        /* Create raster from json file */
        VSILFILE *fp = VSIFileFromMemBuffer(jsonFile.c_str(), (GByte *)file, (vsi_l_offset)filelength, FALSE);
        CHECKPTR(fp);
        VSIFCloseL(fp);

        jsonDset = (GDALDataset *)GDALOpenEx(jsonFile.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        CHECKPTR(jsonDset);
        OGRLayer *srcLayer = jsonDset->GetLayer(0);
        CHECKPTR(srcLayer);

        OGREnvelope e;
        OGRErr ogrerr = srcLayer->GetExtent(&e);
        CHECK_GDALERR(ogrerr);

        double cellsize = _cellsize;
        int cols = int((e.MaxX - e.MinX) / cellsize);
        int rows = int((e.MaxY - e.MinY) / cellsize);

        char **options = NULL;
        options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");

        GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        CHECKPTR(driver);
        rasterDset = (GDALDataset *)driver->Create(rasterFile.c_str(), cols, rows, 1, GDT_Byte, options);
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

        int bandInx = 1; /* Band index starts at 1, not 0 */
        GDALRasterBand *rb = rasterDset->GetRasterBand(bandInx);
        CHECKPTR(rb);
        rb->SetNoDataValue(RASTER_NODATA_VALUE);

        /*
         * Build params for GDALRasterizeLayers
         * Raster with 1 band, using first layer from json vector
         */
        const int BANDCNT = 1;

        int bandlist[BANDCNT];
        bandlist[0] = bandInx;

        OGRLayer *layers[BANDCNT];
        layers[0] = srcLayer;

        double burnValues[BANDCNT];
        burnValues[0] = RASTER_PIXEL_ON;

        CPLErr cplerr = GDALRasterizeLayers(rasterDset, 1, bandlist, 1, (OGRLayerH *)&layers[0], NULL, NULL, burnValues, NULL, NULL, NULL);
        CHECK_GDALERR(cplerr);
        mlog(DEBUG, "Rasterized geojson into raster %s", rasterFile.c_str());

        /* Store raster creation time */
        gmtDate = TimeLib::gettime();

        /* Must close raster to flush it into file */
        GDALClose((GDALDatasetH)rasterDset);
        rasterDset = NULL;

        /* Create vrt file, used in base class as index data set */
        List<std::string> rasterList;
        rasterList.add(rasterFile);
        buildVRT(vrtFile, rasterList);

        /* Open vrt as base class geoindex file. */
        openGeoIndex();

        rasterCreated = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
    }

   /* Cleanup */
   VSIUnlink(jsonFile.c_str());
   if (jsonDset) GDALClose((GDALDatasetH)jsonDset);
   if (rasterDset) GDALClose((GDALDatasetH)rasterDset);

   if (!rasterCreated)
        throw RunTimeException(CRITICAL, RTE_ERROR, "GeoJsonRaster failed");
}


/*----------------------------------------------------------------------------
 * getIndexFile
 *----------------------------------------------------------------------------*/
void GeoJsonRaster::getIndexFile(std::string &file, double lon, double lat)
{
    std::ignore = lon;
    std::ignore = lat;
    file = vrtFile;
}

/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
bool GeoJsonRaster::getRasterDate(raster_info_t& rinfo)
{
    rinfo.gmtDate = gmtDate;
    return true;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/