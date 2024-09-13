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

#ifndef __geo_data_frame__
#define __geo_data_frame__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "FieldDictionary.h"
#include "FieldElement.h"
#include "FieldColumn.h"
#include "RegionMask.h"
#include "List.h"
#include "MathLib.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class GeoDataFrame: public LuaObject, public Field
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* GDF;
        static const char* META;

        /*--------------------------------------------------------------------
         * Subclasses
         *--------------------------------------------------------------------*/

        struct FrameColumn: public LuaObject
        {
            static const char* OBJECT_TYPE;
            static const char* LUA_META_NAME;
            static const struct luaL_Reg LUA_META_TABLE[];

            static int luaGetData (lua_State* L);

            FrameColumn(lua_State* L, const Field* _column);
            ~FrameColumn(void) override = default;

            const Field* column;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              luaExport       (lua_State* L);
        static int              luaImport       (lua_State* L);
        static int              luaGetColumnData(lua_State* L);
        static int              luaGetMetaData  (lua_State* L);

        long                    length          (void);
        long                    addRow          (void);
        void                    addColumnData   (const char* name, Field* column);
        Field*                  getColumnData   (const char* name);
        void                    addMetaData     (const char* name, Field* column);
        Field*                  getMetaData     (const char* name);

        void                    setTimeColumn   (const char* name, FieldColumn<int64_t>* time_column = NULL);
        void                    setXColumn      (const char* name, FieldColumn<double>* x_column = NULL);
        void                    setYColumn      (const char* name, FieldColumn<double>* y_column = NULL);
        void                    setZColumn      (const char* name, FieldColumn<double>* z_column = NULL);

        FieldColumn<int64_t>&   getTimeColumn   (void);
        FieldColumn<double>&    getXColumn      (void);
        FieldColumn<double>&    getYColumn      (void);
        FieldColumn<double>&    getZColumn      (void);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GeoDataFrame    (lua_State* L, 
                         const char* meta_name,
                         const struct luaL_Reg meta_table[],
                         const std::initializer_list<FieldDictionary::entry_t>& column_list, 
                         const std::initializer_list<FieldDictionary::entry_t>& meta_list,
                         FieldColumn<int64_t>* time_column = NULL,
                         FieldColumn<double>* x_column = NULL,
                         FieldColumn<double>* y_column = NULL,
                         FieldColumn<double>* z_column = NULL);
        ~GeoDataFrame   (void) override = default;

        int     toLua   (lua_State* L) const override;
        void    fromLua (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        vector<long> index;
        FieldDictionary columnFields;
        FieldDictionary metaFields;

        FieldColumn<int64_t>* timeColumn;
        FieldColumn<double>* xColumn;
        FieldColumn<double>* yColumn;
        FieldColumn<double>* zColumn;
};

#endif  /* __geo_data_frame__ */
