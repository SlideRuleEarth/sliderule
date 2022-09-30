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


/******************************************************************************
 * LOCAL DEFINES AND MACROS
 ******************************************************************************/

#define CHECKPTR(p)                                                           \
do                                                                            \
{                                                                             \
    assert(p);                                                                \
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


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArcticDEMRaster::LuaMetaName = "ArcticDEMRaster";
const struct luaL_Reg ArcticDEMRaster::LuaMetaTable[] = {
    {"dim",         luaDimensions},
    {"bbox",        luaBoundingBox},
    {"cell",        luaCellSize},
    {"sample",      luaSample},
    {"samples",     luaSamples},
    {NULL,          NULL}
};


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
 * luaCreate
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
    const int radius = getLuaInteger(L, -1);
    lua_pop(L, 1);
    const char* dem_sampling = getLuaString(L, -1);
    lua_pop(L, 1);
    const char* dem_type = getLuaString(L, -1);
    lua_pop(L, 1);
    return new ArcticDEMRaster(L, dem_type, dem_sampling, radius);
}


/*----------------------------------------------------------------------------
 * sample
 *----------------------------------------------------------------------------*/
float ArcticDEMRaster::sample (double lon, double lat)
{
    OGRPoint p  = {lon, lat};

    if(p.transform(latlon2xy) == OGRERR_NONE)
    {
        lon = p.getX();
        lat = p.getY();

        if ( rdset &&
            (lon >= bbox.lon_min) &&
            (lon <= bbox.lon_max) &&
            (lat >= bbox.lat_min) &&
            (lat <= bbox.lat_max))
        {
            return readRaster(&p);
        }
        else
        {
            /* Point not in the raster */
            findNewRaster(&p);
            return readRaster(&p);
        }
    }

    return ARCTIC_DEM_INVALID_ELELVATION;
}


/*----------------------------------------------------------------------------
 * elevations
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::samples(double lon, double lat, List<ArcticDEMRaster::elevation_t>& elist)
{
    OGRPoint p  = {lon, lat};
    elevation_t el;

    elist.clear();

    if (ismosaic)
    {
        el.value    = sample(lon, lat);
        el.filename = rasterfname;
        elist.add(el);
    }
    else
    {
        if (p.transform(latlon2xy) == OGRERR_NONE)
        {
            bool reset = true;
            while (findNewRaster(&p, reset))
            {
                el.value    = readRaster(&p);
                el.filename = rasterfname;
                elist.add(el);
                reset = false; /* Don't reset feature reading ptr */
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArcticDEMRaster::~ArcticDEMRaster(void)
{
    if (idset) GDALClose((GDALDatasetH)idset);
    if (rdset) GDALClose((GDALDatasetH)rdset);
    if (latlon2xy) OGRCoordinateTransformation::DestroyCT(latlon2xy);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * findNewRaster
 *----------------------------------------------------------------------------*/
bool ArcticDEMRaster::findNewRaster(OGRPoint* p, bool reset)
{
    bool rasterFound = false;
    std::string filename;

    try
    {
        /* Close existing raster */
        if (rdset)
        {
            GDALClose((GDALDatasetH)rdset);
            rdset = NULL;
            bbox = {0.0, 0.0, 0.0, 0.0};
            cellsize = rows = cols = xblocksize = yblocksize = 0;
            rasterfname.clear();
        }

        /* Index shapefile was already opened in constructor */
        OGRFeature *feature;
        if(reset) ilayer->ResetReading();
        while ((feature = ilayer->GetNextFeature()) != NULL)
        {
            const OGRGeometry *geo = feature->GetGeometryRef();
            CHECKPTR(geo);
            if (geo->getGeometryType() == wkbPolygon)
            {
                const OGRPolygon *poly = (OGRPolygon *)geo;
                CHECKPTR(poly);
                if (poly->Contains(p))
                {
                    /* Found polygon with point in it, get the name of raster directory/file */
                    std::string name = feature->GetFieldAsString("name");
                    if (ismosaic)
                    {
                        rasterfname = name + "_reg_dem.tif";
                        filename = "/data/ArcticDEM/mosaic/" + name + "/" + rasterfname;
                    }
                    else
                    {
                        rasterfname = name + "_dem.tif";
                        filename = "/data/ArcticDEM/strip/" + name + "/" + rasterfname;
                    }

                    mlog(DEBUG, "Raster %s, point(%0.2f, %0.2f)", filename.c_str(), p->getX(), p->getY());
                    rasterFound = true;
                    break;
                }
            }
            OGRFeature::DestroyFeature(feature);
        }

        /* Open new raster, store it's info */
        if (rasterFound)
        {
            rdset = (GDALDataset *)GDALOpenEx(filename.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
            CHECKPTR(rdset);

            /* Store information about raster */
            rows = rdset->GetRasterYSize();
            cols = rdset->GetRasterXSize();

            /* Get raster boundry box */
            double geot[6] = {0, 0, 0, 0, 0, 0};
            rdset->GetGeoTransform(geot);
            bbox.lon_min = geot[0];
            bbox.lon_max = geot[0] + cols * geot[1];
            bbox.lat_max = geot[3];
            bbox.lat_min = geot[3] + rows * geot[5];

            cellsize = geot[1];

            /* Get raster block size */
            GDALRasterBand *band = rdset->GetRasterBand(1);
            CHECKPTR(band);
            band->GetBlockSize(&xblocksize, &yblocksize);
            mlog(DEBUG, "Raster xblocksize: %d, yblocksize: %d", xblocksize, yblocksize);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ArcticDEMRaster: %s", e.what());
    }

    return rasterFound;
}


/*----------------------------------------------------------------------------
 * readRaster
 *----------------------------------------------------------------------------*/
float ArcticDEMRaster::readRaster(OGRPoint* p)
{
    float elevation = ARCTIC_DEM_INVALID_ELELVATION;

    try
    {
        if(rdset)
        {
            /* Raster row, col for point */
            const uint32_t col = (p->getX() - bbox.lon_min) / cellsize;
            const uint32_t row = (bbox.lat_max - p->getY()) / cellsize;

            GDALRasterBand *band = rdset->GetRasterBand(1);
            CHECKPTR(band);

            /* Use fast 'lookup' method for nearest neighbour. */
            if (algorithm == GRIORA_NearestNeighbour)
            {
                /* Raster offsets to block of interest */
                uint32_t xblk = col / xblocksize;
                uint32_t yblk = row / yblocksize;

                GDALRasterBlock *block = band->GetLockedBlockRef(xblk, yblk, false);
                CHECKPTR(block);

                float *p = (float *)block->GetDataRef();
                CHECKPTR(p);

                /* Block row, col for point */
                uint32_t _col = col % xblocksize;
                uint32_t _row = row % yblocksize;
                uint32_t offset = _row * xblocksize + _col;

                elevation = p[offset];
                block->DropLock();

                mlog(DEBUG, "Elevation: %f, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u\n",
                     elevation, col, row, xblk, yblk, _col, _row, offset);
            }
            else
            {
                float rbuf[1] = {0};
                int _cellsize = cellsize;
                int  radius_in_meters = ((radius + _cellsize - 1) / _cellsize) * _cellsize;  // Round to multiple of cellsize
                int  radius_in_pixels = (radius_in_meters == 0) ? 1 : radius_in_meters / _cellsize;
                int _col = col - radius_in_pixels;
                int _row = row - radius_in_pixels;
                int size = radius_in_pixels + 1 + radius_in_pixels;

                /* If 8 pixels around pixel of interest are not in the raster boundries return pixel value. */
                if (_col < 0 || _row < 0)
                {
                    _col = col;
                    _row = row;
                    size = 1;
                    algorithm = GRIORA_NearestNeighbour;
                }


                GDALRasterIOExtraArg args;
                INIT_RASTERIO_EXTRA_ARG(args);
                args.eResampleAlg = algorithm;
                CPLErr err = band->RasterIO(GF_Read, _col, _row, size, size, rbuf, 1, 1, GDT_Float32, 0, 0, &args);

                elevation = rbuf[0];
                mlog(DEBUG, "Resampled elevation:  %f, radiusMeters: %d, radiusPixels: %d, size: %d\n", rbuf[0], radius, radius_in_pixels, size);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error reading ArcticDEMRaster: %s", e.what());
    }

    return elevation;
}


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
ArcticDEMRaster::ArcticDEMRaster(lua_State *L, const char* dem_type, const char* dem_sampling, const int sampling_radius):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    char uuid_str[UUID_STR_LEN] = {0};
    bool objCreated = false;

    CHECKPTR(dem_type);
    CHECKPTR(dem_sampling);

    if (!strcasecmp(dem_type, "mosaic"))
    {
        indexfname = "/data/ArcticDEM/mosaic/ArcticDEM_Tile_Index_Rel7/ArcticDEM_Tile_Index_Rel7.shp";
        ismosaic = true;
    }
    else if(!strcasecmp(dem_type, "strip"))
    {
        indexfname = "/data/ArcticDEM/strip/ArcticDEM_Strip_Index_Rel7/ArcticDEM_Strip_Index_Rel7.shp";
        ismosaic = false;
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid dem_type: %s:", dem_type);

    if     (!strcasecmp(dem_sampling, "NearestNeighbour")) algorithm = GRIORA_NearestNeighbour;
    else if(!strcasecmp(dem_sampling, "Bilinear"))         algorithm = GRIORA_Bilinear;
    else if(!strcasecmp(dem_sampling, "Cubic"))            algorithm = GRIORA_Cubic;
    else if(!strcasecmp(dem_sampling, "CubicSpline"))      algorithm = GRIORA_CubicSpline;
    else if(!strcasecmp(dem_sampling, "Lanczos"))          algorithm = GRIORA_Lanczos;
    else if(!strcasecmp(dem_sampling, "Average"))          algorithm = GRIORA_Average;
    else if(!strcasecmp(dem_sampling, "Mode"))             algorithm = GRIORA_Mode;
    else if(!strcasecmp(dem_sampling, "Gauss"))            algorithm = GRIORA_Gauss;
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling algorithm: %s:", dem_sampling);

    if(sampling_radius>=0)
        radius = sampling_radius;
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling radius: %d:", sampling_radius);


    /* Initialize Class Data Members */
    rdset = NULL;
    idset = NULL;
    ilayer = NULL;
    xblocksize = 0;
    yblocksize = 0;
    bbox = {0.0, 0.0, 0.0, 0.0};
    rows = 0;
    cols = 0;

    cellsize = 0.0;
    latlon2xy = NULL;
    source.Clear();
    target.Clear();
    rasterfname.clear();

    try
    {
        idset = (GDALDataset *)GDALOpenEx(indexfname.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
        CHECKPTR(idset);
        ilayer = idset->GetLayer(0);
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
        objCreated = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ArcticDEMRaster: %s", e.what());
    }

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
int ArcticDEMRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        ArcticDEMRaster *lua_obj = (ArcticDEMRaster *)getLuaSelf(L, 1);

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
 * luaSample - :sample(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaSample(lua_State *L)
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
        float el = lua_obj->sample(lon, lat);
        lua_pushnumber(L, el);
        num_ret++;

        if( el != ARCTIC_DEM_INVALID_ELELVATION )
            status = true;

    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting elevation: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaSamples - :samples(lon, lat) --> in|out
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaSamples(lua_State *L)
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

        List<ArcticDEMRaster::elevation_t> elist;

        /* Get Elevations */
        lua_obj->samples(lon, lat, elist);

        if(elist.length() > 0)
        {
            /* Create Table */
            lua_createtable(L, elist.length(), 0);

            for (int i = 0; i < elist.length(); i++)
            {
                elevation_t el = elist.get(i);

                lua_createtable(L, 0, 2);
                LuaEngine::setAttrStr(L, "file",  el.filename.c_str());
                LuaEngine::setAttrNum(L, "value", el.value);
                lua_rawseti(L, -2, i+1);
            }

            num_ret++;
            status = true;
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting elevation: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}
