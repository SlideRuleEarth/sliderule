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
            INT8            = 0,
            INT16           = 1,
            INT32           = 2,
            INT64           = 3,
            UINT8           = 4,
            UINT16          = 5,
            UINT32          = 6,
            UINT64          = 7,
            FLOAT           = 8,
            DOUBLE          = 9,
            STRING          = 10,
            NUM_ENCODINGS   = 11
        } encoding_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit        Field       (encoding_t _encoding): encoding(_encoding) {}
        virtual         ~Field      (void) = default;

        encoding_t      getEncoding (void) const { return encoding; }
        virtual bool    toJson      (string& str) = 0;
        virtual int     toLua       (lua_State* L) = 0;
        virtual bool    fromJson    (const string& str) = 0;
        virtual int     fromLua     (lua_State* L) = 0;

        inline static encoding_t getFieldEncoding(const int8_t& v)      { (void)v; return Field::INT8;   }
        inline static encoding_t getFieldEncoding(const int16_t& v)     { (void)v; return Field::INT16;  }
        inline static encoding_t getFieldEncoding(const int32_t& v)     { (void)v; return Field::INT32;  }
        inline static encoding_t getFieldEncoding(const int64_t& v)     { (void)v; return Field::INT64;  }
        inline static encoding_t getFieldEncoding(const uint8_t& v)     { (void)v; return Field::UINT8;  }
        inline static encoding_t getFieldEncoding(const uint16_t& v)    { (void)v; return Field::UINT16; }
        inline static encoding_t getFieldEncoding(const uint32_t& v)    { (void)v; return Field::UINT32; }
        inline static encoding_t getFieldEncoding(const uint64_t& v)    { (void)v; return Field::UINT64; }
        inline static encoding_t getFieldEncoding(const float& v)       { (void)v; return Field::FLOAT;  }
        inline static encoding_t getFieldEncoding(const double& v)      { (void)v; return Field::DOUBLE; }
        inline static encoding_t getFieldEncoding(const char* v)        { (void)v; return Field::STRING; }

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const encoding_t encoding;
};

#endif  /* __field__ */
