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
 * INCLUDE
 ******************************************************************************/

#include "OsApi.h"
#include "AssetField.h"

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - AssetField
 *----------------------------------------------------------------------------*/
AssetField::AssetField(void):
    Field(ELEMENT, Field::STRING),
    asset(NULL)
{
}

/*----------------------------------------------------------------------------
 * Constructor - AssetField
 *----------------------------------------------------------------------------*/
AssetField::AssetField(const char* asset_name):
    Field(ELEMENT, Field::STRING),
    asset(NULL)
{
    // only initialize if provided
    if(asset_name)
    {
        // get asset
        asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE));

        // throw error on asset not found
        if(!asset) throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to find asset %s", asset_name);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
AssetField::~AssetField(void)
{
    if(asset) asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getName
 *----------------------------------------------------------------------------*/
const char* AssetField::getName (void) const
{
    if(asset)
    {
        return asset->getName();
    }
    return "<nil>";
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
string AssetField::toJson (void) const
{
    return FString("\"%s\"", getName()).c_str();
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
int AssetField::toLua (lua_State* L) const
{
    if(asset) lua_pushstring(L, asset->getName());
    else lua_pushnil(L);
    return 1;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void AssetField::fromLua (lua_State* L, int index)
{
    // get asset name (or throw if not provided)
    const char* asset_name = LuaObject::getLuaString(L, index);

     // if we successfully got a name then release asset if already set
    if(asset) asset->releaseLuaObject();
    asset = NULL;

    // get asset
    asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE));

    // throw error on asset not found
    if(!asset) throw RunTimeException(CRITICAL, RTE_FAILURE, "unable to find asset %s", asset_name);
}
