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
#include "TimeLib.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class Field
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        // encodings - values
        static const uint32_t BOOL          = 0x0001;
        static const uint32_t INT8          = 0x0002;
        static const uint32_t INT16         = 0x0003;
        static const uint32_t INT32         = 0x0004;
        static const uint32_t INT64         = 0x0005;
        static const uint32_t UINT8         = 0x0006;
        static const uint32_t UINT16        = 0x0007;
        static const uint32_t UINT32        = 0x0008;
        static const uint32_t UINT64        = 0x0009;
        static const uint32_t FLOAT         = 0x000A;
        static const uint32_t DOUBLE        = 0x000B;
        static const uint32_t TIME8         = 0x000C;
        static const uint32_t STRING        = 0x000D;
        static const uint32_t USER          = 0x000E;
        static const uint32_t NESTED_COLUMN = 0x8000;
        static const uint32_t NESTED_ARRAY  = 0x4000;

        // encodings - columns
        static const uint32_t TIME_COLUMN   = 0x80000000;
        static const uint32_t X_COLUMN      = 0x40000000;
        static const uint32_t Y_COLUMN      = 0x20000000;
        static const uint32_t Z_COLUMN      = 0x10000000;


        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            FIELD         = -1,
            ELEMENT       = 0,
            ARRAY         = 1,
            ENUMERATION   = 2,
            LIST          = 3,
            COLUMN        = 4,
            DICTIONARY    = 5,
            DATAFRAME     = 6
        } type_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Field       (type_t _type, uint32_t _encoding): 
                                    type(_type), encoding(_encoding), provided(false), initialized(false) {};

        virtual         ~Field      (void) = default;
        
        virtual string  toJson      (void) const = 0;
        virtual int     toLua       (lua_State* L) const = 0;
        virtual void    fromLua     (lua_State* L, int index) = 0;

        virtual int toLua (lua_State* L, long key) const {
            (void)key;
            lua_pushnil(L);
            return 1;
        };

        virtual int toLua (lua_State* L, const string& key) const {
            (void)key;
            lua_pushnil(L);
            return 1;
        };

        uint32_t getValueEncoding(void) const { 
            return encoding & 0xFFFF; 
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        type_t type;        // child class type (dictionary, column, etc)
        uint32_t encoding;  // encoding = 0xn0000vv; n: upper bits for nested types; vv: <value type>
        bool provided;      // whether the field has been populated by the fromLua function
        bool initialized;   // whether the field has been initialized by any means
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

// tojson
inline static string convertToJson(const bool& v)     { if(v) return string("true"); else return string("false"); }
inline static string convertToJson(const int8_t& v)   { return std::to_string(v);       }
inline static string convertToJson(const int16_t& v)  { return std::to_string(v);       }
inline static string convertToJson(const int32_t& v)  { return std::to_string(v);       }
inline static string convertToJson(const int64_t& v)  { return std::to_string(v);       }
inline static string convertToJson(const uint8_t& v)  { return std::to_string(v);       }
inline static string convertToJson(const uint16_t& v) { return std::to_string(v);       }
inline static string convertToJson(const uint32_t& v) { return std::to_string(v);       }
inline static string convertToJson(const uint64_t& v) { return std::to_string(v);       }
inline static string convertToJson(const float& v)    { return std::to_string(v);       }
inline static string convertToJson(const double& v)   { return std::to_string(v);       }
inline static string convertToJson(const time8_t& v)  { 
    const int64_t gps = TimeLib::sysex2gpstime(v); 
    const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(gps);
    const TimeLib::date_t date = TimeLib::gmt2date(gmt);
    const double seconds = (double)gmt.second + ((double)gmt.millisecond / 1000.0);
    const FString str("%04d-%02d-%02dT%02d:%02d:%.03lfZ ", date.year, date.month, date.day, gmt.hour, gmt.minute, seconds);
    return str.c_str();
}
inline static string convertToJson(const string& v)   { return "\"" + string(v) + "\""; }

// tolua
inline int convertToLua(lua_State* L, const bool& v)     { lua_pushboolean(L, v); return 1; }
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
inline int convertToLua(lua_State* L, const time8_t& v)  { lua_pushinteger(L, v.nanoseconds);  return 1; }
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
inline void convertFromLua(lua_State* L, int index, time8_t& v)  { v = static_cast<int64_t>(LuaObject::getLuaInteger(L, index)); }
inline void convertFromLua(lua_State* L, int index, string& v)   { v = LuaObject::getLuaString(L, index); }

// encoding
inline uint32_t toEncoding(bool& v)     { (void)v; return Field::BOOL;   };
inline uint32_t toEncoding(int8_t& v)   { (void)v; return Field::INT8;   };
inline uint32_t toEncoding(int16_t& v)  { (void)v; return Field::INT16;  };
inline uint32_t toEncoding(int32_t& v)  { (void)v; return Field::INT32;  };
inline uint32_t toEncoding(int64_t& v)  { (void)v; return Field::INT64;  };
inline uint32_t toEncoding(uint8_t& v)  { (void)v; return Field::UINT8;  };
inline uint32_t toEncoding(uint16_t& v) { (void)v; return Field::UINT16; };
inline uint32_t toEncoding(uint32_t& v) { (void)v; return Field::UINT32; };
inline uint32_t toEncoding(uint64_t& v) { (void)v; return Field::UINT64; };
inline uint32_t toEncoding(float& v)    { (void)v; return Field::FLOAT;  };
inline uint32_t toEncoding(double& v)   { (void)v; return Field::DOUBLE; };
inline uint32_t toEncoding(time8_t& v)  { (void)v; return Field::TIME8;  };
inline uint32_t toEncoding(string& v)   { (void)v; return Field::STRING; };

// encoding
template<class T>
inline uint32_t getImpliedEncoding(void) {
    T dummy;
    return toEncoding(dummy);
}

#endif  /* __field__ */
