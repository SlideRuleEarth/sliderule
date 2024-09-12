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

#ifndef __region_mask__
#define __region_mask__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "FieldDictionary.h"
#include "FieldElement.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class RegionMask: public FieldDictionary
{
    public:
    
        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int PIXEL_ON   = 1;
        static const int PIXEL_OFF  = 0;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef bool (*burn_func_t) (RegionMask& image);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void registerRasterizer  (burn_func_t func);

                    RegionMask          (void);
                    ~RegionMask         (void) override;

        RegionMask& operator=           (const RegionMask v);
        bool        operator==          (const RegionMask& v) const;
        bool        operator!=          (const RegionMask& v) const;

        bool        includes            (double lon, double lat) const;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static burn_func_t      burnMask;

        FieldElement<string>    geojson{""};
        FieldElement<double>    cellSize{0.0};
        FieldElement<uint32_t>  cols{0};
        FieldElement<uint32_t>  rows{0};
        FieldElement<double>    lonMin{0.0};
        FieldElement<double>    latMin{0.0};
        FieldElement<double>    lonMax{0.0};
        FieldElement<double>    latMax{0.0};    
        
        uint8_t*                data{NULL};
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

int convertToLua(lua_State* L, const RegionMask& v);
void convertFromLua(lua_State* L, int index, RegionMask& v);


#endif  /* __region_mask__ */
