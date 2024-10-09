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

#ifndef __arrow_fields__
#define __arrow_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "FieldDictionary.h"
#include "FieldElement.h"
#include "FieldList.h"
#include "Asset.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * ARROW PARAMETERS CLASS
 ******************************************************************************/

class ArrowFields: public FieldDictionary
{
    public:

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/
        typedef enum {
            FEATHER = 1,
            PARQUET = 2,
            GEOPARQUET = 3,
            CSV = 4
        } format_t;

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        FieldElement<string>    path;                       // file system path to the file (includes filename)
        FieldElement<format_t>  format {PARQUET};           // format of the file
        FieldElement<bool>      openOnComplete {false};     // flag to client to open file on completion
        FieldElement<bool>      asGeo {false};              // whether to create a standard geo-based formatted file
        FieldElement<bool>      withChecksum {false};       // whether to perform checksum on file and send EOF record
        FieldElement<bool>      withValidation {false};     // whether to validate the arrow structure before outputing
        FieldElement<string>    assetName;
        FieldElement<string>    region;
        FieldList<string>       ancillaryFields;

        #ifdef __aws__
        CredentialStore::Credential credentials;
        #endif

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        ArrowFields     (void);
        ~ArrowFields    (void) override = default;

        void fromLua    (lua_State* L, int index) override;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const ArrowFields::format_t& v);
int convertToLua(lua_State* L, const ArrowFields::format_t& v);
void convertFromLua(lua_State* L, int index, ArrowFields::format_t& v);

inline uint32_t toEncoding(ArrowFields::format_t& v) { (void)v; return Field::INT32; }


#endif  /* __arrow_fields__ */
