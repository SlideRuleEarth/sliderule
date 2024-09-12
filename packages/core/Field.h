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

#ifndef __field__
#define __field__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "LuaObject.h"

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

// tolua
inline int convertToLua(lua_State* L, const bool& v)     { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const int8_t& v)   { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const int16_t& v)  { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const int32_t& v)  { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const int64_t& v)  { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const uint8_t& v)  { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const uint16_t& v) { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const uint32_t& v) { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const uint64_t& v) { lua_pushinteger(L, v); return 1; }
inline int convertToLua(lua_State* L, const float& v)    { lua_pushnumber(L, v);  return 1; }
inline int convertToLua(lua_State* L, const double& v)   { lua_pushnumber(L, v);  return 1; }
inline int convertToLua(lua_State* L, const string& v)   { lua_pushstring(L, v.c_str()); return 1; }

// fromlua
inline void convertFromLua(lua_State* L, int index, bool& v)     { v = LuaObject::getLuaBoolean(L, index); }
inline void convertFromLua(lua_State* L, int index, int8_t& v)   { v = static_cast<int8_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, int16_t& v)  { v = static_cast<int16_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, int32_t& v)  { v = static_cast<int32_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, int64_t& v)  { v = static_cast<int64_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, uint8_t& v)  { v = static_cast<uint8_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, uint16_t& v) { v = static_cast<uint16_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, uint32_t& v) { v = static_cast<uint32_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, uint64_t& v) { v = static_cast<uint64_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, float& v)    { v = static_cast<float>(LuaObject::getLuaFloat(L, index)); }
inline void convertFromLua(lua_State* L, int index, double& v)   { v = LuaObject::getLuaFloat(L, index); }
inline void convertFromLua(lua_State* L, int index, string& v)   { v = LuaObject::getLuaString(L, index); }

/******************************************************************************
 * CLASS
 ******************************************************************************/

class Field
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Field       (void) = default;
        virtual         ~Field      (void) = default;
        
        virtual int     toLua       (lua_State* L) const = 0;
        virtual void    fromLua     (lua_State* L, int index) = 0;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool provided {false};
};

#endif  /* __field__ */
