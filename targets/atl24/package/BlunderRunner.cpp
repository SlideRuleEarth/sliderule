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

#include "cleanup.h" // ATL24

#include "OsApi.h"
#include "GeoLib.h"
#include "FieldElement.h"
#include "BlunderRunner.h"
#include "Icesat2Parameters.h"
#include "Atl24DataFrame.h"

using namespace ATL24::cleanup;
using namespace ATL24::photon;

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* BlunderRunner::LUA_META_NAME = "BlunderRunner";
const struct luaL_Reg BlunderRunner::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BlunderRunner::luaCreate (lua_State* L)
{
    Icesat2Parameters* _parms = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Parameters*>(getLuaObject(L, 1, Icesat2Parameters::OBJECT_TYPE));
        return createLuaObject(L, new BlunderRunner(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BlunderRunner::BlunderRunner (lua_State* L, Icesat2Parameters* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BlunderRunner::~BlunderRunner (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BlunderRunner::run (GeoDataFrame* dataframe)
{
    bool status = true;

    // cast dataframe to ATL24 specific dataframe
    Atl24DataFrame& df = *dynamic_cast<Atl24DataFrame*>(dataframe);

    // convert dataframe to input structure of ATL24 v2 cleanup algorithm
    vector<Photon> p(df.length());
    for(size_t i = 0; i < static_cast<size_t>(df.length()); ++i)
    {
        // only the below members of the structure are used
        p[i].x_atc = df.x_atc[i];
        p[i].h_ph = df.ortho_h[i];
        p[i].class_ph = df.class_ph[i];
    }

    // execute ATL24 v2 cleanup algorithm
    Params params;
    const vector<size_t> q = do_cleanup(p, params);
    for(size_t i: q)
    {
        // check valid photon
        if(i >= static_cast<size_t>(df.length()))
        {
            mlog(CRITICAL, "Attempting to cleanup photon that does not exist: %ld >= %ld", i, df.length());
            status = false;
            break;
        }

        // check only bathy photons should have changed
        if(df.class_ph[i] != Atl24Fields::BATHYMETRY)
        {
            mlog(CRITICAL, "Attempting to cleanup photon that is not labelled bathymetry: [%ld] => %d", i, df.class_ph[i]);
            status = false;
            break;
        }

        // change classification
        df.class_ph[i] = Atl24Fields::UNCLASSIFIED;
        df.low_confidence_flag[i] = 0;
    }

    // add metadata to dataframe
    FieldElement<int64_t>* relabeled = new FieldElement<int64_t>(q.size());
    if(!df.addMetaData("relabeled", relabeled, StringLib::duplicate("boolean on whether photon was relabeled as part of the cleanup"), true))
    {
        mlog(CRITICAL, "Failed to add metadata to dataframe");
        delete relabeled;
    }

    // return success
    return status;
}
