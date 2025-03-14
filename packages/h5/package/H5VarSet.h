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

#ifndef __h5_variable_set__
#define __h5_variable_set__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Asset.h"
#include "RecordObject.h"
#include "Dictionary.h"
#include "FieldList.h"
#include "GeoDataFrame.h"
#include "H5DArray.h"
#include "H5Coro.h"

/******************************************************************************
 * H5 VARIABLE SET
 ******************************************************************************/

class H5VarSet
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/
                    H5VarSet            (const FieldList<string>& variable_list, H5Coro::Context* context, const char* group=NULL, long col=0, long startrow=0, long numrows=H5Coro::ALL_ROWS);
        virtual     ~H5VarSet           (void) = default;
        long        length              (void) const { return variables.length(); }
        void        joinToGDF           (GeoDataFrame* gdf, int timeout_ms, bool throw_exception=true);
        void        addToGDF            (GeoDataFrame* gdf, long element) const;
        static int  getDictSize         (int list_size) { return list_size * 2 + 1; };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Dictionary<H5DArray*> variables;
};

#endif  /* __h5_variable_set__ */
