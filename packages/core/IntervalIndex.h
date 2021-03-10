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

#ifndef __interval_index__
#define __interval_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "AssetIndex.h"
#include "LuaObject.h"

/******************************************************************************
 * TIME INDEX CLASS
 ******************************************************************************/

typedef struct {
    double t0;  // start
    double t1;  // stop            
} intervalspan_t;

class IntervalIndex: public AssetIndex<intervalspan_t>
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        IntervalIndex   (lua_State* L, Asset* _asset, const char* _fieldname0, const char* _fieldname1, int _threshold);
                        ~IntervalIndex  (void);

        static int      luaCreate       (lua_State* L);

        void            split           (node_t* node, intervalspan_t& lspan, intervalspan_t& rspan) override;
        bool            isleft          (node_t* node, const intervalspan_t& span) override;
        bool            isright         (node_t* node, const intervalspan_t& span) override;
        bool            intersect       (const intervalspan_t& span1, const intervalspan_t& span2) override;
        intervalspan_t  combine         (const intervalspan_t& span1, const intervalspan_t& span2) override;
        intervalspan_t  attr2span       (Dictionary<double>* attr, bool* provided=NULL) override;
        intervalspan_t  luatable2span   (lua_State* L, int parm) override;
        void            displayspan     (const intervalspan_t& span) override;
    
    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char* fieldname0;
        const char* fieldname1;
};

#endif  /* __interval_index__ */
