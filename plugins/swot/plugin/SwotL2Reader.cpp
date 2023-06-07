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

#include <math.h>
#include <float.h>
#include <stdarg.h>

#include "core.h"
#include "h5.h"
#include "swot.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SwotL2Reader::OBJECT_TYPE = "SwotL2Reader";
const char* SwotL2Reader::LuaMetaName = "SwotL2Reader";
const struct luaL_Reg SwotL2Reader::LuaMetaTable[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

const char* SwotL2Reader::varRecType = "swotl2rec";
const RecordObject::fieldDef_t SwotL2Reader::varRecDef[] = {
    {"granule",     RecordObject::STRING,   offsetof(var_rec_t, granule),       MAX_GRANULE_NAME_STR,   NULL, NATIVE_FLAGS},
    {"variable",    RecordObject::STRING,   offsetof(var_rec_t, variable),      MAX_VARIABLE_NAME_STR,  NULL, NATIVE_FLAGS},
    {"datatype",    RecordObject::UINT32,   offsetof(var_rec_t, datatype),      1,                      NULL, NATIVE_FLAGS},
    {"elements",    RecordObject::UINT32,   offsetof(var_rec_t, elements),      1,                      NULL, NATIVE_FLAGS},
    {"width",       RecordObject::UINT32,   offsetof(var_rec_t, width),         1,                      NULL, NATIVE_FLAGS},
    {"size",        RecordObject::UINT32,   offsetof(var_rec_t, size),          1,                      NULL, NATIVE_FLAGS},
    {"data",        RecordObject::UINT8,    sizeof(var_rec_t),                  0,                      NULL, NATIVE_FLAGS}
};

const char* SwotL2Reader::scanRecType = "swotl2rec.scan";
const RecordObject::fieldDef_t SwotL2Reader::scanRecDef[] = {
    {"latitude",    RecordObject::DOUBLE,   offsetof(scan_rec_t, latitude),     1,                      NULL, NATIVE_FLAGS},
    {"longitude",   RecordObject::DOUBLE,   offsetof(scan_rec_t, longitude),    1,                      NULL, NATIVE_FLAGS}
};

const char* SwotL2Reader::geoRecType = "swotl2rec.geo";
const RecordObject::fieldDef_t SwotL2Reader::geoRecDef[] = {
    {"scan",        RecordObject::USER,     offsetof(geo_rec_t, scan),          0,                      scanRecType, NATIVE_FLAGS}
};

/******************************************************************************
 * SWOT L2 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int SwotL2Reader::luaCreate (lua_State* L)
{
    Asset* asset = NULL;
    SwotParms* parms = NULL;

    try
    {
        /* Get Parameters */
        asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        parms = (SwotParms*)getLuaObject(L, 4, SwotParms::OBJECT_TYPE);
        bool send_terminator = getLuaBoolean(L, 5, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new SwotL2Reader(L, asset, resource, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
        if(asset) asset->releaseLuaObject();
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating SwotL2Reader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void SwotL2Reader::init (void)
{
    RECDEF(varRecType,  varRecDef,  sizeof(var_rec_t),  NULL);
    RECDEF(scanRecType, scanRecDef, sizeof(scan_rec_t), NULL);
    RECDEF(geoRecType,  geoRecDef,  sizeof(geo_rec_t),  NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SwotL2Reader::SwotL2Reader (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, SwotParms* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    context {},
    region(_asset, _resource, _parms, &context),
    read_timeout_ms(_parms->read_timeout * 1000)
{
    /* Initialize Reader */
    asset       = _asset;
    resource    = StringLib::duplicate(_resource);
    parms       = _parms;
    active      = true;
    numComplete = 0;
    threadCount = 1 + parms->variables.length();

    /* Create Publisher */
    outQ = new Publisher(outq_name);
    sendTerminator = _send_terminator;

    /* Clear Statistics */
    stats.variables_read       = 0;
    stats.variables_filtered   = 0;
    stats.variables_sent       = 0;
    stats.variables_dropped    = 0;
    stats.variables_retried    = 0;

    /* Initialize Geo Thread */
    geoPid = new Thread(geoThread, this);

    /* Initialize Variable Threads */
    if(threadCount > 1)
    {
        varPid = new Thread* [threadCount - 1];
        for(int i = 0; i < threadCount - 1; i++)
        {
            info_t* info = new info_t;
            info->reader = this;
            info->variable_name = parms->variables[i].str(true);
            varPid[i] = new Thread(varThread, info);
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SwotL2Reader::~SwotL2Reader (void)
{
    active = false;

    if(geoPid) delete geoPid;
    for(int i = 0; i < threadCount; i++)
    {
        if(varPid[i]) delete varPid[i];
    }

    delete outQ;

    parms->releaseLuaObject();

    delete [] resource;

    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
SwotL2Reader::Region::Region (Asset* asset, const char* resource, SwotParms* parms, H5Coro::context_t* context):
    read_timeout_ms (parms->read_timeout * 1000),
    lat             (asset, resource, "latitude", context),
    lon             (asset, resource, "longitude", context),
    inclusion_mask  (NULL),
    inclusion_ptr   (NULL)
{
    /* Join Reads */
    lat.join(read_timeout_ms, true);
    lon.join(read_timeout_ms, true);

    /* Initialize Region */
    first_line = 0;
    num_lines = H5Coro::ALL_ROWS;

    /* Determine Spatial Extent */
    if(parms->raster != NULL)
    {
        rasterregion(parms);
    }
    else if(parms->polygon.length() > 0)
    {
        polyregion(parms);
    }
    else
    {
        num_lines = MIN(lat.size, lon.size);
    }

    /* Check If Anything to Process */
    if(num_lines <= 0)
    {
        cleanup();
        throw RunTimeException(DEBUG, RTE_EMPTY_SUBSET, "empty spatial region for %s", resource);
    }

    /* Trim Geospatial Datasets Read from File */
    lat.trim(first_line);
    lon.trim(first_line);
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
SwotL2Reader::Region::~Region (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * Region::cleanup
 *----------------------------------------------------------------------------*/
void SwotL2Reader::Region::cleanup (void)
{
    if(inclusion_mask) delete [] inclusion_mask;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void SwotL2Reader::Region::polyregion (SwotParms* parms)
{
    int points_in_polygon = parms->polygon.length();

    /* Determine Best Projection To Use */
    MathLib::proj_t projection = MathLib::PLATE_CARREE;
    if(lat[0] > 70.0) projection = MathLib::NORTH_POLAR;
    else if(lat[0] < -70.0) projection = MathLib::SOUTH_POLAR;

    /* Project Polygon */
    List<MathLib::coord_t>::Iterator poly_iterator(parms->polygon);
    MathLib::point_t* projected_poly = new MathLib::point_t [points_in_polygon];
    for(int i = 0; i < points_in_polygon; i++)
    {
        projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
    }

    /* Find First and Last Lines in Polygon */
    bool first_line_found = false;
    bool last_line_found = false;
    int line = 0;
    while(line < lat.size)
    {
        bool inclusion = false;

        /* Project Line Coordinate */
        MathLib::coord_t footprint_coord = {lon[line], lat[line]};
        MathLib::point_t footprint_point = MathLib::coord2point(footprint_coord, projection);

        /* Test Inclusion */
        if(MathLib::inpoly(projected_poly, points_in_polygon, footprint_point))
        {
            inclusion = true;
        }

        /* Find First Line */
        if(!first_line_found && inclusion)
        {
            /* Set First Line */
            first_line_found = true;
            first_line = line;
        }
        else if(first_line_found && !last_line_found && !inclusion)
        {
            /* Set Last Line */
            last_line_found = true;
            break; // full extent found!
        }

        /* Bump Line */
        line++;
    }

    /* Set Number of Segments */
    if(first_line_found)
    {
        num_lines = line - first_line;
    }

    /* Delete Projected Polygon */
    delete [] projected_poly;
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void SwotL2Reader::Region::rasterregion (SwotParms* parms)
{
    /* Allocate Inclusion Mask */
    if(lat.size <= 0) return;
    inclusion_mask = new bool [lat.size];
    inclusion_ptr = inclusion_mask;

    /* Loop Throuh Lines */
    bool first_line_found = false;
    long last_line = 0;
    int line = 0;
    while(line < lat.size)
    {
        /* Check Inclusion */
        bool inclusion = parms->raster->includes(lon[line], lat[line]);
        inclusion_mask[line] = inclusion;

        /* If Coordinate Is In Raster */
        if(inclusion)
        {
            if(!first_line_found)
            {
                first_line_found = true;
                first_line = line;
                last_line = line;
            }
            else
            {
                last_line = line;
            }
        }

        /* Bump Line */
        line++;
    }

    /* Set Number of Lines */
    if(first_line_found)
    {
        num_lines = last_line - first_line + 1;

        /* Trim Inclusion Mask */
        inclusion_ptr = &inclusion_mask[first_line];
    }
}

/*----------------------------------------------------------------------------
 * geoThread
 *----------------------------------------------------------------------------*/
void* SwotL2Reader::geoThread (void* parm)
{
    /* Get Thread Info */
    SwotL2Reader* reader = (SwotL2Reader*)parm;
    SwotParms* parms = reader->parms;

    /* Calculate Total Size of Record Data */
    int total_size = sizeof(scan_rec_t) * reader->region.num_lines;

    /* Create Record Object */
    RecordObject rec_obj(geoRecType, total_size);
    geo_rec_t* rec_data = (geo_rec_t*)rec_obj.getRecordData();

    /* Populate Record Object */
    for(int i = 0; i < reader->region.num_lines; i++)
    {
        rec_data->scan[i].latitude = reader->region.lat[i];
        rec_data->scan[i].longitude = reader->region.lon[i];
    }

    /* Post Record */
    unsigned char* rec_buf;
    int rec_size = rec_obj.serialize(&rec_buf, RecordObject::REFERENCE);
    int post_status = MsgQ::STATE_TIMEOUT;
    while( reader->active && ((post_status = reader->outQ->postCopy(rec_buf, rec_size, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT) );
    if(post_status <= 0)
    {
        mlog(CRITICAL, "Failed (%d) to post geo record for %s", post_status, reader->resource);
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Count Completion */
        reader->numComplete++;

        /* Indicate End of Data */
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", reader->resource);
            if(reader->sendTerminator) reader->outQ->postCopy("", 0);
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * varThread
 *----------------------------------------------------------------------------*/
void* SwotL2Reader::varThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    SwotL2Reader* reader = (SwotL2Reader*)info->reader;
    SwotParms* parms = reader->parms;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Initialize Results */
    H5Coro::info_t results;
    results.data = NULL;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "swot_l2_reader", "{\"asset\":\"%s\", \"resource\":\"%s\"}", reader->asset->getName(), reader->resource);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Read Dataset */
        results = H5Coro::read(reader->asset, reader->resource, info->variable_name, RecordObject::DYNAMIC, H5Coro::ALL_COLS, reader->region.first_line, reader->region.num_lines, &(reader->context));

        /* Post Results to Output Queue */
        if(results.data)
        {
            /* Create Record Object */
            RecordObject rec_obj(varRecType);
            var_rec_t* rec_data = (var_rec_t*)rec_obj.getRecordData();
            StringLib::copy(rec_data->granule, reader->resource, MAX_GRANULE_NAME_STR);
            StringLib::copy(rec_data->variable, info->variable_name, MAX_VARIABLE_NAME_STR);
            rec_data->datatype = (uint32_t)results.datatype;
            rec_data->elements = results.elements;
            rec_data->width = results.elements / reader->region.num_lines;
            rec_data->size = results.datasize;

            /* Post Record */
            unsigned char* rec_buf;
            int rec_size = rec_obj.serialize(&rec_buf, RecordObject::REFERENCE, sizeof(var_rec_t) + results.datasize);
            int post_status = MsgQ::STATE_TIMEOUT;
            while( reader->active && ((post_status = reader->outQ->postCopy(rec_buf, rec_size - results.datasize, results.data, results.datasize, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT) )
            {
                local_stats.variables_retried++;
            }

            /* Handle Errors */
            if(post_status > 0)
            {
                local_stats.variables_sent++;
            }
            else
            {
                mlog(CRITICAL, "Failed (%d) to post variable: %s/%s", post_status, reader->resource, info->variable_name);
                local_stats.variables_dropped++;
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failure during processing of %s/%s: %s", reader->resource, info->variable_name, e.what());
        LuaEndpoint::generateExceptionStatus(e.code(), e.level(), reader->outQ, &reader->active, "%s: (%s)", e.what(), reader->resource);
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Count Completion */
        reader->numComplete++;

        /* Update Statistics */
        reader->stats.variables_read += local_stats.variables_read;
        reader->stats.variables_filtered += local_stats.variables_filtered;
        reader->stats.variables_sent += local_stats.variables_sent;
        reader->stats.variables_dropped += local_stats.variables_dropped;
        reader->stats.variables_retried += local_stats.variables_retried;

        /* Indicate End of Data */
        if(reader->numComplete == reader->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", reader->resource);
            if(reader->sendTerminator) reader->outQ->postCopy("", 0);
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up */
    if(results.data) delete [] results.data;
    delete [] info->variable_name;
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
