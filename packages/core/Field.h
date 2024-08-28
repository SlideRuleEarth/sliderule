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

/******************************************************************************
 * CLASS
 ******************************************************************************/

class Field
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            INVALID         = -1,
            BOOLEAN         = 0,   
            INT8            = 1,
            INT16           = 2,
            INT32           = 3,
            INT64           = 4,
            UINT8           = 5,
            UINT16          = 6,
            UINT32          = 7,
            UINT64          = 8,
            FLOAT           = 9,
            DOUBLE          = 10,
            STRING          = 11,
            NUM_ENCODINGS   = 12
        } encoding_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit        Field       (encoding_t _encoding): encoding(_encoding) {}
        virtual         ~Field      (void) = default;

        encoding_t      getEncoding (void) const { return encoding; }
        virtual string  toJson      (void) = 0;
        virtual int     toLua       (lua_State* L) = 0;
        virtual void    fromJson    (const string& str) = 0;
        virtual void    fromLua     (lua_State* L, int index) = 0;

        // encoding 
        inline static encoding_t getFieldEncoding(const bool& v)     { (void)v; return Field::BOOLEAN; }
        inline static encoding_t getFieldEncoding(const int8_t& v)   { (void)v; return Field::INT8;    }
        inline static encoding_t getFieldEncoding(const int16_t& v)  { (void)v; return Field::INT16;   }
        inline static encoding_t getFieldEncoding(const int32_t& v)  { (void)v; return Field::INT32;   }
        inline static encoding_t getFieldEncoding(const int64_t& v)  { (void)v; return Field::INT64;   }
        inline static encoding_t getFieldEncoding(const uint8_t& v)  { (void)v; return Field::UINT8;   }
        inline static encoding_t getFieldEncoding(const uint16_t& v) { (void)v; return Field::UINT16;  }
        inline static encoding_t getFieldEncoding(const uint32_t& v) { (void)v; return Field::UINT32;  }
        inline static encoding_t getFieldEncoding(const uint64_t& v) { (void)v; return Field::UINT64;  }
        inline static encoding_t getFieldEncoding(const float& v)    { (void)v; return Field::FLOAT;   }
        inline static encoding_t getFieldEncoding(const double& v)   { (void)v; return Field::DOUBLE;  }
        inline static encoding_t getFieldEncoding(const string& v)   { (void)v; return Field::STRING;  }

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
        inline static string convertToJson(const string& v)   { return "\"" + string(v) + "\""; }

        // tolua
        inline static int convertToLua(lua_State* L, const bool& v)     { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const int8_t& v)   { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const int16_t& v)  { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const int32_t& v)  { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const int64_t& v)  { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const uint8_t& v)  { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const uint16_t& v) { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const uint32_t& v) { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const uint64_t& v) { lua_pushinteger(L, v); return 1;        }
        inline static int convertToLua(lua_State* L, const float& v)    { lua_pushnumber(L, v);  return 1;        }
        inline static int convertToLua(lua_State* L, const double& v)   { lua_pushnumber(L, v);  return 1;        }
        inline static int convertToLua(lua_State* L, string& v)         { lua_pushstring(L, v.c_str()); return 1; }

        // fromjson
        inline static void convertFromJson(const string& str, bool& v)  { 
            if(!StringLib::str2bool(str.c_str(), &v))
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to parse boolean element: %s", str.c_str()); 
            }
        }
        inline static void convertFromJson(const string& str, int8_t& v)   { v = static_cast<int8_t>(std::stoi(str));     }
        inline static void convertFromJson(const string& str, int16_t& v)  { v = static_cast<int16_t>(std::stoi(str));    }
        inline static void convertFromJson(const string& str, int32_t& v)  { v = static_cast<int32_t>(std::stol(str));    }
        inline static void convertFromJson(const string& str, int64_t& v)  { v = static_cast<int64_t>(std::stoll(str));   }
        inline static void convertFromJson(const string& str, uint8_t& v)  { v = static_cast<uint8_t>(std::stoi(str));    }
        inline static void convertFromJson(const string& str, uint16_t& v) { v = static_cast<uint16_t>(std::stoi(str));   }
        inline static void convertFromJson(const string& str, uint32_t& v) { v = static_cast<uint32_t>(std::stoul(str));  }
        inline static void convertFromJson(const string& str, uint64_t& v) { v = static_cast<uint64_t>(std::stoull(str)); }
        inline static void convertFromJson(const string& str, float& v)    { v = std::stof(str);                          }
        inline static void convertFromJson(const string& str, double& v)   { v = std::stod(str);                          }
        inline static void convertFromJson(const string& str, string& v)   { 
            const size_t first_pos = str.find('"');
            size_t last_pos = first_pos;
            size_t pos = first_pos;
            while (pos != std::string::npos)
            {
                last_pos = pos;
                pos = str.find('"', pos + 1);
            }
            if(first_pos == std::string::npos) throw RunTimeException(CRITICAL, RTE_ERROR, "missing string quotations");
            else if(first_pos == last_pos) throw RunTimeException(CRITICAL, RTE_ERROR, "missing matching string quotations");
            v = str.substr(first_pos + 1, last_pos - first_pos - 1);
        }

        // fromlua
        inline static void convertFromLua(lua_State* L, int index, bool& v)     { v = LuaObject::getLuaBoolean(L, index);                        }
        inline static void convertFromLua(lua_State* L, int index, int8_t& v)   { v = static_cast<int8_t>(LuaObject::getLuaInteger(L, index));   }
        inline static void convertFromLua(lua_State* L, int index, int16_t& v)  { v = static_cast<int16_t>(LuaObject::getLuaInteger(L, index));  }
        inline static void convertFromLua(lua_State* L, int index, int32_t& v)  { v = static_cast<int32_t>(LuaObject::getLuaInteger(L, index));  }
        inline static void convertFromLua(lua_State* L, int index, int64_t& v)  { v = static_cast<int64_t>(LuaObject::getLuaInteger(L, index));  }
        inline static void convertFromLua(lua_State* L, int index, uint8_t& v)  { v = static_cast<uint8_t>(LuaObject::getLuaInteger(L, index));  }
        inline static void convertFromLua(lua_State* L, int index, uint16_t& v) { v = static_cast<uint16_t>(LuaObject::getLuaInteger(L, index)); }
        inline static void convertFromLua(lua_State* L, int index, uint32_t& v) { v = static_cast<uint32_t>(LuaObject::getLuaInteger(L, index)); }
        inline static void convertFromLua(lua_State* L, int index, uint64_t& v) { v = static_cast<uint64_t>(LuaObject::getLuaInteger(L, index)); }
        inline static void convertFromLua(lua_State* L, int index, float& v)    { v = static_cast<float>(LuaObject::getLuaFloat(L, index));      }
        inline static void convertFromLua(lua_State* L, int index, double& v)   { v = LuaObject::getLuaFloat(L, index);                          }
        inline static void convertFromLua(lua_State* L, int index, string& v)   { v = LuaObject::getLuaString(L, index);                         }

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        encoding_t encoding;
};

#endif  /* __field__ */
