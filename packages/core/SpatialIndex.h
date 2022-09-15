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

#ifndef __spatial_index__
#define __spatial_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "AssetIndex.h"
#include "MathLib.h"
#include "LuaObject.h"

/******************************************************************************
 * TIME INDEX CLASS
 ******************************************************************************/

typedef struct {
    MathLib::coord_t c0;
    MathLib::coord_t c1;
} spatialspan_t;

class SpatialIndex: public AssetIndex<spatialspan_t>
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        SpatialIndex    (lua_State* L, Asset* _asset, MathLib::proj_t _projection, int _threshold);
                        ~SpatialIndex   (void);

        static int      luaCreate       (lua_State* L);

        void            split           (node_t* node, spatialspan_t& lspan, spatialspan_t& rspan) override;
        bool            isleft          (node_t* node, const spatialspan_t& span) override;
        bool            isright         (node_t* node, const spatialspan_t& span) override;
        bool            intersect       (const spatialspan_t& span1, const spatialspan_t& span2) override;
        spatialspan_t   combine         (const spatialspan_t& span1, const spatialspan_t& span2) override;
        spatialspan_t   attr2span       (Dictionary<double>* attr, bool* provided=NULL) override;
        spatialspan_t   luatable2span   (lua_State* L, int parm) override;
        void            displayspan     (const spatialspan_t& span) override;

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            MathLib::point_t p0;
            MathLib::point_t p1;
        } projspan_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        projspan_t      project         (spatialspan_t span);
        spatialspan_t   restore         (projspan_t proj);

        static int      luaProject      (lua_State* L);
        static int      luaSphere       (lua_State* L);
        static int      luaSplit        (lua_State* L);
        static int      luaIntersect    (lua_State* L);
        static int      luaCombine      (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        MathLib::proj_t projection;
};

#endif  /* __spatial_index__ */
