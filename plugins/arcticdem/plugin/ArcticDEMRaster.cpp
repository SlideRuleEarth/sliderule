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
#include "ArcticDEMRaster.h"

#include <uuid/uuid.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <gdal.h>
#include <gdalwarper.h>
#include <ogr_spatialref.h>
#include <gdal_priv.h>


#define USE_CACHE

/******************************************************************************
 * LOCAL DEFINES AND MACROS
 ******************************************************************************/

#define CHECKPTR(p)                                                           \
do                                                                            \
{                                                                             \
    if ((p) == NULL)                                                          \
    {                                                                         \
        assert(p);                                                            \
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


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArcticDEMRaster::LuaMetaName = "ArcticDEMRaster";
const struct luaL_Reg ArcticDEMRaster::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"pixel",       luaPixel},
    {"subset",      luaSubset},
    {NULL,          NULL}
};

const char* ArcticDEMRaster::FILEDATA_KEY   = "data";
const char* ArcticDEMRaster::FILELENGTH_KEY = "length";
const char* ArcticDEMRaster::CELLSIZE_KEY   = "cellsize";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::init (void)
{
    /* Register all gdal drivers */
    GDALAllRegister();
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::deinit (void)
{
    GDALDestroy();
}

/*----------------------------------------------------------------------------
 * luaCreate - file(
 *  {
 *      file=<file>,
 *      filelength=<filelength>,
 *  })
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaCreate (lua_State* L)
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
ArcticDEMRaster* ArcticDEMRaster::create (lua_State* L, int index)
{
#if 0
    //TODO: may need to start with some known region for optimization
    //      creating first raster with user's ROI

    /* Get geojson file */
    lua_getfield(L, index, FILEDATA_KEY);
    const char* file = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get geojson file Length */
    lua_getfield(L, index, FILELENGTH_KEY);
    size_t filelength = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);
    /* Create ArcticDEMRaster */
    return new ArcticDEMRaster(L, file, filelength);
#else
    return new ArcticDEMRaster(L, NULL, 0);
#endif

}


/*----------------------------------------------------------------------------
 * readRaster
 *----------------------------------------------------------------------------*/
bool ArcticDEMRaster::readRaster(OGRPoint* p, bool findNewRaster)
{
    GDALDataset *idset = NULL;
    GDALDataset *rdset = NULL;
    bool rasterFound = false;
    bool rasterRead  = false;

    try
    {
        if(findNewRaster)
        {
            /* Open index shapefile and find raster tile */
            idset = (GDALDataset *)GDALOpenEx(indexfname.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
            CHECKPTR(idset);
            OGRLayer *ilayer = idset->GetLayer(0);
            CHECKPTR(ilayer);

            OGRFeature *feature;
            ilayer->ResetReading();
            while ((feature = ilayer->GetNextFeature()) != NULL)
            {
                const OGRGeometry *geo = feature->GetGeometryRef();
                CHECKPTR(geo);
                if (geo->getGeometryType() == wkbPolygon)
                {
                    OGRPolygon *poly = (OGRPolygon *)geo;
                    CHECKPTR(poly);
                    if (poly->Contains(p))
                    {
                        /* Found polygon with point in it, get the name of raster directory/file */
                        std::string rname = feature->GetFieldAsString("name");
                        rasterfname += rname + '/' + rname + "_reg_dem.tif";
                        print2term("Found raster %s at %0.2f, %0.2f\n", rasterfname.c_str(), p->getX(), p->getY());
                        rasterFound = true;
                        break;
                    }
                }
                OGRFeature::DestroyFeature(feature);
            }
        } else rasterFound = true;

        /* Open the raster, read data into cache for fast lookup */
        if(rasterFound)
        {
            rdset = (GDALDataset *)GDALOpenEx(rasterfname.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
            CHECKPTR(rdset);

            /* Store information about raster */
            rrows = rdset->GetRasterYSize();
            rcols = rdset->GetRasterXSize();
            print2term("Raster rows: %d, cols: %d\n", rrows, rcols);

            /* Get raster boundry box */
            double geot[6] = {0, 0, 0, 0, 0, 0};
            rdset->GetGeoTransform(geot);
            rbbox.lon_min = geot[0];
            rbbox.lon_max = geot[0] + rcols * geot[1];
            rbbox.lat_max = geot[3];
            rbbox.lat_min = geot[3] + rrows * geot[5];

            cellsize = geot[1];
            print2term("Raster rbbox.lon_min/max %0.2lf %0.2lf, rbbox.lat_min/max %0.2lf %0.2lf, cellsize: %lf\n",
                       rbbox.lon_min, rbbox.lon_max, rbbox.lat_min, rbbox.lat_max, cellsize);

            /* Which row/col of raster do we need? */
            uint32_t row = (rbbox.lat_max - p->getY()) / cellsize;
            uint32_t col = (p->getX() - rbbox.lon_min) / cellsize;
            print2term("Need row: %u, col: %u\n", row, col);

            /* Calculate raster cache offsets and rows/cols to read */
            int crow_offset = row-CACHE_MAX_ROWS_OFFSET;
            if(crow_offset<0)
                crow_offset = 0;

            int rows2read = CACHE_MAX_ROWS;
            if(crow_offset+rows2read > rrows)
                rows2read = rrows-crow_offset;

            int ccol_offset = col-CACHE_MAX_COLS_OFFSET;
            if(ccol_offset<0)
                ccol_offset = 0;

            int cols2read = CACHE_MAX_COLS;
            if(ccol_offset+cols2read > rcols)
                cols2read = rcols-ccol_offset;

            crows = rows2read;
            ccols = cols2read;
            crows_offset = crow_offset;
            ccols_offset = ccol_offset;

            print2term("Cache row_offset: %d, col_offset: %d, rows2read: %d, cols2read: %d\n",
                        crow_offset, ccol_offset, crows, ccols);

            int bandInx = 1; /* Band index starts at 1, not 0 */
            GDALRasterBand *band = rdset->GetRasterBand(bandInx);
            CHECKPTR(band);
            CPLErr cplerr;

#ifdef USE_CACHE
            /* Store cached part of raster in array. */
            cplerr = band->RasterIO(GF_Read, ccols_offset, crows_offset, ccols, crows, raster_cache,
                                    CACHE_MAX_COLS, CACHE_MAX_ROWS, GDT_Float32, 0, 0);
            CHECK_GDALERR(cplerr);
#else
            long size = rcols * rrows;
            raster = new float[size];
            CHECKPTR(raster);

            /* Store raster in array. */
            cplerr = band->RasterIO(GF_Read, 0, 0, rcols, rrows, raster, rcols, rrows, GDT_Float32, 0, 0);
            CHECK_GDALERR(cplerr);
#endif
            rasterRead = true;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ArcticDEMRaster: %s", e.what());
    }

   /* Cleanup */

   if (idset) GDALClose((GDALDatasetH)idset);
   if (rdset) GDALClose((GDALDatasetH)rdset);

    if(!rasterRead)
        throw RunTimeException(CRITICAL, RTE_ERROR, "ArcticDEMRaster failed");

    return rasterRead;
}


/*----------------------------------------------------------------------------
 * subset
 *----------------------------------------------------------------------------*/
float ArcticDEMRaster::subset (double lon, double lat)
{
    OGRPoint p  = {lon, lat};

    if(p.transform(latlon2xy) == OGRERR_NONE)
    {
        lon = p.getX();
        lat = p.getY();

GET_ELEV:
        if ((lon >= rbbox.lon_min) &&
            (lon <= rbbox.lon_max) &&
            (lat >= rbbox.lat_min) &&
            (lat <= rbbox.lat_max))
        {
            uint32_t row = (rbbox.lat_max - lat) / cellsize;
            uint32_t col = (lon - rbbox.lon_min) / cellsize;

#ifdef USE_CACHE
            if ((row >= crows_offset) && (row < crows_offset+crows) &&
                (col >= ccols_offset) && (col < ccols_offset+ccols))
            {
                return cacheRawPixel(row, col);
            }
            else
            {
                /* Point is in this raster but not in cache */
                if (readRaster(&p, false)) goto GET_ELEV;
                else return ARCTIC_DEM_INVALID_EL;
            }
#else
            if ((row < rrows) && (col < rcols)) return rawPixel(row, col);
#endif
        }
        else
        {
            /* Point not in the raster */
            if( readRaster(&p, true) ) goto GET_ELEV;
            else return ARCTIC_DEM_INVALID_EL;
        }

    }

    return ARCTIC_DEM_INVALID_EL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArcticDEMRaster::~ArcticDEMRaster(void)
{
    if (raster_cache) delete[] raster_cache;
    if (raster) delete[] raster;
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

/* Utilitiy function to check constructor's params */
static void validatedParams(const char *file, long filelength)
{
    if (file == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid file pointer (NULL)");

    if (filelength <= 0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid filelength: %ld:", filelength);
}


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArcticDEMRaster::ArcticDEMRaster(lua_State *L, const char *file, long filelength):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    char uuid_str[UUID_STR_LEN] = {0};
    bool objCreated = false;
    GDALDataset *idset = NULL;

    /* Initialize Class Data Members */
    raster = NULL;
    rbbox = {0.0, 0.0, 0.0, 0.0};
    rrows = 0;
    rcols = 0;

    raster_cache = NULL;
    crows = 0;
    ccols = 0;
    crows_offset = 0;
    ccols_offset = 0;
    cellsize = 0.0;
    latlon2xy = NULL;
    source.Clear();
    target.Clear();

    try
    {
        idset = (GDALDataset *)GDALOpenEx(indexfname.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        CHECKPTR(idset);
        OGRLayer *ilayer = idset->GetLayer(0);
        CHECKPTR(ilayer);

        OGRSpatialReference *srcSrs = ilayer->GetSpatialRef();
        CHECKPTR(srcSrs);
        //srcSrs->dumpReadable();

        char *wkt;
        srcSrs->exportToWkt(&wkt);
        mlog(DEBUG, "indexfile WKT: %s", wkt);

        OGRErr ogrerr = source.importFromEPSG(RASTER_PHOTON_CRS);
        CHECK_GDALERR(ogrerr);
        ogrerr = target.importFromWkt(wkt);
        CHECK_GDALERR(ogrerr);
        CPLFree(wkt);

        /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
        target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        /* Create coordinates transformation */
        latlon2xy = OGRCreateCoordinateTransformation(&source, &target);
        CHECKPTR(latlon2xy);

        /* Allocate memory for raster cache */
        long size = RASTER_MAX_SIZE / sizeof(float);
        raster_cache = new float[size];

        /* Preload cache, this may be done based on poligon past to constructor */
        double lat =  82.86;
        double lon = -74.60;
        subset(lon, lat);

        objCreated = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ArcticDEMRaster: %s", e.what());
    }

   /* Cleanup */

   if (idset) GDALClose((GDALDatasetH)idset);

    if(!objCreated)
        throw RunTimeException(CRITICAL, RTE_ERROR, "ArcticDEMRaster failed");
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->rrows);
        lua_pushinteger(L, lua_obj->rcols);
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
int ArcticDEMRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->rbbox.lon_min);
        lua_pushnumber(L, lua_obj->rbbox.lat_min);
        lua_pushnumber(L, lua_obj->rbbox.lon_max);
        lua_pushnumber(L, lua_obj->rbbox.lat_max);
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
int ArcticDEMRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

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
int ArcticDEMRaster::luaPixel(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Get Pixel Index */
        uint32_t r = getLuaInteger(L, 2);
        uint32_t c = getLuaInteger(L, 3);

        /* Get Pixel */
        if ((r < lua_obj->rrows) && (c < lua_obj->rcols))
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
int ArcticDEMRaster::luaSubset(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

        /* Get Coordinates */
        double lon = getLuaFloat(L, 2);
        double lat = getLuaFloat(L, 3);

        /* Get Elevation */
        float el = lua_obj->subset(lon, lat);
        lua_pushnumber(L, el);
        num_ret++;

        if( el != ARCTIC_DEM_INVALID_EL )
            status = true;

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error subsetting: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}
