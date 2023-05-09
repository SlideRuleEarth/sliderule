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
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "core.h"
#include "RasterObject.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RasterObject::OBJECT_TYPE  = "RasterObject";
const char* RasterObject::LuaMetaName  = "RasterObject";
const struct luaL_Reg RasterObject::LuaMetaTable[] = {
    {NULL,          NULL}
};

Mutex RasterObject::factoryMut;
Dictionary<RasterObject::factory_t> RasterObject::factories;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void RasterObject::init( void )
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void RasterObject::deinit( void )
{
}

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int RasterObject::luaCreate( lua_State* L )
{
    GeoParms* _parms = NULL;
    try
    {
        /* Get Parameters */
        _parms = (GeoParms*)getLuaObject(L, 1, GeoParms::OBJECT_TYPE);

        /* Get Factory */
        factory_t _create = NULL;
        factoryMut.lock();
        {
            factories.find(_parms->asset_name, &_create);
        }
        factoryMut.unlock();

        /* Check Factory */
        if(_create == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to find registered raster for %s", _parms->asset_name);

        /* Create Raster */
        RasterObject* _raster = _create(L, _parms);
        if(_raster == NULL) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create raster of type: %s", _parms->asset_name);

        /* Return Object */
        return createLuaObject(L, _raster);
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * registerDriver
 *----------------------------------------------------------------------------*/
bool RasterObject::registerRaster (const char* _name, factory_t create)
{
    bool status;

    factoryMut.lock();
    {
        status = factories.add(_name, create);
    }
    factoryMut.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RasterObject::~RasterObject(void)
{
    /* Release GeoParms LuaObject */
    parms->releaseLuaObject();
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RasterObject::RasterObject(lua_State *L, GeoParms* _parms):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    parms(_parms)
{
    /* Establish Credentials */
    #ifdef __aws__
    if(_parms->asset)
    {
        const char* identity = _parms->asset->getIdentity();
        CredentialStore::Credential credentials = CredentialStore::get(identity);
        if(credentials.provided)
        {
            const char* path = _parms->asset->getPath();
            VSISetPathSpecificOption(path, "AWS_ACCESS_KEY_ID", credentials.accessKeyId);
            VSISetPathSpecificOption(path, "AWS_SECRET_ACCESS_KEY", credentials.secretAccessKey);
            VSISetPathSpecificOption(path, "AWS_SESSION_TOKEN", credentials.sessionToken);
        }
    }
    #endif
}

/*----------------------------------------------------------------------------
 * fileDictAdd
 *----------------------------------------------------------------------------*/
uint64_t RasterObject::fileDictAdd(const std::string& fileName)
{
    uint64_t id;

    if(!fileDict.find(fileName.c_str(), &id))
    {
        id = (parms->key_space << 32) | fileDict.length();
        fileDict.add(fileName.c_str(), id);
    }

    return id;
}
