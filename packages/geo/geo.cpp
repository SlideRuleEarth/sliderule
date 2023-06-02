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
 *INCLUDES
 ******************************************************************************/

#include "core.h"
#include "geo.h"
#include <gdal.h>
#include <cpl_conv.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEO_LIBNAME  "geo"

/******************************************************************************
 * GEO FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Optimal configuration for cloud based COGs based on:
 * https://developmentseed.org/titiler/advanced/performance_tuning/
 *----------------------------------------------------------------------------*/
static void configGDAL(void)
{
    /*
     * When reading datasets with necessary external sidecar files, it's imperative to set FALSE.
     * For example, the landsat-pds bucket on AWS S3 contains GeoTIFF images where overviews are in external .ovr files.
     * If set to EMPTY_DIR, GDAL won't find the .ovr files.
     * However, in all other cases, it's much better to set EMPTY_DIR because this prevents GDAL from making a LIST request.
     */
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");

    /*
     * Default GDAL block cache. The value can be either in Mb, bytes or percent of the physical RAM
     * Recommended 200Mb
     */
    CPLSetConfigOption("GDAL_CACHEMAX", "200");

    /*
     * A global least-recently-used cache shared among all downloaded content and may be reused after a file handle has been closed and reopen
     * 200 Mb VSI Cache.
     */
    CPLSetConfigOption("CPL_VSIL_CURL_CACHE_SIZE", "20000000");

    /*
     * A global least-recently-used cache shared among all downloaded content and may be reused after a file handle has been closed and reopen
     * Strongly recommended for s3
     */
    CPLSetConfigOption("VSI_CACHE", "TRUE");

    /*
     * The size of the above VSI cache in bytes per-file handle.
     * If you open a VRT with 10 files and your VSI_CACHE_SIZE is 10 bytes, the total cache memory usage would be 100 bytes.
     * The cache is RAM based and the content of the cache is discarded when the file handle is closed.
     * Recommended: 5000000 (5Mb per file handle)
     */
    CPLSetConfigOption("VSI_CACHE_SIZE", "5000000");

    /*
     * GDAL Block Cache type: ARRAY or HASHSET. See:
     * https://gdal.org/development/rfc/rfc26_blockcache.html
     */
    CPLSetConfigOption("GDAL_BAND_BLOCK_CACHE", "HASHSET");

    /*
     * Tells GDAL to merge consecutive range GET requests.
     */
    CPLSetConfigOption("GDAL_HTTP_MERGE_CONSECUTIVE_RANGES", "YES");

    /*
     * When set to YES, this attempts to download multiple range requests in parallel, reusing the same TCP connection.
     * Note this is only possible when the server supports HTTP2, which many servers don't yet support.
     * There's no downside to setting YES here.
     */
    CPLSetConfigOption("GDAL_HTTP_MULTIPLEX", "YES");

    /*
     * Both Multiplex and HTTP_VERSION will only have impact if the files are stored in an environment which support HTTP 2 (e.g cloudfront).
     */
    CPLSetConfigOption("GDAL_HTTP_VERSION", "2");

    /*
     * Defaults to 100. Used by gcore/gdalproxypool.cpp
     * Number of datasets that can be opened simultaneously by the GDALProxyPool mechanism (used by VRT for example).
     * Can be increased to get better random I/O performance with VRT mosaics made of numerous underlying raster files.
     * Be careful : on Linux systems, the number of file handles that can be opened by a process is generally limited to 1024.
    */
    CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "300");


    /*
     * Enable PROJ library network capabilities for accessing GeoTIFF grids
     * https://proj.org/en/9.2/usage/network.html
     */
    OSRSetPROJEnableNetwork(1);
    if(!OSRGetPROJEnableNetwork())
    {
        mlog(CRITICAL, "PROJ library network capabilities are DISABLED");
    }
}

/*----------------------------------------------------------------------------
 * geo_open
 *----------------------------------------------------------------------------*/
int geo_open (lua_State* L)
{
    static const struct luaL_Reg geo_functions[] = {
        {"geojson",     GeoJsonRaster::luaCreate},
        {"raster",      RasterObject::luaCreate},
        {"sampler",     RasterSampler::luaCreate},
        {"parms",       GeoParms::luaCreate},
        {NULL,          NULL}
    };

    /* Set Package Library */
    luaL_newlib(L, geo_functions);

    /* Set Globals */
    LuaEngine::setAttrStr   (L, "PARMS",                            GeoParms::SELF);
    LuaEngine::setAttrStr   (L, GeoParms::NEARESTNEIGHBOUR_ALGO,    GeoParms::NEARESTNEIGHBOUR_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::BILINEAR_ALGO,            GeoParms::BILINEAR_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::CUBIC_ALGO,               GeoParms::CUBIC_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::CUBICSPLINE_ALGO,         GeoParms::CUBICSPLINE_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::LANCZOS_ALGO,             GeoParms::LANCZOS_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::AVERAGE_ALGO,             GeoParms::AVERAGE_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::MODE_ALGO,                GeoParms::MODE_ALGO);
    LuaEngine::setAttrStr   (L, GeoParms::GAUSS_ALGO,               GeoParms::GAUSS_ALGO);

    return 1;
}


/*----------------------------------------------------------------------------
 * Error handler called by GDAL lib on errors
 *----------------------------------------------------------------------------*/
void GdalErrHandler(CPLErr eErrClass, int err_no, const char *msg)
{
    (void)eErrClass;  /* Silence compiler warning */
    mlog(CRITICAL, "GDAL ERROR %d: %s", err_no, msg);
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/
extern "C" {
void initgeo (void)
{
    /* Register all gdal drivers */
    GDALAllRegister();

    /* Custom GDAL configuration for cloud based COGs */
    configGDAL();

    /* Initialize Modules */
    GeoRaster::init();
    VrtRaster::init();
    VctRaster::init();
    RasterSampler::init();

    /* Register GDAL custom error handler */
    void (*fptrGdalErrorHandler)(CPLErr, int, const char *) = GdalErrHandler;
    CPLSetErrorHandler(fptrGdalErrorHandler);

    /* Extend Lua */
    LuaEngine::extend(LUA_GEO_LIBNAME, geo_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEO_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_GEO_LIBNAME, LIBID);
}

void deinitgeo (void)
{
    VctRaster::deinit();
    VrtRaster::deinit();
    GeoRaster::deinit();
    GDALDestroy();
}
}
