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
#include <tiffio.h>

#include <gdal.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <gdal_priv.h>
#include <cpl_string.h>
#include <cpl_conv.h>
#include <gdalwarper.h>
#include <cpl_vsi.h>

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
    const char* image = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get file Length */
    lua_getfield(L, index, FILELENGTH_KEY);
    size_t filelength = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Get file type */
    lua_getfield(L, index, FILETYPE_KEY);
    long filetype = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Convert Image from Base64 to Binary */
    std::string tiff = MathLib::b64decode(image, filelength);

    /* Create GdalRaster */
    return new GdalRaster(L, tiff.c_str(), tiff.size(), filetype);
}

/*----------------------------------------------------------------------------
 * subset
 *----------------------------------------------------------------------------*/
bool GdalRaster::subset (double lon, double lat)
{
    OGRPoint p  = {lon, lat};

    if(!latlon2xy) return false;

    OGRErr eErr = p.transform(latlon2xy);
    if(eErr != OGRERR_NONE)
    {
        mlog(CRITICAL, "Raster lat/lon transformation failed with error: %d\n", eErr);
    }

    static bool first_hit = true;
    if (first_hit)
    {
        first_hit = false;

        print2term("\n\nEPSG is: %u\n", epsg);
        print2term("lon: %lf, lat: %lf, x: %lf, y: %lf\n", lon, lat, p.getX(), p.getY());
        print2term("blon_min: %lf, blon_max: %lf, blat_min: %lf, blat_max: %lf\n\n", bbox.lon_min, bbox.lon_max, bbox.lat_min, bbox.lat_max);
        print2term("\n\n");
    }

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
    return false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GdalRaster::~GdalRaster(void)
{
    if(raster) CPLFree(raster);
    if(latlon2xy) OGRCoordinateTransformation::DestroyCT(latlon2xy);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GdalRaster::GdalRaster(lua_State* L, const char* image, long imagelength, long _filetype):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    OGRErr eErr;

    /* Initialize Class Data Members */
    raster = NULL;
    rows = 0;
    cols = 0;
    bbox = {0.0, 0.0, 0.0, 0.0 };
    lon_cellsize = 0;
    lat_cellsize = 0;
    epsg = 0;
    latlon2xy = NULL;
    source.Clear();
    target.Clear();

    /* Must have valid file */
    if(!image || imagelength == 0) {mlog(CRITICAL, "Invalid file type!\n"); return;} 
    
    const char  *format = "GTiff";
    const char  *fname  = "/vsimem/temp.tif";

    filetype = (file_t)_filetype;
    switch (filetype)
    {
    case GEOJSON:
    {
        /* Create raster from json file, store it in 'fname' */
        break;
    }

    case ESRISHAPE:
    {
        /* Create raster from shape file, store it in 'fname' */
        break;
    }

    case GEOTIF:
    {
        VSILFILE *fp = VSIFileFromMemBuffer(fname, (GByte *)image, (vsi_l_offset)imagelength, FALSE);
        if (!fp) {mlog(CRITICAL, "Failed creating memFile\n"); return;}
        VSIFCloseL(fp);
        break;
    }

    default:
    {
        mlog(CRITICAL, "Invalid file type!\n");
        return; /* Must have valid file */
    }
    }

    /* 
     * Store raster in byte array for fast lookup. 
     * Create transform for lat lon to raster projection.
     */

    GDALDataset *dataset= (GDALDataset*) GDALOpen(fname, GA_ReadOnly);
    if(!dataset) {mlog(CRITICAL, "Failed creating dataset\n"); return;}  
    
    const OGRSpatialReference *sref = (OGRSpatialReference *)GDALGetSpatialRef(dataset);
    if(!dataset) {mlog(CRITICAL, "Failed GDALGetSpatialRef\n"); return;}  

    /* Set projection */    
    epsg = sref->GetEPSGGeogCS();

    long bands = dataset->GetBands().size();
    if(bands == 0)
    {
        mlog(CRITICAL, "Raster has no bands!\n");
        return;  /* Must have at least one band */
    }
    else if(bands > 1)
        mlog(WARNING, "Raster has: %ld bands, using first band\n", bands);

    GDALRasterBand *band = dataset->GetRasterBand(1);
    assert(band);

    cols = band->GetXSize();
    rows = band->GetYSize();

    double adfGeoTransform[6];

    /* Get raster boundry box and cell/pixel size */
    dataset->GetGeoTransform(adfGeoTransform);
    bbox.lon_min = adfGeoTransform[0];
    bbox.lon_max = adfGeoTransform[0] + cols * adfGeoTransform[1];
    bbox.lat_max = adfGeoTransform[3];      
    bbox.lat_min = adfGeoTransform[3] + rows * adfGeoTransform[5];

    lon_cellsize = adfGeoTransform[1];
    lat_cellsize = adfGeoTransform[5];

    long size = cols * rows;
    raster = (uint8_t*)CPLMalloc(sizeof(uint8_t)*size);
    assert(raster); 
 
    /*
     * RasterIO: acording to GDAL docs, the raster original data will be converted
     * automaticly to GDT_Byte by RasterIO call below.
     */
    eErr = band->RasterIO(GF_Read, 0, 0, cols, rows, raster, cols, rows, GDT_Byte, 0, 0);
    if (eErr != OGRERR_NONE)
    {
        mlog(CRITICAL, "RasterIO failed with error: %d\n", eErr);
    }

    GDALClose((GDALDatasetH) dataset);
    VSIUnlink(fname);

    /* Create coordinates transformation */    
    source.importFromEPSG(GDALRASTER_PHOTON_CRS);
    target.importFromEPSG(epsg);
    target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    latlon2xy = OGRCreateCoordinateTransformation(&source, &target);
    if(!latlon2xy)
    {
        mlog(CRITICAL, "Failed to create projection from EPSG:%d to EPSG:%d\n", GDALRASTER_PHOTON_CRS, epsg);
    } 
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GdalRaster::luaDimensions (lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster* lua_obj = (GdalRaster*)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->rows);
        lua_pushinteger(L, lua_obj->cols);
        num_ret += 2;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting dimensions: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaBoundingBox - :bbox() --> (lon_min, lat_min, lon_max, lat_max)
 *----------------------------------------------------------------------------*/
int GdalRaster::luaBoundingBox(lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster* lua_obj = (GdalRaster*)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->bbox.lon_min);
        lua_pushinteger(L, lua_obj->bbox.lat_min);
        lua_pushinteger(L, lua_obj->bbox.lon_max);
        lua_pushinteger(L, lua_obj->bbox.lat_max);
        num_ret += 4;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting bounding box: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaCellSize - :cell() --> cell size
 *----------------------------------------------------------------------------*/
int GdalRaster::luaCellSize(lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster* lua_obj = (GdalRaster*)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->lat_cellsize);
        lua_pushnumber(L, lua_obj->lon_cellsize);
        num_ret += 2;

        /* Set Return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting cell size: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaPixel - :pixel(r, c) --> on|off
 *----------------------------------------------------------------------------*/
int GdalRaster::luaPixel (lua_State* L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GdalRaster* lua_obj = (GdalRaster*)getLuaSelf(L, 1);

        /* Get Pixel Index */
        uint32_t r = getLuaInteger(L, 2);
        uint32_t c = getLuaInteger(L, 3);

        /* Get Pixel */
        if((r < lua_obj->rows) && (c < lua_obj->cols))
        {
            lua_pushboolean(L, lua_obj->rawPixel(r,c));
            num_ret++;

            /* Set Return Status */
            status = true;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid index provided <%d, %d>", r, c);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting pixel: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaSubset - :subset(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int GdalRaster::luaSubset (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        GdalRaster* lua_obj = (GdalRaster*)getLuaSelf(L, 1);

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get Inclusion */
        status = lua_obj->subset(lon, lat);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error subsetting: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
