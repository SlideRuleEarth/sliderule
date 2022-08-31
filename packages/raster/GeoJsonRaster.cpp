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
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <gdal_priv.h>


/******************************************************************************
 * LOCAL DEFINES AND MACROS
 ******************************************************************************/

#define CHECKPTR(p)                                                           \
do                                                                            \
{                                                                             \
    if ((p) == NULL)                                                          \
    {                                                                         \
        throw RunTimeException(CRITICAL, RTE_ERROR, "NULL pointer detected"); \
    }                                                                         \
} while (0)


#define CHECK_GDALERR(e)                                                          \
do                                                                                \
{                                                                                 \
    if ((e))   /* CPLErr and OGRErr types have 0 for no error  */                 \
    {                                                                             \
        throw RunTimeException(CRITICAL, RTE_ERROR, "GDAL ERROR detected: %d", e);\
    }                                                                             \
} while (0)


/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

struct GeoJsonRaster::impl
{
    public:
        OGRCoordinateTransformation *latlon2xy;
        OGRSpatialReference source;
        OGRSpatialReference target;
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoJsonRaster::LuaMetaName = "GeoJsonRaster";
const struct luaL_Reg GeoJsonRaster::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"pixel",       luaPixel},
    {NULL,          NULL}
};

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

    /* Create GeoJsonRaster */
    return new GeoJsonRaster(L, file, filelength, _cellsize);
}

/*----------------------------------------------------------------------------
 * subset
 *----------------------------------------------------------------------------*/
bool GeoJsonRaster::subset (double lon, double lat)
{
    OGRPoint p  = {lon, lat};

    if(p.transform(pimpl->latlon2xy) == OGRERR_NONE)
    {
        lon = p.getX();
        lat = p.getY();

        if ((lon >= bbox.lon_min) &&
            (lon <= bbox.lon_max) &&
            (lat >= bbox.lat_min) &&
            (lat <= bbox.lat_max))
        {
            uint32_t row = (bbox.lat_max - lat) / cellsize;
            uint32_t col = (lon - bbox.lon_min) / cellsize;

            if ((row < rows) && (col < cols))
            {
                return rawPixel(row, col);
            }
        }
    }
    else
    {
        /*
         * Cannot log this error...
         * Transform failed for probably thousands of pixels in raster.
         */
    }

    return false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::~GeoJsonRaster(void)
{
    if (raster) delete[] raster;
    if (pimpl->latlon2xy) OGRCoordinateTransformation::DestroyCT(pimpl->latlon2xy);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/* Utilitiy function to get UUID string */
static const char *getUuid(char *uuid_str)
{
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
    return uuid_str;
}

/* Utilitiy function to check constructor's params */
static void validatedParams(const char *file, long filelength, double _cellsize)
{
    if (file == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid file pointer (NULL)");

    if (filelength <= 0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid filelength: %ld:", filelength);

    if (_cellsize <= 0.0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid cellsize: %lf:", _cellsize);
}


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoJsonRaster::GeoJsonRaster(lua_State *L, const char *file, long filelength, double _cellsize):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    pimpl{ new impl{} }
{
    char uuid_str[UUID_STR_LEN] = {0};
    bool rasterCreated = false;
    GDALDataset *rasterDset = NULL;
    GDALDataset *jsonDset   = NULL;
    std::string rasterfname = "/vsimem/" + std::string(getUuid(uuid_str));
    std::string jsonfname   = "/vsimem/" + std::string(getUuid(uuid_str));

    /* Initialize Class Data Members */
    raster = NULL;
    rows = 0;
    cols = 0;
    bbox = {0.0, 0.0, 0.0, 0.0};
    cellsize = 0.0;
    pimpl->latlon2xy = NULL;
    pimpl->source.Clear();
    pimpl->target.Clear();

    validatedParams(file, filelength, _cellsize);

    try
    {
        /* Create raster from json file */
        VSILFILE *fp = VSIFileFromMemBuffer(jsonfname.c_str(), (GByte *)file, (vsi_l_offset)filelength, FALSE);
        CHECKPTR(fp);
        VSIFCloseL(fp);

        jsonDset = (GDALDataset *)GDALOpenEx(jsonfname.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        CHECKPTR(jsonDset);
        OGRLayer *srcLayer = jsonDset->GetLayer(0);
        CHECKPTR(srcLayer);

        OGREnvelope e;
        OGRErr ogrerr = srcLayer->GetExtent(&e);
        CHECK_GDALERR(ogrerr);

        cellsize = _cellsize;
        int _cols = int((e.MaxX - e.MinX) / cellsize);
        int _rows = int((e.MaxY - e.MinY) / cellsize);

        char **options = NULL;
        options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");

        GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        CHECKPTR(driver);
        rasterDset = (GDALDataset *)driver->Create(rasterfname.c_str(), _cols, _rows, 1, GDT_Byte, options);
        CSLDestroy(options);
        CHECKPTR(rasterDset);
        double geot[6] = {e.MinX, cellsize, 0, e.MaxY, 0, -cellsize};
        rasterDset->SetGeoTransform(geot);

        OGRSpatialReference *srcSrs = srcLayer->GetSpatialRef();
        CHECKPTR(srcSrs);

        char *wkt;
        srcSrs->exportToWkt(&wkt);
        mlog(DEBUG, "geojson WKT: %s", wkt);
        rasterDset->SetProjection(wkt);
        CPLFree(wkt);

        int bandInx = 1; /* Band index starts at 1, not 0 */
        GDALRasterBand *band = rasterDset->GetRasterBand(bandInx);
        CHECKPTR(band);
        band->SetNoDataValue(RASTER_NODATA_VALUE);

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

        /* Store information about raster */
        cols = rasterDset->GetRasterXSize();
        rows = rasterDset->GetRasterYSize();

        /* Get raster boundry box */
        rasterDset->GetGeoTransform(geot);
        bbox.lon_min = geot[0];
        bbox.lon_max = geot[0] + cols * geot[1];
        bbox.lat_max = geot[3];
        bbox.lat_min = geot[3] + rows * geot[5];

        long size = cols * rows;
        raster = new uint8_t[size];
        CHECKPTR(raster);

        /* Store raster in byte array. */
        cplerr = band->RasterIO(GF_Read, 0, 0, cols, rows, raster, cols, rows, GDT_Byte, 0, 0);
        CHECK_GDALERR(cplerr);

        const char *_wkt = rasterDset->GetProjectionRef();
        CHECKPTR(_wkt);
        ogrerr = pimpl->source.importFromEPSG(RASTER_PHOTON_CRS);
        CHECK_GDALERR(ogrerr);
        ogrerr = pimpl->target.importFromWkt(_wkt);
        CHECK_GDALERR(ogrerr);

        /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
        pimpl->target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        pimpl->source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        /* Create coordinates transformation */
        pimpl->latlon2xy = OGRCreateCoordinateTransformation(&pimpl->source, &pimpl->target);
        CHECKPTR(pimpl->latlon2xy);

        rasterCreated = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoJsonRaster: %s", e.what());
    }

   /* Cleanup */
   VSIUnlink(jsonfname.c_str());
   VSIUnlink(rasterfname.c_str());

   if (jsonDset) GDALClose((GDALDatasetH)jsonDset);
   if (rasterDset) GDALClose((GDALDatasetH)rasterDset);

    if(!rasterCreated)
        throw RunTimeException(CRITICAL, RTE_ERROR, "GeoJsonRaster failed");
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GeoJsonRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoJsonRaster *lua_obj = (GeoJsonRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->rows);
        lua_pushinteger(L, lua_obj->cols);
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
int GeoJsonRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoJsonRaster *lua_obj = (GeoJsonRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->bbox.lon_min);
        lua_pushnumber(L, lua_obj->bbox.lat_min);
        lua_pushnumber(L, lua_obj->bbox.lon_max);
        lua_pushnumber(L, lua_obj->bbox.lat_max);
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
int GeoJsonRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoJsonRaster *lua_obj = (GeoJsonRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->cellsize);
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
 * luaPixel - :pixel(r, c) --> on|off
 *----------------------------------------------------------------------------*/
int GeoJsonRaster::luaPixel(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoJsonRaster *lua_obj = (GeoJsonRaster *)getLuaSelf(L, 1);

        /* Get Pixel Index */
        uint32_t r = getLuaInteger(L, 2);
        uint32_t c = getLuaInteger(L, 3);

        /* Get Pixel */
        if ((r < lua_obj->rows) && (c < lua_obj->cols))
        {
            lua_pushboolean(L, lua_obj->rawPixel(r, c));
            num_ret++;

            /* Set Return Status */
            status = true;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid index provided <%d, %d>", r, c);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting pixel: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaSubset - :subset(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int GeoJsonRaster::luaSubset(lua_State *L)
{
    bool status = false;

    try
    {
        /* Get Self */
        GeoJsonRaster *lua_obj = (GeoJsonRaster *)getLuaSelf(L, 1);

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get Inclusion */
        status = lua_obj->subset(lon, lat);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error subsetting: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
