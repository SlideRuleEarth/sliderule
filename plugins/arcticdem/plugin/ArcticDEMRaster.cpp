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

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal.h"
#include "ogr_spatialref.h"

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
    {"samples",     luaSamples},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::init( void )
{
    /* Register all gdal drivers */
    GDALAllRegister();
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::deinit( void )
{
    GDALDestroy();
}

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int ArcticDEMRaster::luaCreate( lua_State* L )
{
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch( const RunTimeException& e )
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}


/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
ArcticDEMRaster* ArcticDEMRaster::create( lua_State* L, int index )
{
    const int radius = getLuaInteger(L, -1);
    lua_pop(L, 1);
    const char* dem_sampling = getLuaString(L, -1);
    lua_pop(L, 1);
    const char* dem_type = getLuaString(L, -1);
    lua_pop(L, 1);
    return new ArcticDEMRaster(L, dem_type, dem_sampling, radius);
}


static void getVrtName( double lon, double lat, std::string& vrtFile )
{
    int ilat = floor(lat);
    int ilon = floor(lon);

    vrtFile = "/data/ArcticDem/strips/n" +
               std::to_string(ilat)      +
               ((ilon < 0) ? "w" : "e")  +
               std::to_string(abs(ilon)) +
               ".vrt";
}

/*----------------------------------------------------------------------------
 * samples
 *----------------------------------------------------------------------------*/
void ArcticDEMRaster::samples( double lon, double lat )
{
    OGRPoint p = {lon, lat};

    if (p.transform(transf) == OGRERR_NONE)
    {
        double _lon = p.getX();
        double _lat = p.getY();

        /* Is point in VRT raster? */
        if (vrtDset &&
            (_lon >= vrtBbox.lon_min) &&
            (_lon <= vrtBbox.lon_max) &&
            (_lat >= vrtBbox.lat_min) &&
            (_lat <= vrtBbox.lat_max))
        {
            bool foundSample = false;

            /* Point is in VRT dataset */
            if (rastersList.length() > 0)
                foundSample = readRasters(&p);

            if (!foundSample)
            {
                if (findRasters(&p))
                    readRasters(&p);
            }
        }
        else
        {
            if (isStrip)
            {
                /* Find VRT file for scene with this point */
                std::string newVrtFile;
                getVrtName(lon, lat, newVrtFile);

                if (!openVrtDset(newVrtFile.c_str()))
                    throw RunTimeException(CRITICAL, RTE_ERROR, "ArcticDEMRaster get samples failed");

                if (findRasters(&p))
                    readRasters(&p);
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArcticDEMRaster::~ArcticDEMRaster(void)
{
    if (vrtDset) GDALClose((GDALDatasetH)vrtDset);
    for (int i = 0; i < rastersList.length(); i++)
    {
        raster_info_t rinfo = rastersList.get(i);
        if (rinfo.dset) GDALClose((GDALDatasetH)rinfo.dset);
    }
    if (transf) OGRCoordinateTransformation::DestroyCT(transf);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * findRasters
 *----------------------------------------------------------------------------*/
bool ArcticDEMRaster::findRasters(OGRPoint *p)
{
    bool foundRaster = false;

    try
    {
        /* Close existing rasters */
        const int length = rastersList.length();
        if (length > 0)
        {
            for (int i = 0; i < length; i++)
            {
                raster_info_t rinfo = rastersList.get(i);
                if (rinfo.dset) GDALClose((GDALDatasetH)rinfo.dset);
            }
            rastersList.clear();
        }

        const int32_t col = static_cast<int32_t>(floor(invgeot[0] + invgeot[1] * p->getX() + invgeot[2] * p->getY()));
        const int32_t row = static_cast<int32_t>(floor(invgeot[3] + invgeot[4] * p->getX() + invgeot[5] * p->getY()));

        if (col >= 0 && row >= 0 && col < vrtDset->GetRasterXSize() && row < vrtDset->GetRasterYSize())
        {
            CPLString str;
            str.Printf("Pixel_%d_%d", col, row);

            const char *mdata = vrtBand->GetMetadataItem(str, "LocationInfo");
            if (mdata)
            {
                CPLXMLNode *root = CPLParseXMLString(mdata);
                if (root && root->psChild && root->eType == CXT_Element && EQUAL(root->pszValue, "LocationInfo"))
                {
                    for (CPLXMLNode *psNode = root->psChild; psNode; psNode = psNode->psNext)
                    {
                        if (psNode->eType == CXT_Element && EQUAL(psNode->pszValue, "File") && psNode->psChild)
                        {
                            raster_info_t rinfo = {nullptr, nullptr, "", ARCTIC_DEM_INVALID_ELELVATION};

                            char *fname = CPLUnescapeString(psNode->psChild->pszValue, nullptr, CPLES_XML);
                            CHECKPTR(fname);
                            rinfo.fileName = fname;
                            CPLFree(fname);
                            mlog(DEBUG, "%s, contains VRT file point(%u, %u)", rinfo.fileName.c_str(), col, row);

                            rinfo.dset = (GDALDataset *)GDALOpenEx(rinfo.fileName.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
                            CHECKPTR(rinfo.dset);

                            /* Store information about raster */
                            rinfo.cols = rinfo.dset->GetRasterXSize();
                            rinfo.rows = rinfo.dset->GetRasterYSize();

                            /* Get raster boundry box */
                            double geot[6] = {0, 0, 0, 0, 0, 0};
                            CPLErr err = rinfo.dset->GetGeoTransform(geot);
                            CHECK_GDALERR(err);
                            rinfo.bbox.lon_min = geot[0];
                            rinfo.bbox.lon_max = geot[0] + rinfo.cols * geot[1];
                            rinfo.bbox.lat_max = geot[3];
                            rinfo.bbox.lat_min = geot[3] + rinfo.rows * geot[5];

                            rinfo.cellSize = geot[1];

                            /* Get raster block size */
                            rinfo.band = rinfo.dset->GetRasterBand(1);
                            CHECKPTR(rinfo.band);
                            rinfo.band->GetBlockSize(&rinfo.xBlockSize, &rinfo.yBlockSize);
                            mlog(DEBUG, "Raster xBlockSize: %d, yBlockSize: %d", rinfo.xBlockSize, rinfo.yBlockSize);
                            rastersList.add(rinfo);
                            foundRaster = true;
                        }
                    }
                }
                CPLDestroyXMLNode(root);
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating ArcticDEMRaster: %s", e.what());
    }

    return foundRaster;
}

/*----------------------------------------------------------------------------
 * readRasters
 *----------------------------------------------------------------------------*/
bool ArcticDEMRaster::readRasters(OGRPoint *p)
{
    bool containsPoint = false;

    try
    {
        for (int i = 0; i < rastersList.length(); i++)
        {
            raster_info_t rinfo = rastersList.get(i);

            double lon = p->getX();
            double lat = p->getY();

            /* Is point in this raster? */
            if (rinfo.dset &&
                (lon >= rinfo.bbox.lon_min) &&
                (lon <= rinfo.bbox.lon_max) &&
                (lat >= rinfo.bbox.lat_min) &&
                (lat <= rinfo.bbox.lat_max))
            {
                /* Raster row, col for point */
                const int32_t col = static_cast<int32_t>(floor((p->getX() - rinfo.bbox.lon_min) / rinfo.cellSize));
                const int32_t row = static_cast<int32_t>(floor((rinfo.bbox.lat_max - p->getY()) / rinfo.cellSize));

                /* At least one raster contains this point */
                containsPoint = true;

                /* Use fast 'lookup' method for nearest neighbour. */
                if (sampleAlg == GRIORA_NearestNeighbour)
                {
                    /* Raster offsets to block of interest */
                    uint32_t xblk = col / rinfo.xBlockSize;
                    uint32_t yblk = row / rinfo.yBlockSize;

                    GDALRasterBlock *block = rinfo.band->GetLockedBlockRef(xblk, yblk, false);
                    CHECKPTR(block);

                    float *p = (float *)block->GetDataRef();
                    CHECKPTR(p);

                    /* col, row inside of block */
                    uint32_t _col = col % rinfo.xBlockSize;
                    uint32_t _row = row % rinfo.yBlockSize;
                    uint32_t offset = _row * rinfo.xBlockSize + _col;

                    rinfo.value = p[offset];
                    block->DropLock();
                    rastersList.set(i, rinfo);
                    mlog(DEBUG, "Elevation: %f, col: %u, row: %u, xblk: %u, yblk: %u, bcol: %u, brow: %u, offset: %u\n",
                         rinfo.value, col, row, xblk, yblk, _col, _row, offset);
                }
                else
                {
                    float rbuf[1] = {0};
                    int _cellsize = rinfo.cellSize;
                    int radius_in_meters = ((radius + _cellsize - 1) / _cellsize) * _cellsize; // Round to multiple of cellSize
                    int radius_in_pixels = (radius_in_meters == 0) ? 1 : radius_in_meters / _cellsize;
                    int _col = col - radius_in_pixels;
                    int _row = row - radius_in_pixels;
                    int size = radius_in_pixels + 1 + radius_in_pixels;

                    /* If 8 pixels around pixel of interest are not in the raster boundries return pixel value. */
                    if (_col < 0 || _row < 0)
                    {
                        _col = col;
                        _row = row;
                        size = 1;
                        sampleAlg = GRIORA_NearestNeighbour;
                    }

                    GDALRasterIOExtraArg args;
                    INIT_RASTERIO_EXTRA_ARG(args);
                    args.eResampleAlg = sampleAlg;
                    CPLErr err = rinfo.band->RasterIO(GF_Read, _col, _row, size, size, rbuf, 1, 1, GDT_Float32, 0, 0, &args);
                    CHECK_GDALERR(err);
                    rinfo.value = rbuf[0];
                    rastersList.set(i, rinfo);
                    mlog(DEBUG, "Resampled elevation:  %f, radiusMeters: %d, radiusPixels: %d, size: %d\n", rbuf[0], radius, radius_in_pixels, size);
                }
            }
            else
            {
                rinfo.value = ARCTIC_DEM_INVALID_ELELVATION;
                rastersList.set(i, rinfo);
                // print2term("%lf, %lf Point not in the raster: %d, %s\n", lon, lat, i, rinfo.fileName.c_str());
            }
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error reading ArcticDEMRaster: %s", e.what());
    }

    return containsPoint;
}

bool ArcticDEMRaster::openVrtDset(const char *fileName)
{
    bool objCreated = false;

    try
    {
        /* Cleanup previous vrtDset */
        if (vrtDset)
        {
            GDALClose((GDALDatasetH)vrtDset);
            vrtDset = nullptr;
        }

        if (transf)
        {
            OGRCoordinateTransformation::DestroyCT(transf);
            transf = nullptr;
        }

        /* New vrtDset */
        vrtDset = (VRTDataset *)GDALOpenEx(fileName, GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
        CHECKPTR(vrtDset);
        vrtFileName = fileName;

        vrtBand = vrtDset->GetRasterBand(1);
        CHECKPTR(vrtBand);

        /* Get inverted geo transfer for vrt */
        double geot[6] = {0};
        CPLErr err = GDALGetGeoTransform(vrtDset, geot);
        CHECK_GDALERR(err);
        if (!GDALInvGeoTransform(geot, invgeot))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
            CHECK_GDALERR(CE_Failure);
        }

        /* Store information about vrt raster */
        vrtCols = vrtDset->GetRasterXSize();
        vrtRows = vrtDset->GetRasterYSize();

        /* Get raster boundry box */
        bzero(geot, sizeof(geot));
        err = vrtDset->GetGeoTransform(geot);
        CHECK_GDALERR(err);
        vrtBbox.lon_min = geot[0];
        vrtBbox.lon_max = geot[0] + vrtCols * geot[1];
        vrtBbox.lat_max = geot[3];
        vrtBbox.lat_min = geot[3] + vrtRows * geot[5];

        vrtCellSize = geot[1];

        OGRErr ogrerr = srcSrs.importFromEPSG(RASTER_PHOTON_CRS);
        CHECK_GDALERR(ogrerr);
        const char *projref = vrtDset->GetProjectionRef();
        if (projref)
        {
            mlog(DEBUG, "%s", projref);
            ogrerr = trgSrs.importFromProj4(projref);
        }
        else
        {
            /* In case vrt file does not have projection info, use default */
            ogrerr = trgSrs.importFromEPSG(RASTER_ARCTIC_DEM_CRS);
        }
        CHECK_GDALERR(ogrerr);

        /* Force traditional axis order to avoid lat,lon and lon,lat API madness */
        trgSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        /* Create coordinates transformation */
        transf = OGRCreateCoordinateTransformation(&srcSrs, &trgSrs);
        CHECKPTR(transf);
        objCreated = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error creating new VRT dataset: %s", e.what());
    }

    return objCreated;
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
ArcticDEMRaster::ArcticDEMRaster(lua_State *L, const char *dem_type, const char *dem_sampling, const int sampling_radius):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    char uuid_str[UUID_STR_LEN] = {0};
    std::string fname;

    CHECKPTR(dem_type);
    CHECKPTR(dem_sampling);

    if (!strcasecmp(dem_type, "mosaic"))
    {
        // fname = "/data/ArcticDem/mosaic_short.vrt";
        fname = "/data/ArcticDem/mosaic.vrt";
        isStrip = false;
    }
    else if (!strcasecmp(dem_type, "strip"))
    {
        // fname = "/data/ArcticDem/strips/n51w178.vrt";
        fname = "/data/ArcticDem/strips/n51e156.vrt"; /* First strip file in catalog */
        isStrip = true;
    }
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid dem_type: %s:", dem_type);

    if      (!strcasecmp(dem_sampling, "NearestNeighbour")) sampleAlg = GRIORA_NearestNeighbour;
    else if (!strcasecmp(dem_sampling, "Bilinear"))         sampleAlg = GRIORA_Bilinear;
    else if (!strcasecmp(dem_sampling, "Cubic"))            sampleAlg = GRIORA_Cubic;
    else if (!strcasecmp(dem_sampling, "CubicSpline"))      sampleAlg = GRIORA_CubicSpline;
    else if (!strcasecmp(dem_sampling, "Lanczos"))          sampleAlg = GRIORA_Lanczos;
    else if (!strcasecmp(dem_sampling, "Average"))          sampleAlg = GRIORA_Average;
    else if (!strcasecmp(dem_sampling, "Mode"))             sampleAlg = GRIORA_Mode;
    else if (!strcasecmp(dem_sampling, "Gauss"))            sampleAlg = GRIORA_Gauss;
    else
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling algorithm: %s:", dem_sampling);

    if (sampling_radius >= 0)
        radius = sampling_radius;
    else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid sampling radius: %d:", sampling_radius);

    /* Initialize Class Data Members */
    vrtDset = nullptr;
    vrtBand = nullptr;
    rastersList.clear();
    bzero(invgeot, sizeof(invgeot));
    vrtRows = vrtCols = vrtCellSize = 0;
    bzero(&vrtBbox, sizeof(vrtBbox));
    transf = nullptr;
    srcSrs.Clear();
    trgSrs.Clear();

    if (!openVrtDset(fname.c_str()))
        throw RunTimeException(CRITICAL, RTE_ERROR, "ArcticDEMRaster constructor failed");
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
        lua_pushinteger(L, lua_obj->vrtRows);
        lua_pushinteger(L, lua_obj->vrtCols);
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
        lua_pushnumber(L, lua_obj->vrtBbox.lon_min);
        lua_pushnumber(L, lua_obj->vrtBbox.lat_min);
        lua_pushnumber(L, lua_obj->vrtBbox.lon_max);
        lua_pushnumber(L, lua_obj->vrtBbox.lat_max);
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
        lua_pushnumber(L, lua_obj->vrtCellSize);
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

        /* Get Elevations */
        lua_obj->samples(lon, lat);

        if (lua_obj->rastersList.length() > 0)
        {
            /* Create Table */
            lua_createtable(L, lua_obj->rastersList.length(), 0);

            for (int i = 0; i < lua_obj->rastersList.length(); i++)
            {
                raster_info_t rinfo = lua_obj->rastersList.get(i);

                lua_createtable(L, 0, 2);
                LuaEngine::setAttrStr(L, "file", rinfo.fileName.c_str());
                LuaEngine::setAttrNum(L, "value", rinfo.value);
                lua_rawseti(L, -2, i + 1);
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