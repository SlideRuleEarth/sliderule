/*
 * Copyright (c) 2025, University of Washington
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

#ifndef __gedi_dataframe__
#define __gedi_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "GeoDataFrame.h"
#include "H5Object.h"
#include "H5Array.h"
#include "H5Coro.h"
#include "H5VarSet.h"
#include "OsApi.h"
#include "StringLib.h"
#include "AreaOfInterest.h"
#include "GediFields.h"

using AreaOfInterestGedi = AreaOfInterestT<double>;

/******************************************************************************
 * GEDI DATAFRAME BASE
 ******************************************************************************/

class GediDataFrame: public GeoDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GediDataFrame  (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[],
                        const std::initializer_list<FieldMap<FieldUntypedColumn>::init_entry_t>& column_list,
                        GediFields* _parms, H5Object* _hdf, const char* beam_str, const char* outq_name);

        ~GediDataFrame (void) override;
        okey_t getKey  (void) const override;

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<uint8_t>  beam;
        FieldElement<uint32_t> orbit;
        FieldElement<uint16_t> track;
        FieldElement<string>   granule;

        std::atomic<bool> active;
        Thread*           readerPid;
        int               readTimeoutMs;
        Publisher*        outQ;
        GediFields*       parms;
        H5Object*         hdf;
        okey_t            dfKey;
        const char*       beamStr;
        char              group[9];

        private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static const char* getCRS (void);
};

#endif  /* __gedi_dataframe__ */
