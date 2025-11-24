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

#include <cmath>
#include <vector>
#include <tiffio.h>
#include <gdal.h>
#include <ogr_spatialref.h>
#include <geos_c.h>


#include "GeoLib.h"
#include "LuaObject.h"
#include "GdalRaster.h"
#include "GeoJsonRaster.h"

/******************************************************************************
 * LOCAL TYPES
 ******************************************************************************/

typedef struct {
    OGRSpatialReferenceH srs_in;
    OGRSpatialReferenceH srs_out;
    OGRCoordinateTransformationH transform;
} ogr_trans_t;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoLib::DEFAULT_CRS = "EPSG:7912";  // as opposed to "EPSG:4326"
static constexpr double CLOSE_RING_EPSILON       = 1e-9;
static constexpr double REMOVE_DUPLICATE_EPSILON = 1e-12;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

static bool luaTableToCoords(lua_State* L, int index, std::vector<MathLib::coord_t>& coords)
{
    if(!lua_istable(L, index))
    {
        mlog(ERROR, "Polygon parameter is not a table");
        return false;
    }

    const int num_points = lua_rawlen(L, index);
    for(int i = 1; i <= num_points; i++)
    {
        lua_rawgeti(L, index, i);
        if(!lua_istable(L, -1))
        {
            mlog(ERROR, "Polygon vertex %d is not a table", i);
            lua_pop(L, 1);
            return false;
        }

        MathLib::coord_t coord {};

        lua_getfield(L, -1, "lon");
        coord.lon = LuaObject::getLuaFloat(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "lat");
        coord.lat = LuaObject::getLuaFloat(L, -1);
        lua_pop(L, 1);

        coords.push_back(coord);
        lua_pop(L, 1);
    }

    return true;
}

static GEOSGeometry* coordsToGeosPolygon(GEOSContextHandle_t context, const std::vector<MathLib::coord_t>& coords)
{
    if(coords.size() < 3)
    {
        mlog(ERROR, "Polygon requires at least three vertices");
        return NULL;
    }

    /* Ensure ring is closed */
    std::vector<MathLib::coord_t> ring = coords;
    const MathLib::coord_t& first = ring.front();
    const MathLib::coord_t& last = ring.back();
    if(fabs(first.lat - last.lat) > CLOSE_RING_EPSILON || fabs(first.lon - last.lon) > CLOSE_RING_EPSILON)
    {
        ring.push_back(first);
    }

    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(context, ring.size(), 2);
    if(seq == NULL)
    {
        mlog(ERROR, "Failed to create GEOS coordinate sequence");
        return NULL;
    }

    for(size_t i = 0; i < ring.size(); i++)
    {
        const MathLib::coord_t& c = ring[i];
        if(!GEOSCoordSeq_setX_r(context, seq, i, c.lon) || !GEOSCoordSeq_setY_r(context, seq, i, c.lat))
        {
            GEOSCoordSeq_destroy_r(context, seq);
            mlog(ERROR, "Failed to populate GEOS coordinate sequence");
            return NULL;
        }
    }

    GEOSGeometry* linearRing = GEOSGeom_createLinearRing_r(context, seq);
    if(linearRing == NULL)
    {
        GEOSCoordSeq_destroy_r(context, seq);
        mlog(ERROR, "Failed to create GEOS linear ring");
        return NULL;
    }

    GEOSGeometry* polygon = GEOSGeom_createPolygon_r(context, linearRing, NULL, 0);
    if(polygon == NULL)
    {
        GEOSGeom_destroy_r(context, linearRing);
        mlog(ERROR, "Failed to create GEOS polygon");
    }

    return polygon;
}

static bool pushPolygonToLua(lua_State* L, GEOSContextHandle_t context, const GEOSGeometry* polygon)
{
    const GEOSGeometry* ring = GEOSGetExteriorRing_r(context, polygon);
    if(ring == NULL)
    {
        return false;
    }

    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(context, ring);
    if(seq == NULL)
    {
        return false;
    }

    uint32_t size = 0;
    if(!GEOSCoordSeq_getSize_r(context, seq, &size) || size < 3)
    {
        return false;
    }

    /* Skip duplicated closing point, if present */
    uint32_t limit = size;
    double firstX = 0.0;
    double firstY = 0.0;
    double lastX = 0.0;
    double lastY = 0.0;

    if(GEOSCoordSeq_getX_r(context, seq, 0, &firstX) && GEOSCoordSeq_getY_r(context, seq, 0, &firstY) &&
       GEOSCoordSeq_getX_r(context, seq, size - 1, &lastX) && GEOSCoordSeq_getY_r(context, seq, size - 1, &lastY) &&
       fabs(firstX - lastX) < REMOVE_DUPLICATE_EPSILON && fabs(firstY - lastY) < REMOVE_DUPLICATE_EPSILON)
    {
        limit = size - 1;
    }

    lua_newtable(L);
    for(uint32_t i = 0; i < limit; i++)
    {
        double x = 0.0;
        double y = 0.0;
        if(!GEOSCoordSeq_getX_r(context, seq, i, &x) || !GEOSCoordSeq_getY_r(context, seq, i, &y))
        {
            lua_pop(L, 1); // remove partially filled table
            lua_pushnil(L);
            return false;
        }

        lua_newtable(L);
        lua_pushstring(L, "lat");
        lua_pushnumber(L, y);
        lua_settable(L, -3);

        lua_pushstring(L, "lon");
        lua_pushnumber(L, x);
        lua_settable(L, -3);

        lua_rawseti(L, -2, i + 1);
    }

    return true;
}

/******************************************************************************
 * UTMTransform Subclass
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoLib::UTMTransform::UTMTransform(double initial_latitude, double initial_longitude, const char* input_crs)
{
    zone = static_cast<int>(ceil((initial_longitude + 180.0) / 6)); // set public member
    is_north = (initial_latitude >= 0.0); // set public member
    in_error = false; // set public member
    ogr_trans_t* ogr_trans = new ogr_trans_t;
    ogr_trans->srs_in = OSRNewSpatialReference(NULL);
    ogr_trans->srs_out = OSRNewSpatialReference(NULL);
    OSRSetFromUserInput(ogr_trans->srs_in, input_crs);
    OSRSetProjCS(ogr_trans->srs_out, "UTM");
    OSRSetUTM(ogr_trans->srs_out, zone, is_north);
    ogr_trans->transform = OCTNewCoordinateTransformation(ogr_trans->srs_in, ogr_trans->srs_out);
    transform = reinterpret_cast<utm_transform_t>(ogr_trans); // set private member
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoLib::UTMTransform::UTMTransform(int _zone, bool _is_north, const char* output_crs):
    zone(_zone),
    is_north(_is_north),
    in_error(false)
{
    ogr_trans_t* ogr_trans = new ogr_trans_t;
    ogr_trans->srs_in = OSRNewSpatialReference(NULL);
    ogr_trans->srs_out = OSRNewSpatialReference(NULL);
    OSRSetProjCS(ogr_trans->srs_in, "UTM");
    OSRSetUTM(ogr_trans->srs_in, zone, is_north);
    OSRSetFromUserInput(ogr_trans->srs_out, output_crs);
    ogr_trans->transform = OCTNewCoordinateTransformation(ogr_trans->srs_in, ogr_trans->srs_out);
    transform = reinterpret_cast<utm_transform_t>(ogr_trans); // set private member
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoLib::UTMTransform::~UTMTransform(void)
{
    ogr_trans_t* ogr_trans = reinterpret_cast<ogr_trans_t*>(transform);
    OSRDestroySpatialReference(ogr_trans->srs_in);
    OSRDestroySpatialReference(ogr_trans->srs_out);
    OCTDestroyCoordinateTransformation(ogr_trans->transform);
    delete ogr_trans;
}

/*----------------------------------------------------------------------------
 * Destructor
 *  TODO: why is the x and y flipped?
 *        it only gives the correct answer when in this order
 *----------------------------------------------------------------------------*/
GeoLib::point_t GeoLib::UTMTransform::calculateCoordinates(double x, double y)
{
    point_t coord;

    /* Force In Error To Be Set On Success */
    in_error = true;

    /* Pull Out Transformation */
    ogr_trans_t* ogr_trans = reinterpret_cast<ogr_trans_t*>(transform);

    /* Perform Transformation */
    if(OCTTransform(ogr_trans->transform, 1, &x, &y, NULL) == TRUE)
    {
        coord.x = x;
        coord.y = y;
        in_error = false;
    }

    /* Return Coordinates */
    return coord;
}

/******************************************************************************
 * TIFFImage Subclass
 ******************************************************************************/

const char* GeoLib::TIFFImage::OBJECT_TYPE = "TIFFImage";
const char* GeoLib::TIFFImage::LUA_META_NAME = "TIFFImage";
const struct luaL_Reg GeoLib::TIFFImage::LUA_META_TABLE[] = {
    {"dimensions",  GeoLib::TIFFImage::luaDimensions},
    {"pixel",       GeoLib::TIFFImage::luaPixel},
    {"tobmp",       GeoLib::TIFFImage::luaConvertToBMP},
    {NULL,          NULL}
};

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaCreate (lua_State* L)
{
    try
    {
        const char* filename = getLuaString(L, 1);
        const long driver = getLuaInteger(L, 2, true, LIBTIFF_DRIVER);
        return createLuaObject(L, new TIFFImage(L, filename, driver));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoLib::TIFFImage::TIFFImage(lua_State* L, const char* filename, long driver):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
    if(driver == LIBTIFF_DRIVER)
    {
        TIFF* tif = TIFFOpen(filename, "r");
        if(!tif) throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to open tiff file: %s", filename);

        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
        typesize = 4; // tiff driver only supports uint32_t (RGB)

        mlog(INFO, "Reading image %s which is %u x %u pixels", filename, width, height);

        size = width * height * typesize;
        raster = new  (std::align_val_t(RASTER_DATA_ALIGNMENT)) uint8_t[size];
        type = RecordObject::UINT32;

        uint32_t* _raster = reinterpret_cast<uint32_t*>(raster);
        if(!TIFFReadRGBAImage(tif, width, height, _raster, 0))
        {
            operator delete[](raster, std::align_val_t(RASTER_DATA_ALIGNMENT));
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to read tiff file: %s", filename);
        }

        TIFFClose(tif);
    }
    else if(driver == GDAL_DRIVER)
    {
        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(filename, GA_ReadOnly));
        if(!dataset) throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to open tiff file: %s", filename);
        width = dataset->GetRasterXSize();
        height = dataset->GetRasterYSize();
        GDALRasterBand* band = dataset->GetRasterBand(1);
        const GDALDataType dtype = band->GetRasterDataType();
        typesize  = GDALGetDataTypeSizeBytes(dtype);

        mlog(INFO, "Reading image %s which is %u x %u pixels", filename, width, height);

        size = width * height * typesize;
        raster = new  (std::align_val_t(RASTER_DATA_ALIGNMENT)) uint8_t[size];
        void* _data = const_cast<void*>(reinterpret_cast<const void*>(raster));
        const OGRErr err = band->RasterIO(GF_Read, 0, 0, width, height, _data, width, height, dtype, 0, 0);
        GDALClose((GDALDatasetH)dataset);

        switch(dtype)
        {
            case GDT_Int8:      type = RecordObject::INT8;   break;
            case GDT_Int16:     type = RecordObject::INT16;  break;
            case GDT_Int32:     type = RecordObject::INT32;  break;
            case GDT_Int64:     type = RecordObject::INT64;  break;
            case GDT_Byte:      type = RecordObject::UINT8;  break;
            case GDT_UInt16:    type = RecordObject::UINT16; break;
            case GDT_UInt32:    type = RecordObject::UINT32; break;
            case GDT_UInt64:    type = RecordObject::UINT64; break;
            case GDT_Float32:   type = RecordObject::FLOAT;  break;
            case GDT_Float64:   type = RecordObject::DOUBLE; break;
            default:            type = RecordObject::INVALID_FIELD; break;
        }

        if(err != CE_None)
        {
            operator delete[](raster, std::align_val_t(RASTER_DATA_ALIGNMENT));
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to read tiff file: %s", filename);
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid driver selected: %ld", driver);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoLib::TIFFImage::~TIFFImage(void)
{
    operator delete[](raster, std::align_val_t(RASTER_DATA_ALIGNMENT));
}

/*----------------------------------------------------------------------------
 * getPixel
 *----------------------------------------------------------------------------*/
GeoLib::TIFFImage::val_t GeoLib::TIFFImage::getPixel(uint32_t x, uint32_t y)
{
    val_t val = {.u64 = INVALID_PIXEL};
    const uint32_t offset = ((y * width) + x) * typesize;

    if(offset < size)
    {
        switch(typesize)
        {
            case 1:
            {
                val.u8 = raster[offset];
                break;
            }
            case 2:
            {
                const uint16_t* valptr = reinterpret_cast<uint16_t*>(&raster[offset]);
                val.u16 = *valptr;
                break;
            }
            case 4:
            {
                const uint32_t* valptr = reinterpret_cast<uint32_t*>(&raster[offset]);
                val.u32 = *valptr;
                break;
            }
            case 8:
            {
                const uint64_t* valptr = reinterpret_cast<uint64_t*>(&raster[offset]);
                val.u64 = *valptr;
                break;
            }
        }
    }

    return val;
}

/*----------------------------------------------------------------------------
 * getWidth
 *----------------------------------------------------------------------------*/
uint32_t GeoLib::TIFFImage::getWidth(void) const
{
    return width;
}

/*----------------------------------------------------------------------------
 * getLength
 *----------------------------------------------------------------------------*/
uint32_t GeoLib::TIFFImage::getHeight() const
{
    return height;
}

/*----------------------------------------------------------------------------
 * luaDimensions
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaDimensions (lua_State* L)
{
    try
    {
        GeoLib::TIFFImage* lua_obj = dynamic_cast<GeoLib::TIFFImage*>(getLuaSelf(L, 1));
        lua_pushnumber(L, lua_obj->width);
        lua_pushnumber(L, lua_obj->height);
        lua_pushnumber(L, lua_obj->typesize);
        lua_pushnumber(L, lua_obj->type);
        return returnLuaStatus(L, true, 5);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaPixel
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaPixel (lua_State* L)
{
    bool status = false;
    try
    {
        GeoLib::TIFFImage* lua_obj = dynamic_cast<GeoLib::TIFFImage*>(getLuaSelf(L, 1));
        const int type = getLuaInteger(L, 2, true, RecordObject::UINT32);
        const int x = getLuaInteger(L, 2);
        const int y = getLuaInteger(L, 3);
        if(x < 0 || x >= (int)lua_obj->width || y < 0 || y >= (int)lua_obj->height)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "out of bounds (%d, %d) ^ (%d, %d)", x, y, lua_obj->width, lua_obj->height);
        }
        const val_t val = lua_obj->getPixel(x, y);
        switch(type)
        {
            case RecordObject::INT8:    lua_pushnumber(L, val.i8);  break;
            case RecordObject::INT16:   lua_pushnumber(L, val.i16); break;
            case RecordObject::INT32:   lua_pushnumber(L, val.i32); break;
            case RecordObject::INT64:   lua_pushnumber(L, val.i64); break;
            case RecordObject::UINT8:   lua_pushnumber(L, val.u8);  break;
            case RecordObject::UINT16:  lua_pushnumber(L, val.u16); break;
            case RecordObject::UINT32:  lua_pushnumber(L, val.u32); break;
            case RecordObject::UINT64:  lua_pushnumber(L, val.u64); break;
            case RecordObject::FLOAT:   lua_pushnumber(L, val.f32); break;
            case RecordObject::DOUBLE:  lua_pushnumber(L, val.f64); break;
            default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid type: %d", type);
        }

        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "failed to get pixel: %s", e.what());
        lua_pushnil(L);
    }
    return returnLuaStatus(L, status, 2);
}

/*----------------------------------------------------------------------------
 * luaConvertToBMP
 *----------------------------------------------------------------------------*/
int GeoLib::TIFFImage::luaConvertToBMP (lua_State* L)
{
    bool status = false;
    try
    {
        const GeoLib::TIFFImage* lua_obj = dynamic_cast<GeoLib::TIFFImage*>(getLuaSelf(L, 1));
        const char* bmp_filename = getLuaString(L, 2);
        if(lua_obj->type == RecordObject::DOUBLE)
        {
            /* special case conversion of 64-bit floats to scaled 32-bit unsigned ints */
            const uint32_t num_elements = lua_obj->width * lua_obj->height;
            const double* _raster = reinterpret_cast<double*>(lua_obj->raster);
            uint32_t* data = new uint32_t [num_elements];
            double minval =  std::numeric_limits<double>::max();
            double maxval =  std::numeric_limits<double>::min();
            for(uint32_t i = 0; i < num_elements; i++)
            {
                if(_raster[i] < minval) minval = _raster[i];
                if(_raster[i] > maxval) maxval = _raster[i];
            }
            const double spread = maxval - minval;
            const double resolution = spread / 0xFFFFFFFF;
            for(uint32_t i = 0; i < num_elements; i++)
            {
                data[i] = (_raster[i] - minval) / resolution;
            }
            status = GeoLib::writeBMP(data, lua_obj->width, lua_obj->height, bmp_filename);
            delete [] data;
        }
        if(lua_obj->typesize == 1)
        {
            /* special case conversion of 8-bit integers to 32-bit unsigned ints */
            const uint32_t num_elements = lua_obj->width * lua_obj->height;
            uint32_t* data = new uint32_t [num_elements];
            for(uint32_t i = 0; i < num_elements; i++)
            {
                data[i] = lua_obj->raster[i];
            }
            status = GeoLib::writeBMP(data, lua_obj->width, lua_obj->height, bmp_filename);
            delete [] data;
        }
        else if(lua_obj->typesize != 4)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "only 32-bit tiff files can be converted to BMP: %d", lua_obj->typesize);
        }
        else
        {
            /* just use the value as-is if it is 32 bits */
            const uint32_t* _raster = reinterpret_cast<uint32_t*>(lua_obj->raster);
            status = GeoLib::writeBMP(_raster, lua_obj->width, lua_obj->height, bmp_filename);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "failed to convert to BMP: %s", e.what());
    }
    return returnLuaStatus(L, status);
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/

void customTIFFErrorHandler(const char* , const char* , va_list ) // NOLINT [readability-named-parameter]
{
    // Ignore or suppress error messages here
}
void GeoLib::init (void)
{
    TIFFSetErrorHandler(NULL); // disables error messages
    TIFFSetWarningHandler(NULL); // disables warning messages
}

/*----------------------------------------------------------------------------
 * luaCalcUTM
 *----------------------------------------------------------------------------*/
int GeoLib::luaCalcUTM (lua_State* L)
{
    double latitude;
    double longitude;

    try
    {
        latitude = LuaObject::getLuaFloat(L, 1);
        longitude = LuaObject::getLuaFloat(L, 2);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Failed to get parameters for UTM calculation: %s", e.what());
        return 0;
    }

    UTMTransform transform(latitude, longitude);
    const point_t coord = transform.calculateCoordinates(latitude, longitude);
    if(!transform.in_error)
    {
        lua_pushinteger(L, transform.zone);
        lua_pushnumber(L, coord.x);
        lua_pushnumber(L, coord.y);
        return 3;
    }
    else
    {
        mlog(CRITICAL, "Failed to perform UTM transformation on %lf, %lf", latitude, longitude);
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * luaPolySimplify
 *----------------------------------------------------------------------------*/
int GeoLib::luaPolySimplify(lua_State* L)
{
    GEOSContextHandle_t context = initGEOS_r(NULL, NULL);
    if(context == NULL)
    {
        mlog(CRITICAL, "Failed to initialize GEOS context");
        lua_pushnil(L);
        return 1;
    }

    GEOSGeometry* polygon = NULL;
    GEOSGeometry* buffered = NULL;
    GEOSGeometry* simplified = NULL;
    GEOSGeometry* hull = NULL;
    bool status = false;

    try
    {
        const double bufferDistance    = LuaObject::getLuaFloat(L, 2, true, 0.0);
        const double simplifyTolerance = LuaObject::getLuaFloat(L, 3, true, 0.0);
        if(bufferDistance < 0.0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Buffer distance must be >= 0.0");
        }
        if(simplifyTolerance < 0.0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Simplify tolerance must be >= 0.0");
        }

        std::vector<MathLib::coord_t> coords;
        if(!luaTableToCoords(L, 1, coords))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid polygon argument");
        }

        polygon = coordsToGeosPolygon(context, coords);
        if(polygon == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create GEOS polygon");
        }

        /* Buffer first to clean up small gaps/invalidities */
        buffered = GEOSBuffer_r(context, polygon, bufferDistance, 8);
        if(buffered == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "GEOS buffer failed");
        }

        /* Simplify */
        simplified = (simplifyTolerance > 0.0) ?
            GEOSTopologyPreserveSimplify_r(context, buffered, simplifyTolerance) :
            GEOSGeom_clone_r(context, buffered);

        if(simplified == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "GEOS simplification failed");
        }

        /* Compute convex hull of simplified geometry */
        hull = GEOSConvexHull_r(context, simplified);
        if (hull == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "GEOS convex hull failed");
        }

        /* Reject empty geometries */
        if (GEOSisEmpty_r(context, hull))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Convex hull result is empty");
        }

        /* Hull must be a single polygon
         * In rare cases hull can degenerate to line or point */
        if (GEOSGeomTypeId_r(context, hull) != GEOS_POLYGON)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Convex hull did not produce a polygon");
        }

        /*
         * Validate the hull before returning it to Lua. This is quick relative to
         * buffer/simplify and guards against numeric edge cases yielding an invalid polygon.
         */
        if(GEOSisValid_r(context, hull) != 1)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Convex hull failed validity check");
        }

        if(!pushPolygonToLua(L, context, hull))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to convert simplified polygon to Lua");
        }

        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error simplifying polygon: %s", e.what());
    }

    GEOSGeom_destroy_r(context, hull);
    GEOSGeom_destroy_r(context, simplified);
    GEOSGeom_destroy_r(context, buffered);
    GEOSGeom_destroy_r(context, polygon);
    finishGEOS_r(context);

    if(status) return 1;

    lua_pushnil(L);
    return 1;
}

/*----------------------------------------------------------------------------
 * writeBMP
 *----------------------------------------------------------------------------*/
bool GeoLib::writeBMP (const uint32_t* data, int width, int height, const char* filename, uint32_t min_val, uint32_t max_val)
{
    /* open file */
    FILE* bmp_file = fopen(filename, "w");
    if(!bmp_file)
    {
        char errbuf[256];
        mlog(CRITICAL, "Failed to open file %s: %s", filename, strerror_r(errno, errbuf, sizeof(errbuf)));
        return false;
    }

    /* populate attributes */
    const uint32_t palette_size = 1024; // 2^(pixelsize) * 4
	const uint32_t image_size	= height * modup(width, 4);
	const uint32_t data_offset  = 0x36 + palette_size; // header plus palette
	const uint32_t file_size	= data_offset + image_size;

    /* populate header */
    bmp_hdr_t bmp_hdr = {
        .file_size          = file_size,
        .reserved1          = 0,
        .reserved2          = 0,
        .data_offset        = data_offset,
        .hdr_size           = 40,
        .image_width        = width,
        .image_height       = height,
        .color_planes       = 1,
        .color_depth        = 8,
        .compression        = 0,
        .image_size         = image_size,
        .hor_res            = 1,
        .ver_res            = 1,
        .palette_colors     = 0,
        .important_colors   = 0,
    };

    /* write header */
    fwrite("B", 1, 1, bmp_file); // magic numbers written first to avoid alignment issues
    fwrite("M", 1, 1, bmp_file); // "
    fwrite(&bmp_hdr, sizeof(bmp_hdr_t), 1, bmp_file);

    /* write color palette */
    for(int i = 0; i < 256; i++)
    {
        uint8_t b = static_cast<uint8_t>(i);
        union {
            uint8_t b[4];
            uint32_t v;
        } value = {
            .b = {b, b, b, b}
        };
        (void)value.b; // silence static analysis "unused variable"
        fwrite(&value.v, 4, 1, bmp_file);
    }

    /* write image data */
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            const int offset = (y * width) + x;
            const uint32_t value = data[offset] - min_val;
            const double normalized_pixel = (static_cast<double>(value) / static_cast<double>(max_val)) * 256.0;
            uint8_t scaled_pixel = static_cast<uint8_t>(normalized_pixel);
            fwrite(&scaled_pixel, 1, 1, bmp_file);
        }
        for(int padding = 0; padding < modup(width, 4); padding++)
        {
            uint8_t zero = 0;
            fwrite(&zero, 1, 1, bmp_file);
        }
    }

    /* close file */
    fclose(bmp_file);

    /* return success */
    return true;
}

/*----------------------------------------------------------------------------
 * burnGeoJson
 *----------------------------------------------------------------------------*/
bool GeoLib::burnGeoJson(RegionMask& image)
{
    // initialize raster (to be cleaned up below)
    GeoJsonRaster* raster = NULL;

    // reset image data
    delete [] image.data;
    image.data = NULL;

    try
    {
        // create geojson raster
        raster = GeoJsonRaster::create(image.geojson.value, image.cellSize.value);
        if(!raster) throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to create raster");

        // populate image attributes
        const GeoJsonRaster::bbox_t bbox = raster->getRasterBbox();
        image.cols = raster->getRasterCols();
        image.rows = raster->getRasterRows();
        image.lonMin = bbox.lon_min;
        image.lonMax = bbox.lon_max;
        image.latMin = bbox.lat_min;
        image.latMax = bbox.lat_max;

        // populate image data
        const long data_size = static_cast<long>(image.cols.value) * static_cast<long>(image.rows.value);

        if(data_size > 0)
        {
            image.data = new uint8_t [data_size];
            memcpy(image.data, raster->getRasterData(), data_size);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error rasterizing region mask: %s", e.what());
    }

    // clean up raster
    delete raster;

    // return status
    return image.data == NULL;
}