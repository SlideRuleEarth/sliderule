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

#ifndef __gedi_parms__
#define __gedi_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "GeoJsonRaster.h"
#include "List.h"
#include "NetsvcParms.h"

/******************************************************************************
 * GEDI PARAMETERS
 ******************************************************************************/

class GediParms: public NetsvcParms
{
    public:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Beams */
        typedef enum {
            UNKNOWN_BEAM = -2,
            ALL_BEAMS = -1,
            BEAM0000 = 0,
            BEAM0001 = 1,
            BEAM0010 = 2,
            BEAM0011 = 3,
            BEAM0101 = 5,
            BEAM0110 = 6,
            BEAM1000 = 8,
            BEAM1011 = 11,
            NUM_BEAMS = 8
        } beam_t;

        /* Degrade Flag */
        typedef enum {
            DEGRADE_UNFILTERED = -1,
            DEGRADE_UNSET = 0,
            DEGRADE_SET = 1
        } degrade_t;

        /* L2 Quality Flag */
        typedef enum {
            L2QLTY_UNFILTERED = -1,
            L2QLTY_UNSET = 0,
            L2QLTY_SET = 1
        } l2_quality_t;

        /* L4 Quality Flag */
        typedef enum {
            L4QLTY_UNFILTERED = -1,
            L4QLTY_UNSET = 0,
            L4QLTY_SET = 1
        } l4_quality_t;

        /* Surface Flag */
        typedef enum {
            SURFACE_UNFILTERED = -1,
            SURFACE_UNSET = 0,
            SURFACE_SET = 1
        } surface_t;

        /* Flags */
        typedef enum {
            DEGRADE_FLAG_MASK    = 0x01,
            L2_QUALITY_FLAG_MASK = 0x02,
            L4_QUALITY_FLAG_MASK = 0x04,
            SURFACE_FLAG_MASK    = 0x80
        } flags_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* DEGRADE_FLAG;
        static const char* L2_QUALITY_FLAG;
        static const char* L4_QUALITY_FLAG;
        static const char* SURFACE_FLAG;
        static const char* BEAM;

        static const int64_t GEDI_SDP_EPOCH_GPS     = 1198800018; // seconds to add to GEDI delta times to get GPS times

        static const uint8_t BEAM_NUMBER[NUM_BEAMS];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);
        static const char*  beam2group          (int beam);
        static int          group2beam          (const char* group);
        static int          beam2index          (int beam);
        static const char*  index2group         (int index);
        static int64_t      deltatime2timestamp (double delta_time);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                    beams[NUM_BEAMS];
        degrade_t               degrade_filter;
        l2_quality_t            l2_quality_filter;
        l4_quality_t            l4_quality_filter;
        surface_t               surface_filter;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                GediParms               (lua_State* L, int index);
                                ~GediParms              (void);
        void                    cleanup                 (void);
        bool                    set_beam                (int beam);
        void                    get_lua_beams           (lua_State* L, int index, bool* provided);
};

#endif  /* __gedi_parms__ */
