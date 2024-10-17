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

#include "OsApi.h"
#include "H5Coro.h"
#include "SwotFields.h"
#include "SwotL2Reader.h"

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define CONVERT_LAT(c) (((double)(c)) / 1000000.0)
#define CONVERT_LON(c) (fmod(((((double)(c)) / 1000000.0) + 180.0), 360.0) - 180.0)

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SwotL2Reader::OBJECT_TYPE = "SwotL2Reader";
const char* SwotL2Reader::LUA_META_NAME = "SwotL2Reader";
const struct luaL_Reg SwotL2Reader::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

const char* SwotL2Reader::varRecType = "swotl2var";
const RecordObject::fieldDef_t SwotL2Reader::varRecDef[] = {
    {"granule",     RecordObject::STRING,   offsetof(var_rec_t, granule),       MAX_GRANULE_NAME_STR,   NULL, NATIVE_FLAGS},
    {"variable",    RecordObject::STRING,   offsetof(var_rec_t, variable),      MAX_VARIABLE_NAME_STR,  NULL, NATIVE_FLAGS},
    {"datatype",    RecordObject::UINT32,   offsetof(var_rec_t, datatype),      1,                      NULL, NATIVE_FLAGS},
    {"elements",    RecordObject::UINT32,   offsetof(var_rec_t, elements),      1,                      NULL, NATIVE_FLAGS},
    {"width",       RecordObject::UINT32,   offsetof(var_rec_t, width),         1,                      NULL, NATIVE_FLAGS},
    {"size",        RecordObject::UINT32,   offsetof(var_rec_t, size),          1,                      NULL, NATIVE_FLAGS},
    {"data",        RecordObject::UINT8,    sizeof(var_rec_t),                  0,                      NULL, NATIVE_FLAGS}
};

const char* SwotL2Reader::scanRecType = "swotl2geo.scan";
const RecordObject::fieldDef_t SwotL2Reader::scanRecDef[] = {
    {"scan_id",     RecordObject::UINT64,   offsetof(scan_rec_t, scan_id),      1,                      NULL, NATIVE_FLAGS},
    {"latitude",    RecordObject::DOUBLE,   offsetof(scan_rec_t, latitude),     1,                      NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",   RecordObject::DOUBLE,   offsetof(scan_rec_t, longitude),    1,                      NULL, NATIVE_FLAGS | RecordObject::X_COORD}
};

const char* SwotL2Reader::geoRecType = "swotl2geo";
const RecordObject::fieldDef_t SwotL2Reader::geoRecDef[] = {
    {"granule",     RecordObject::STRING,   offsetof(geo_rec_t, granule),       MAX_GRANULE_NAME_STR,   NULL, NATIVE_FLAGS},
    {"scan",        RecordObject::USER,     offsetof(geo_rec_t, scan),          0,                      scanRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

/******************************************************************************
 * SWOT L2 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<outq_name>, <parms>, <send terminator>)
 *----------------------------------------------------------------------------*/
int SwotL2Reader::luaCreate (lua_State* L)
{
    SwotFields* parms = NULL;

    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        parms = dynamic_cast<SwotFields*>(getLuaObject(L, 2, SwotFields::OBJECT_TYPE));
        const bool send_terminator = getLuaBoolean(L, 3, true, true);

        /* Return Reader Object */
        return createLuaObject(L, new SwotL2Reader(L, outq_name, parms, send_terminator));
    }
    catch(const RunTimeException& e)
    {
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
SwotL2Reader::SwotL2Reader (lua_State* L, const char* outq_name, SwotFields* _parms, bool _send_terminator):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    context(NULL),
    region(NULL),
    varPid(NULL),
    geoPid(NULL),
    outQ(NULL)
{
    /* Initialize Reader */
    parms       = _parms;
    active      = true;
    numComplete = 0;
    threadCount = 0;

    try
    {
        /* Create H5Coro Context */
        context = new H5Coro::Context(parms->asset.asset, parms->getResource());

        /* Initialize Region */
        region = new Region(context, _parms);

        /* Create Publisher */
        outQ = new Publisher(outq_name);
        sendTerminator = _send_terminator;

        /* Clear Statistics */
        stats.variables_read       = 0;
        stats.variables_filtered   = 0;
        stats.variables_sent       = 0;
        stats.variables_dropped    = 0;
        stats.variables_retried    = 0;

        /* Check If Anything to Process */
        if(region->num_lines > 0)
        {
            /* Set Thread Count = geoThread + varThreads */
            threadCount = 1 + parms->variables.length();

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
                    info->variable_name = StringLib::duplicate(parms->variables[i].c_str());
                    varPid[i] = new Thread(varThread, info);
                }
            }
        }
        else
        {
            /* Report Empty Region */
            alert(INFO, RTE_INFO, outQ, &active, "Empty spatial region for %s", resource);

            /* Terminate */
            checkComplete();
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to create SWOT reader");

        /* Terminate */
        active = false;
        checkComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SwotL2Reader::~SwotL2Reader (void)
{
    active = false;

    delete geoPid;
    for(int i = 0; i < threadCount - 1; i++)
    {
        delete varPid[i];
    }

    delete outQ;

    delete region;

    delete context;

    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
SwotL2Reader::Region::Region (H5Coro::Context* context, const SwotFields* _parms):
    lat             (context, "latitude_nadir"),
    lon             (context, "longitude_nadir"),
    inclusion_mask  (NULL),
    inclusion_ptr   (NULL)
{
    /* Join Reads */
    lat.join(_parms->readTimeout.value * 1000, true);
    lon.join(_parms->readTimeout.value * 1000, true);

    /* Initialize Region */
    first_line = 0;
    num_lines = H5Coro::ALL_ROWS;

    /* Determine Spatial Extent */
    if(_parms->regionMask.valid())
    {
        rasterregion(_parms);
    }
    else if(_parms->pointsInPolygon.value > 0)
    {
        polyregion(_parms);
    }
    else
    {
        num_lines = lat.size;
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
void SwotL2Reader::Region::cleanup (void) const
{
    delete [] inclusion_mask;
}

/*----------------------------------------------------------------------------
 * Region::polyregion
 *----------------------------------------------------------------------------*/
void SwotL2Reader::Region::polyregion (const SwotFields* _parms)
{
    /* Find First and Last Lines in Polygon */
    bool first_line_found = false;
    bool last_line_found = false;
    int line = 0;
    while(line < lat.size && !last_line_found)
    {
        /* Test Inclusion */
        const bool inclusion = _parms->polyIncludes(CONVERT_LON(lon[line]), CONVERT_LAT(lat[line]));

        /* Find First Line */
        if(!first_line_found && inclusion)
        {
            /* Set First Line */
            first_line_found = true;
            first_line = line;
        }
        else if(first_line_found && !inclusion)
        {
            /* Set Last Line */
            last_line_found = true;
        }

        /* Bump Line */
        line++;
    }

    /* Set Number of Segments */
    if(first_line_found)
    {
        num_lines = (line - 1) - first_line;
    }
}

/*----------------------------------------------------------------------------
 * Region::rasterregion
 *----------------------------------------------------------------------------*/
void SwotL2Reader::Region::rasterregion (const SwotFields* _parms)
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
        const bool inclusion = _parms->maskIncludes(CONVERT_LON(lon[line]), CONVERT_LAT(lat[line]));
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
 * checkComplete
 *----------------------------------------------------------------------------*/
void SwotL2Reader::checkComplete (void)
{
    /* Handle Global Reader Updates */
    threadMut.lock();
    {
        /* Count Completion */
        numComplete++;

        /* Indicate End of Data */
        if(numComplete >= threadCount)
        {
            mlog(INFO, "Completed processing resource %s", resource);
            if(sendTerminator) outQ->postCopy("", 0, SYS_TIMEOUT);
            signalComplete();
        }
    }
    threadMut.unlock();
}

/*----------------------------------------------------------------------------
 * geoThread
 *----------------------------------------------------------------------------*/
void* SwotL2Reader::geoThread (void* parm)
{
    /* Get Thread Info */
    SwotL2Reader* reader = static_cast<SwotL2Reader*>(parm);

    /* Calculate Total Size of Record Data */
    const int total_size = offsetof(geo_rec_t, scan) + (sizeof(scan_rec_t) * reader->region->num_lines);

    /* Create Record Object */
    RecordObject rec_obj(geoRecType, total_size);
    geo_rec_t* rec_data = reinterpret_cast<geo_rec_t*>(rec_obj.getRecordData());

    /* Populate Record Object */
    StringLib::copy(rec_data->granule, reader->resource, MAX_GRANULE_NAME_STR);
    for(int i = 0; i < reader->region->num_lines; i++)
    {
        rec_data->scan[i].scan_id = (uint32_t)reader->region->lat[i];
        rec_data->scan[i].scan_id <<= 32;
        rec_data->scan[i].scan_id |= (uint32_t)reader->region->lon[i];
        rec_data->scan[i].latitude = CONVERT_LAT(reader->region->lat[i]);
        rec_data->scan[i].longitude = CONVERT_LON(reader->region->lon[i]);
    }

    /* Post Record */
    unsigned char* rec_buf;
    const int rec_size = rec_obj.serialize(&rec_buf, RecordObject::REFERENCE);
    int post_status = MsgQ::STATE_TIMEOUT;
    while( reader->active && ((post_status = reader->outQ->postCopy(rec_buf, rec_size, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT) );
    if(post_status <= 0)
    {
        mlog(CRITICAL, "Failed (%d) to post geo record for %s", post_status, reader->resource);
    }

    /* Complete */
    reader->checkComplete();

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * varThread
 *----------------------------------------------------------------------------*/
void* SwotL2Reader::varThread (void* parm)
{
    /* Get Thread Info */
    info_t* info = static_cast<info_t*>(parm);
    SwotL2Reader* reader = info->reader;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Initialize Results */
    H5Coro::info_t results;
    results.data = NULL;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, reader->traceId, "swot_l2_reader", "{\"asset\":\"%s\", \"resource\":\"%s\"}", reader->asset->getName(), reader->resource);

    try
    {
        /* Read Dataset */
        const H5Coro::range_t slice[2] = {{reader->region->first_line, reader->region->first_line + reader->region->num_lines}, {0, H5Coro::EOR}};
        results = H5Coro::read(reader->context, info->variable_name, RecordObject::DYNAMIC, slice, 2, false, trace_id);

        /* Post Results to Output Queue */
        if(results.data)
        {
            /* Create Record Object */
            RecordObject rec_obj(varRecType);
            var_rec_t* rec_data = reinterpret_cast<var_rec_t*>(rec_obj.getRecordData());
            StringLib::copy(rec_data->granule, reader->resource, MAX_GRANULE_NAME_STR);
            StringLib::copy(rec_data->variable, info->variable_name, MAX_VARIABLE_NAME_STR);
            rec_data->datatype = static_cast<uint32_t>(results.datatype);
            rec_data->elements = results.elements;
            rec_data->width = results.elements / reader->region->num_lines;
            rec_data->size = results.datasize;

            /* Post Record */
            unsigned char* rec_buf;
            const int rec_size = rec_obj.serialize(&rec_buf, RecordObject::REFERENCE, sizeof(var_rec_t) + results.datasize);
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
        alert(e.level(), e.code(), reader->outQ, &reader->active, "Failure on %s/%s: %s", reader->resource, info->variable_name, e.what());
    }

    /* Update Statistics */
    reader->threadMut.lock();
    {
        reader->stats.variables_read += local_stats.variables_read;
        reader->stats.variables_filtered += local_stats.variables_filtered;
        reader->stats.variables_sent += local_stats.variables_sent;
        reader->stats.variables_dropped += local_stats.variables_dropped;
        reader->stats.variables_retried += local_stats.variables_retried;
    }
    reader->threadMut.unlock();

    /* Complete */
    reader->checkComplete();

    /* Clean Up */
    operator delete[](results.data, std::align_val_t(H5CORO_DATA_ALIGNMENT));
    delete [] info->variable_name;
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int SwotL2Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    SwotL2Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = dynamic_cast<SwotL2Reader*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        const bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "read",        lua_obj->stats.variables_read);
        LuaEngine::setAttrInt(L, "filtered",    lua_obj->stats.variables_filtered);
        LuaEngine::setAttrInt(L, "sent",        lua_obj->stats.variables_sent);
        LuaEngine::setAttrInt(L, "dropped",     lua_obj->stats.variables_dropped);
        LuaEngine::setAttrInt(L, "retried",     lua_obj->stats.variables_retried);

        /* Clear if Requested */
        if(with_clear) memset(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning stats %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
