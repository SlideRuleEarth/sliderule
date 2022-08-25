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
#include "GdalRaster.h"
#include <uuid/uuid.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdalwarper.h>



/******************************************************************************
 * LOCAL DEFINES AND MACROS
 ******************************************************************************/

#define CHECKPTR(p)                                                         \
    do                                                                      \
    {                                                                       \
        if ((p) == NULL)                                                    \
        {                                                                   \
            mlog(CRITICAL, "NULL pointer detected, constructor failed!\n"); \
            assert(p);                                                      \
            return;                                                         \
        }                                                                   \
    } while (0)

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GdalRaster::LuaMetaName = "GdalRaster";
const struct luaL_Reg GdalRaster::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"pixel",       luaPixel},
    {NULL,          NULL}
};

const char* GdalRaster::FILEDATA_KEY   = "data";
const char* GdalRaster::FILELENGTH_KEY = "length";
const char* GdalRaster::FILETYPE_KEY   = "type";
const char* GdalRaster::CELLSIZE_KEY   = "cellsize";
const char* GdalRaster::DIMENSION_KEY  = "dimension";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - file(
 *  {
 *      file=<file>,
 *      filelength=<filelength>,
 *      filetype=<file type>]
 *  })
 *----------------------------------------------------------------------------*/
int GdalRaster::luaCreate (lua_State* L)
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
GdalRaster* GdalRaster::create (lua_State* L, int index)
{
    /* Get file */
    lua_getfield(L, index, FILEDATA_KEY);
    const char* file = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get file Length */
    lua_getfield(L, index, FILELENGTH_KEY);
    size_t filelength = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Get file type */
    lua_getfield(L, index, FILETYPE_KEY);
    long filetype = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Get cellsize */
    lua_getfield(L, index, CELLSIZE_KEY);
    double _cellsize = getLuaFloat(L, -1);
    lua_pop(L, 1);

    std::string datafile;

    if( filetype == GEOJSON )
        datafile = file;  /* Text */
    else
    {
        /* Convert file from Base64 to Binary */
        datafile = MathLib::b64decode(file, filelength);
    }

    /* Create GdalRaster */
    return new GdalRaster(L, datafile.c_str(), datafile.size(), filetype, _cellsize);
}

/*----------------------------------------------------------------------------
 * subset
 *----------------------------------------------------------------------------*/
bool GdalRaster::subset (double lon, double lat)
{
    OGRPoint p  = {lon, lat};

    if(!latlon2xy) return false; /* Constructor failed */

    if(p.transform(latlon2xy) == OGRERR_NONE)
    {
        lon = p.getX();
        lat = p.getY();

        if ((lon >= bbox.lon_min) &&
            (lon <= bbox.lon_max) &&
            (lat >= bbox.lat_min) &&
            (lat <= bbox.lat_max))
        {
            uint32_t row = (bbox.lat_max - lat) / abs(lat_cellsize);
            uint32_t col = (lon - bbox.lon_min) / abs(lon_cellsize);

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
GdalRaster::~GdalRaster(void)
{
    VSIUnlink(jsonfname.c_str());
    VSIUnlink(rasterfname.c_str());

    if (raster) CPLFree(raster);
    if (jsonDset) GDALClose((GDALDatasetH)jsonDset);
    if (rasterDset) GDALClose((GDALDatasetH)rasterDset);
    if (latlon2xy) OGRCoordinateTransformation::DestroyCT(latlon2xy);
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

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GdalRaster::GdalRaster(lua_State *L, const char *file, long filelength, long _filetype, double _cellsize):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    char uuid_str[UUID_STR_LEN] = {0};
    VSILFILE *fp = NULL;

    /* Initialize Class Data Members */
    raster = NULL;
    rows = 0;
    cols = 0;
    bands = 0;
    bbox = {0.0, 0.0, 0.0, 0.0};
    lon_cellsize = 0;
    lat_cellsize = 0;
    cellsize = _cellsize;
    filetype = (file_t)_filetype;
    pixelon = GDALRASTER_PIXEL_ON;
    latlon2xy = NULL;
    rasterDset = NULL;
    jsonDset = NULL;
    source.Clear();
    target.Clear();
    rasterfname = "/vsimem/" + std::string(getUuid(uuid_str));
    jsonfname   = "/vsimem/" + std::string(getUuid(uuid_str));

    /* Must have valid file */
    CHECKPTR(file);

    if (filetype == GEOJSON)
    {
        /* Create raster from json file */

        if( !(cellsize > 0.0) )
        {
            mlog(CRITICAL, "Cannot create raster with cellsize: %lf\n", cellsize);
            return;
        }

        fp = VSIFileFromMemBuffer(jsonfname.c_str(), (GByte *)file, (vsi_l_offset)filelength, FALSE);
        CHECKPTR(fp);
        VSIFCloseL(fp);

        jsonDset = (GDALDataset *)GDALOpenEx(jsonfname.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        CHECKPTR(jsonDset);
        OGRLayer *srcLayer = jsonDset->GetLayer(0);
        CHECKPTR(srcLayer);

        GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        CHECKPTR(driver);

        OGREnvelope e;
        OGRErr err = srcLayer->GetExtent(&e);
        if (err != OGRERR_NONE)
        {
            mlog(CRITICAL, "Failed to get geojson file extent\n");
            return;
        }
        int x_ncells = int((e.MaxX - e.MinX) / cellsize);
        int y_ncells = int((e.MaxY - e.MinY) / cellsize);

        char **options = NULL;
        options = CSLSetNameValue(options, "COMPRESS", "DEFLATE");

        rasterDset = (GDALDataset *)driver->Create(rasterfname.c_str(), x_ncells, y_ncells, 1, GDT_Byte, options);
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
        band->SetNoDataValue(GDALRASTER_NODATA_VALUE);

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
        burnValues[0] = GDALRASTER_PIXEL_ON;

        CPLErr er = GDALRasterizeLayers(rasterDset, 1, bandlist, 1, (OGRLayerH *)&layers[0], NULL, NULL, burnValues, NULL, NULL, NULL);
        if (er != CPLE_None)
            mlog(CRITICAL, "Failed to rasterize geojson file\n");

        GDALClose(rasterDset);
        rasterDset = NULL;
    }
    else if (filetype == GEOTIF)
    {
        fp = VSIFileFromMemBuffer(rasterfname.c_str(), (GByte *)file, (vsi_l_offset)filelength, FALSE);
        CHECKPTR(fp);
        VSIFCloseL(fp);
    }
    else
    {
        mlog(CRITICAL, "Invalid file type!\n");
        return; /* Must have valid file */
    }

    /*
     * Open raster (newly created or given as parameter)
     * Read it into byte array and create lat/lon transform function.
     */
    rasterDset = (GDALDataset *)GDALOpen(rasterfname.c_str(), GA_ReadOnly);
    CHECKPTR(rasterDset);

    cols  = rasterDset->GetRasterXSize();
    rows  = rasterDset->GetRasterYSize();
    bands = rasterDset->GetRasterCount();

    if (bands == 0)
    {
        mlog(CRITICAL, "Raster has no bands!\n");
        return; /* Must have at least one band */
    }

    /* Get raster boundry box and cell size */
    double geot[6];
    rasterDset->GetGeoTransform(geot);
    bbox.lon_min = geot[0];
    bbox.lon_max = geot[0] + cols * geot[1];
    bbox.lat_max = geot[3];
    bbox.lat_min = geot[3] + rows * geot[5];

    lon_cellsize = geot[1];
    lat_cellsize = geot[5];

    long size = cols * rows;
    raster = (uint8_t *)CPLMalloc(sizeof(uint8_t) * size);
    CHECKPTR(raster);

    int bandInx = 1; /* Band index starts at 1, not 0 */
    GDALRasterBand *band = rasterDset->GetRasterBand(bandInx);
    CHECKPTR(band);

    /* Get raster band stats */
    double min, max, mean, stdev;
    OGRErr err = band->GetStatistics(false, true, &min, &max, &mean, &stdev);
    if (err != OGRERR_NONE)
    {
        mlog(CRITICAL, "Failed to get raster stats\n");
        return;
    }

    /*
     * Do sanity check on raster not created from json file.
     * Raster's first band must be an on/off mask.
     * Double comparison only works if the values are trully the same
     * (created from integers or constants)
     */
    if((min != max) || (min != mean) || (stdev != 0.0))
    {
        mlog(CRITICAL, "Unsuported raster, first band must be a mask\n");
        return;
    }

    /* Set pixelon marker for this raster */
    pixelon = max;

    /* Store raster in byte array.
     * Raster original data will be converted automaticly to GDT_Byte.
     */
    OGRErr e0 = band->RasterIO(GF_Read, 0, 0, cols, rows, raster, cols, rows, GDT_Byte, 0, 0);

    const char *wkt = rasterDset->GetProjectionRef();
    CHECKPTR(wkt);
    OGRErr e1 = source.importFromEPSG(GDALRASTER_PHOTON_CRS);
    OGRErr e2 = target.importFromWkt(wkt);

    if (((e0 == e1) == e2) == OGRERR_NONE)
    {
        /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
        target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        /* Create coordinates transformation */
        latlon2xy = OGRCreateCoordinateTransformation(&source, &target);
        CHECKPTR(latlon2xy);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GdalRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster *lua_obj = (GdalRaster *)getLuaSelf(L, 1);

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
int GdalRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster *lua_obj = (GdalRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->bbox.lon_min);
        lua_pushinteger(L, lua_obj->bbox.lat_min);
        lua_pushinteger(L, lua_obj->bbox.lon_max);
        lua_pushinteger(L, lua_obj->bbox.lat_max);
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
int GdalRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster *lua_obj = (GdalRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->lat_cellsize);
        lua_pushnumber(L, lua_obj->lon_cellsize);
        num_ret += 2;

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
int GdalRaster::luaPixel(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster *lua_obj = (GdalRaster *)getLuaSelf(L, 1);

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
int GdalRaster::luaSubset(lua_State *L)
{
    bool status = false;

    try
    {
        /* Get Self */
        GdalRaster *lua_obj = (GdalRaster *)getLuaSelf(L, 1);

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
