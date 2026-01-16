/*
 * Copyright (c) 2025, University of Washington
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

#include "GediDataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GediDataFrame::GEDI_CRS = "\"EPSG:4326\"";

/******************************************************************************
 * GEDI DATAFRAME BASE
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
 GediDataFrame::GediDataFrame(lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[],
                             const std::initializer_list<FieldMap<FieldUntypedColumn>::init_entry_t>& column_list,
                             GediFields* _parms, H5Object* _hdf, const char* beam_str, const char* outq_name):
    GeoDataFrame(L, meta_name, meta_table, column_list,
    {
        {"beam",    &beam},
        {"orbit",   &orbit},
        {"track",   &track},
        {"granule", &granule}
    },
    GEDI_CRS),
    beam(0, META_COLUMN),
    orbit(static_cast<uint32_t>(_parms->granule_fields.orbit.value), META_COLUMN),
    track(static_cast<uint16_t>(_parms->granule_fields.track.value), META_COLUMN),
    granule(_hdf->name, META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    outQ(NULL),
    parms(_parms),
    hdf(_hdf),
    dfKey(0),
    beamStr(StringLib::duplicate(beam_str)),
    group{0}
{
    assert(_parms);
    assert(_hdf);

    /* Resolve Beam */
    const int beam_index = beamIndexFromString(beam_str);
    StringLib::format(group, sizeof(group), "%s", GediFields::beam2group(beam_index));
    GediFields::beam_t beam_id;
    convertFromIndex(beam_index, beam_id);
    beam = static_cast<uint8_t>(beam_id);

    /* Set DataFrame Key */
    dfKey = static_cast<okey_t>(beam_index);

    /* Optional Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Derived classes call populateDataframe after column members initialize */
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GediDataFrame::~GediDataFrame (void)
{
    active.store(false);
    delete readerPid;
    delete [] beamStr;
    delete outQ;
    if(parms) parms->releaseLuaObject();
    if(hdf) hdf->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t GediDataFrame::getKey (void) const
{
    return dfKey;
}
