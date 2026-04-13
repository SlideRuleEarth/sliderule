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
#include "LuaObject.h"
#include "H5Object.h"
#include "CasalsFields.h"
#include "Casals1bDataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Casals1bDataFrame::LUA_META_NAME = "Casals1bDataFrame";
const struct luaL_Reg Casals1bDataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int Casals1bDataFrame::luaCreate (lua_State* L)
{
    if(lua_gettop(L) == 0)
        return createLuaObject(L, new Casals1bDataFrame(L, NULL, NULL, NULL));

    CasalsFields* _parms = NULL;
    H5Object* _hdf1b = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<CasalsFields*>(getLuaObject(L, 1, CasalsFields::OBJECT_TYPE));
        _hdf1b = dynamic_cast<H5Object*>(getLuaObject(L, 2, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 3, true, NULL);

        /* Return Reader Object */
        return createLuaObject(L, new Casals1bDataFrame(L, _parms, _hdf1b, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf1b) _hdf1b->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Casals1bDataFrame::Casals1bDataFrame (lua_State* L, CasalsFields* _parms, H5Object* _hdf1b, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"time_ns",         &time_ns,       "GPS nanoseconds"},
        {"latitude",        &latitude,      "latitude"},
        {"longitude",       &longitude,     "longitude"},
        {"refh",            &refh,          "reference height (m)"},
    },
    {
        {"granule",         &granule,       "source granule name"}
    },
    _parms ? CasalsFields::crsITRF2020() : NULL),
    granule(_hdf1b ? _hdf1b->name : "", META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms ? _parms->readTimeout.value * 1000 : 0),
    outQ(NULL),
    parms(_parms),
    hdf1b(_hdf1b),
    dfKey(_parms ? 1 : 0)
{
    /* Call Parent Class Initialization of GeoColumns */
    populateGeoColumns();

    /* Schema-only: skip all runtime initialization */
    if(!_parms) return;

    /* Setup Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Kickoff Reader Thread */
    active.store(true);
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Casals1bDataFrame::~Casals1bDataFrame (void)
{
    active.store(false);
    delete readerPid;
    delete outQ;
    if(parms) parms->releaseLuaObject();
    if(hdf1b) hdf1b->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t Casals1bDataFrame::getKey(void) const
{
    return dfKey;
}

/*----------------------------------------------------------------------------
 * Casals1bData::Constructor
 *----------------------------------------------------------------------------*/
Casals1bDataFrame::Casals1bData::Casals1bData (Casals1bDataFrame* df, const AreaOfInterest<double>& aoi):
    delta_time  (df->hdf1b, "delta_time",                   0, aoi.first_index, aoi.count),
    refh        (df->hdf1b, "refh",                         0, aoi.first_index, aoi.count),
    anc_data    (df->parms->anc_fields, df->hdf1b, NULL,    0, aoi.first_index, aoi.count)
{
    /* Join Hardcoded Reads */
    delta_time.join(df->readTimeoutMs, true);
    refh.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Casals1bDataFrame::subsettingThread (void* parm)
{
    /* Get Thread Info */
    Casals1bDataFrame* df = static_cast<Casals1bDataFrame*>(parm);

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "casals_1b_subsetter", "{\"context\":\"%s\"}", df->hdf1b->name);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to AreaOfInterest of Interest */
        const AreaOfInterest<double> aoi(df->hdf1b, "", "refh_latitude", "refh_longitude", df->parms, df->readTimeoutMs);

        /* Read CASALS 1B Datasets */
        const Casals1bData casals1b(df, aoi);

        /* Traverse All Elements In Dataset */
        int32_t current_element = -1;
        while(df->active.load() && (++current_element < aoi.count))
        {
            /* Check AreaOfInterest Mask */
            if(aoi.inclusion_ptr)
            {
                if(!aoi.inclusion_ptr[current_element])
                {
                    continue;
                }
            }

            /* Add Element to DataFrame */
            df->addRow();
            df->time_ns.append(CasalsFields::deltatime2timestamp(casals1b.delta_time[current_element]));
            df->latitude.append(aoi.latitude[current_element]);
            df->longitude.append(aoi.longitude[current_element]);
            df->refh.append(static_cast<float>(casals1b.refh[current_element]));

            /* Add Ancillary Elements */
            if(casals1b.anc_data.length() > 0) casals1b.anc_data.addToGDF(df, current_element);
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s: %s", df->hdf1b->name, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
