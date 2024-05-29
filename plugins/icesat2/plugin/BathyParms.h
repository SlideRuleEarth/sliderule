/*
 * Copyright (c) 2023, University of Texas
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
 * 3. Neither the name of the University of Texas nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __bathy_parms__
#define __bathy_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * REQUEST PARAMETERS
 ******************************************************************************/

class BathyParms: public Icesat2Parms
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* MAX_ALONG_TRACK_SPREAD;
        static const char* MAX_DEM_DELTA;
        static const char* PH_IN_EXTENT;
        static const char* GENERATE_NDWI;
        static const char* USE_BATHY_MASK;
        static const char* RETURN_INPUTS;
        static const char* CLASSIFIERS;
        static const char* SPOTS;
        static const char* ATL09_RESOURCES;

        static const int ATL09_RESOURCE_NAME_LEN = 39;
        static const int ATL09_RESOURCE_KEY_LEN = 6;

        static const double DEFAULT_MAX_ALONG_TRACK_SPREAD;
        static const double DEFAULT_MAX_DEM_DELTA;
        static const int DEFAULT_PH_IN_EXTENT = 8192;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            INVALID_CLASSIFIER  = -1,
            COASTNET            = 0,
            OPENOCEANS          = 1,
            MEDIANFILTER        = 2,
            CSHELPH             = 3,
            BATHY_PATHFINDER    = 4,
            POINTNET2           = 5,
            LOCAL_CONTRAST      = 6,
            ENSEMBLE            = 7,
            NUM_CLASSIFIERS     = 8
        } classifier_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate               (lua_State* L);
        static void         getATL09Key             (char* key, const char* name);
        static int          luaSpotEnabled          (lua_State* L);
        static int          luaClassifierEnabled    (lua_State* L);
        static classifier_t str2classifier          (const char* str);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        double      max_along_track_spread;
        double      max_dem_delta;
        int         ph_in_extent;
        bool        generate_ndwi;
        bool        use_bathy_mask;
        bool        classifiers[NUM_CLASSIFIERS];
        bool        return_inputs; // return the atl03 bathy records back to client
        bool        spots[NUM_SPOTS]; // only used by downstream algorithms
        Dictionary<string> alt09_index;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    BathyParms      (lua_State* L, int index);
                    ~BathyParms     (void) override;

        void        cleanup         (void) const;
        void        get_atl09_list  (lua_State* L, int index, bool* provided);
        void        get_spot_list   (lua_State* L, int index, bool* provided);
        void        get_classifiers (lua_State* L, int index, bool* provided);
};

#endif  /* __bathy_parms__ */
