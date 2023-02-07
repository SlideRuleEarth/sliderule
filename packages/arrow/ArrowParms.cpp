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

#include <lua.h>
#include "core.h"
#include "ArrowParms.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowParms::SELF               = "output";
const char* ArrowParms::PATH               = "path";
const char* ArrowParms::FORMAT             = "format";
const char* ArrowParms::OPEN_ON_COMPLETE   = "open_on_complete";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
void ArrowParms::ArrowParms (lua_State* L, int index):
    path                (NULL),
    format              (NATIVE),
    open_on_complete    (false)

{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
void ArrowParms::~ArrowParms (void)
{
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
bool ArrowParms::fromLua (lua_State* L, int index)
{
    bool provided = false;

    /* Must be a Table */
    if(lua_istable(L, index))
    {
        bool field_provided = false;

        /* Mark as Provided */
        provided = true;

        /* Output Path */
        lua_getfield(L, index, PATH);
        output.path = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, output.path, &field_provided));
        if(field_provided) mlog(DEBUG, "Setting %s to %s", PATH, output.path);
        lua_pop(L, 1);

        /* Output Format */
        lua_getfield(L, index, FORMAT);
        if(lua_isinteger(L, index))
        {
            output.format = (output_format_t)LuaObject::getLuaInteger(L, -1, true, output.format, &field_provided);
            if(output.format < 0 || output.format >= UNSUPPORTED)
            {
                mlog(ERROR, "Output format is unsupported: %d", output.format);
            }
        }
        else if(lua_isstring(L, index))
        {
            const char* output_fmt = LuaObject::getLuaString(L, -1, true, NULL, &field_provided);
            if(field_provided)
            {
                output.format = str2outputformat(output_fmt);
                if(output.format == UNSUPPORTED)
                {
                    mlog(ERROR, "Output format is unsupported: %s", output_fmt);
                }
            }
        }
        else if(!lua_isnil(L, index))
        {
            mlog(ERROR, "Output format must be provided as an integer or string");
        }
        if(field_provided) mlog(DEBUG, "Setting %s to %d", FORMAT, (int)output.format);
        lua_pop(L, 1);

        /* Output Open on Complete */
        lua_getfield(L, index, OPEN_ON_COMPLETE);
        output.open_on_complete = LuaObject::getLuaBoolean(L, -1, true, output.open_on_complete, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", OPEN_ON_COMPLETE, (int)output.open_on_complete);
        lua_pop(L, 1);
    }

    return provided;
}

/*----------------------------------------------------------------------------
 * str2outputformat
 *----------------------------------------------------------------------------*/
ArrowParms::format_t RqstParms::str2outputformat (const char* fmt_str)
{
    if     (StringLib::match(fmt_str, "native"))    return NATIVE;
    else if(StringLib::match(fmt_str, "feather"))   return FEATHER;
    else if(StringLib::match(fmt_str, "parquet"))   return PARQUET;
    else if(StringLib::match(fmt_str, "csv"))       return CSV;
    else                                            return UNSUPPORTED;
}
