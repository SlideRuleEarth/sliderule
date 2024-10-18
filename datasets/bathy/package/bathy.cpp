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

/******************************************************************************
 *INCLUDES
 ******************************************************************************/

#include "bathy.h"
#include "BathyViewer.h"
#include "BathyDataFrame.h"
#include "BathyFields.h"
#include "BathyMask.h"
#include "BathyGranule.h"
#include "BathyKd.h"
#include "BathyCoastnetClassifier.h"
#include "BathyQtreesClassifier.h"
#include "BathyOpenOceansPPClassifier.h"
#include "BathySeaSurfaceFinder.h"
#include "BathyRefractionCorrector.h"
#include "BathyUncertaintyCalculator.h"
#ifdef __unittesting__
#include "UT_RiWater.h"
#endif

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_BATHY_LIBNAME    "bathy"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * bathy_open
 *----------------------------------------------------------------------------*/
int bathy_open (lua_State *L)
{
    static const struct luaL_Reg bathy_functions[] = {
        {"parms",               BathyFields::luaCreate},
        {"dataframe",           BathyDataFrame::luaCreate},
        {"mask",                BathyMask::luaCreate},
        {"kd",                  BathyKd::luaCreate},
        {"granule",             BathyGranule::luaCreate},
        {"viewer",              BathyViewer::luaCreate},
        {"coastnet",            BathyCoastnetClassifier::luaCreate},
        {"qtrees",              BathyQtreesClassifier::luaCreate},
        {"openoceanspp",        BathyOpenOceansPPClassifier::luaCreate},
        {"seasurface",          BathySeaSurfaceFinder::luaCreate},
        {"refraction",          BathyRefractionCorrector::luaCreate},
        {"uncertainty",         BathyUncertaintyCalculator::luaCreate},
        {"inituncertainty",     BathyUncertaintyCalculator::luaInit},
#ifdef __unittesting__
        {"ut_riwater",          UT_RiWater::luaCreate},
#endif
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, bathy_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "QTREES",          BathyFields::QTREES);
    LuaEngine::setAttrInt(L, "COASTNET",        BathyFields::COASTNET);
    LuaEngine::setAttrInt(L, "OPENOCEANSPP",    BathyFields::OPENOCEANSPP);
    LuaEngine::setAttrInt(L, "MEDIANFILTER",    BathyFields::MEDIANFILTER);
    LuaEngine::setAttrInt(L, "CSHELPH",         BathyFields::CSHELPH);
    LuaEngine::setAttrInt(L, "BATHYPATHFINDER", BathyFields::BATHYPATHFINDER);
    LuaEngine::setAttrInt(L, "POINTNET",        BathyFields::POINTNET);
    LuaEngine::setAttrInt(L, "OPENOCEANS",      BathyFields::OPENOCEANS);
    LuaEngine::setAttrInt(L, "ENSEMBLE",        BathyFields::ENSEMBLE);
    LuaEngine::setAttrStr(L, "COASTNET_MODEL",  COASTNET_MODEL);
    LuaEngine::setAttrStr(L, "QTREES_MODEL",    QTREES_MODEL);
    LuaEngine::setAttrStr(L, "ENSEMBLE_MODEL",  ENSEMBLE_MODEL);
    LuaEngine::setAttrStr(L, "POINTNET_MODEL",  POINTNET_MODEL);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initbathy (void)
{
    /* Initialize Modules */
    BathyViewer::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_BATHY_LIBNAME, bathy_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_BATHY_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_BATHY_LIBNAME, LIBID);
}

void deinitbathy (void)
{
}
}
