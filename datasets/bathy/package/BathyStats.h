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

#ifndef __bathy_stats__
#define __bathy_stats__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "FieldDictionary.h"
#include "FieldElement.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

struct BathyStats: public LuaObject, public FieldDictionary 
{
    /*--------------------------------------------------------------------
     * Constants
     *--------------------------------------------------------------------*/

    static const char* OBJECT_TYPE;
    static const char* LUA_META_NAME;
    static const struct luaL_Reg LUA_META_TABLE[];

    /*--------------------------------------------------------------------
     * Methods
     *--------------------------------------------------------------------*/

            BathyStats  (lua_State* L);
            ~BathyStats (void) override = default;

    void    update      (const BathyStats& stats);

    /*--------------------------------------------------------------------
     * Data
     *--------------------------------------------------------------------*/

    FieldElement<bool>      valid                   {true};
    FieldElement<uint64_t>  photonCount             {0};
    FieldElement<uint64_t>  subaqueousPhotons       {0.0};
    FieldElement<double>    correctionsDuration     {0.0};
    FieldElement<double>    qtreesDuration          {0.0};
    FieldElement<double>    coastnetDuration        {0.0};
    FieldElement<double>    openoceansppDuration    {0.0};
    
    Mutex statsLock;
};

#endif  /* __bathy_stats__ */
