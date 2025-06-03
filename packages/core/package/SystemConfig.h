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

#ifndef __system_config__
#define __system_config__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "FieldDictionary.h"
#include "FieldElement.h"
#include "FieldList.h"

/******************************************************************************
 * SINGLETON CLASS
 ******************************************************************************/

class SystemConfig: public FieldDictionary
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* IPV4_ENV;
        static const char* ENVIRONMENT_VERSION_ENV;
        static const char* ORCHESTRATOR_URL_ENV;
        static const char* ORGANIZATION_ENV;
        static const char* PROV_SYS_URL_ENV;
        static const char* MANAGER_URL_ENV;
        static const char* CONTAINER_REGISTRY_ENV;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            TEXT,
            CLOUD,
        } event_format_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaPopulate (lua_State* L);
        static int luaGetField (lua_State* L);
        static int luaSetField (lua_State* L);

        static SystemConfig& settings(void) {
            static SystemConfig instance;
            return instance;
        }

        // Delete copy and move semantics
        SystemConfig(const SystemConfig&) = delete;
        SystemConfig& operator=(const SystemConfig&) = delete;
        SystemConfig(SystemConfig&&) = delete;
        SystemConfig& operator=(SystemConfig&&) = delete;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<event_format_t>    logFormat                   {TEXT};
        FieldElement<event_level_t>     logLevel                    {INFO};
        FieldElement<event_level_t>     traceLevel                  {INFO};
        FieldElement<event_level_t>     telemetryLevel              {INFO};
        FieldElement<event_level_t>     alertLevel                  {INFO};
        FieldElement<int>               appPort                     {9081};
        FieldElement<bool>              authenticateToNSIDC         {true};
        FieldElement<bool>              authenticateToORNLDAAC      {true};
        FieldElement<bool>              authenticateToLPDAAC        {true};
        FieldElement<bool>              authenticateToPODAAC        {true};
        FieldElement<bool>              authenticateToASF           {true};
        FieldElement<bool>              registerAsService           {true};
        FieldElement<string>            assetDirectory              {"asset_directory.csv"};
        FieldElement<float>             normalMemoryThreshold       {1.0};
        FieldElement<float>             streamMemoryThreshold       {0.75};
        FieldElement<int>               msgQDepth                   {10000};
        FieldElement<bool>              authenticateToProvSys       {false};
        FieldElement<bool>              isPublic                    {false};
        FieldElement<bool>              inCloud                     {false};
        FieldElement<string>            systemBucket                {"sliderule"};
        FieldList<string>               postStartupScripts;

        // ENVIRONMENT VARIABLES
        FieldElement<string>            ipv4                        {"127.0.0.1"};
        FieldElement<string>            environmentVersion          {"unknown"};
        FieldElement<string>            orchestratorURL             {"http://127.0.0.1:8050"};
        FieldElement<string>            organization                {"localhost"};
        FieldElement<string>            provSysURL                  {"https://ps.localhost"};
        FieldElement<string>            managerURL                  {"http://127.0.0.1:8000"};
        FieldElement<string>            containerRegistry           {"742127912612.dkr.ecr.us-west-2.amazonaws.com"};

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        SystemConfig ();
        virtual ~SystemConfig  (void) override;
        static void setIfProvided(FieldElement<string>& field, const char* env);
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const SystemConfig::event_format_t& v);
int convertToLua(lua_State* L, const SystemConfig::event_format_t& v);
void convertFromLua(lua_State* L, int index, SystemConfig::event_format_t& v);

string convertToJson(const event_level_t& v);
int convertToLua(lua_State* L, const event_level_t& v);
void convertFromLua(lua_State* L, int index, event_level_t& v);

inline uint32_t toEncoding(SystemConfig::event_format_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(event_level_t& v) { (void)v; return Field::INT32; }

#endif  /* __system_config__ */
