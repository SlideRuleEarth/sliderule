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

#include <openssl/evp.h>

#include "EndpointObject.h"
#include "EventLib.h"
#include "OsApi.h"
#include "SecretManager.h"
#include "StringLib.h"
#include "SystemConfig.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EndpointObject::OBJECT_TYPE = "EndpointObject";

FString EndpointObject::serverHead("sliderule/%s", LIBID);

 /******************************************************************************
 * REQUEST SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointObject::Request::Request (const char* _id):
    path        (NULL),
    resource    (NULL),
    verb        (UNRECOGNIZED),
    version     (NULL),
    headers     (EXPECTED_MAX_HEADER_FIELDS),
    body        (NULL),
    length      (0),
    trace_id    (ORIGIN),
    id          (StringLib::duplicate(_id))
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EndpointObject::Request::~Request (void)
{
    delete [] body;
    delete [] resource;
    delete [] version;
    delete [] path;
    delete [] id;
}

/*----------------------------------------------------------------------------
 * setLuaTable
 *----------------------------------------------------------------------------*/
int EndpointObject::Request::setLuaTable(lua_State* L, const char* rqst_id, const char* rspq_name, const char* argument) const
{
    lua_newtable(L);
    LuaEngine::setAttrStr(L, "id", rqst_id);
    LuaEngine::setAttrStr(L, "rspq", rspq_name);
    LuaEngine::setAttrStr(L, "srcip", getHdrSourceIp());
    LuaEngine::setAttrStr(L, "orgroles", getHdrOrgRoles());
    LuaEngine::setAttrBool(L, "signed", verifyHdrSignature(getHdrAccount()));
    LuaEngine::setAttrStr(L, "arg", argument);
    lua_setglobal(L, "_rqst");
    return 1;
}

 /*----------------------------------------------------------------------------
 * getHdrSourceIp
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrSourceIp (void) const
{
    string* hdr_str;
    if(headers.find("x-forwarded-for", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "0.0.0.0";
}

/*----------------------------------------------------------------------------
 * getHdrClient
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrClient (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-client", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "unknown";
}

/*----------------------------------------------------------------------------
 * getHdrAccount
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrAccount (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-account", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "anonymous";
}

/*----------------------------------------------------------------------------
 * getHdrOrgRoles
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrOrgRoles (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-org-roles", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "[]";
}

/*----------------------------------------------------------------------------
 * getHdrStreaming
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrStreaming (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-streaming", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return NULL; // special default case
}

/*----------------------------------------------------------------------------
 * verifyHdrSignature
 *----------------------------------------------------------------------------*/
bool EndpointObject::Request::verifyHdrSignature (const char* account) const
{
#ifdef __aws__
    /* check signature */
    string* signature_str;
    if(!headers.find("x-sliderule-signature", &signature_str))
    {
        // no need to log message here as it is nominal for requests not to be signed
        return false;
    }

    /* get timestamp */
    string* timestamp_str;
    if(!headers.find("x-sliderule-timestamp", &timestamp_str))
    {
        mlog(CRITICAL, "Signed request did not inlude timestamp");
        return false;
    }

    /* verify timestamp */
    long long timestamp;
    if(StringLib::str2llong(timestamp_str->c_str(), &timestamp))
    {
        const int64_t allowed_range_seconds = SystemConfig::settings().signedRequestTimeWindow.value;
        const int64_t now = OsApi::time(OsApi::SYS_CLK) / 1000000; // seconds
        if((timestamp < (now - allowed_range_seconds)) || (timestamp > (now + allowed_range_seconds)))
        {
            mlog(CRITICAL, "Signed request expired: %lld <> %ld", timestamp, now);
            return false;
        }
    }

    /* get public key */
    const string openssh_public_key = SecretManager::get(SecretManager::PUBKEYS_SECRET, account);
    if(openssh_public_key.empty())
    {
        mlog(CRITICAL, "Failed to retrieve public key for %s", account);
        return false;
    }
    else if(!openssh_public_key.starts_with("ssh-ed25519"))
    {
        mlog(CRITICAL, "Failed to retrieve public key for %s", account);
        return false;
    }

    /* check public key format */
    const size_t first_space = openssh_public_key.find(' ');
    const size_t last_space  = openssh_public_key.rfind(' ');
    if((first_space == string::npos) || (last_space == string::npos) || (first_space >= last_space))
    {
        mlog(CRITICAL, "Public key requires three elements");
        return false;
    }

    /* parse (convert to) Ed25519 public key blob */
    const string openssh_public_key_blob_b64 = openssh_public_key.substr(first_space + 1, last_space - first_space - 1);
    const vector<uint8_t> openssh_public_key_blob = StringLib::b64decode(openssh_public_key_blob_b64);
    if(openssh_public_key_blob.size() < 51)
    {
        mlog(CRITICAL, "Failed to parse base64 encoded public key: %ld", openssh_public_key_blob.size());
        return false;
    }

    /*
     * parse (convert to) public key blob to get 32-byte public key
     *   uint32_t length | "ssh-ed25519" | uint32_t length | 32 byte public key
     */
    const uint32_t public_key_type_len = ((uint32_t)openssh_public_key_blob[0] << 24) | ((uint32_t)openssh_public_key_blob[1] << 16) | ((uint32_t)openssh_public_key_blob[2] << 8) | (uint32_t)openssh_public_key_blob[3];
    const string public_key_type(reinterpret_cast<const char*>(&openssh_public_key_blob[4]), 11);
    const uint32_t public_key_len = ((uint32_t)openssh_public_key_blob[15] << 24) | ((uint32_t)openssh_public_key_blob[16] << 16) | ((uint32_t)openssh_public_key_blob[17] << 8) | (uint32_t)openssh_public_key_blob[18];
    const uint8_t* public_key = &openssh_public_key_blob[19];
    if((public_key_type_len != 11) || (public_key_type != "ssh-ed25519") || (public_key_len != 32))
    {
        mlog(CRITICAL, "Invalid format detected in public key: %u %s %u", public_key_type_len, public_key_type.c_str(), public_key_len);
        return false;
    }

    /* build subdomain from cluster */
    const bool is_localhost = SystemConfig::settings().domain.value == "localhost";
    const FString subdomain("%s%s", is_localhost ? "" : SystemConfig::settings().cluster.value.c_str(), is_localhost ? "" : ".");

    /* build canonical message */
    const FString full_path("%s%s%s/%s", subdomain.c_str(), SystemConfig::settings().domain.value.c_str(), path, resource);
    const string full_path_b64 = StringLib::b64encode(full_path.c_str(), full_path.length(), false);
    const string body_b64 = StringLib::b64encode(body, length, false);
    const FString message("%s:%s:%s", full_path_b64.c_str(), timestamp_str->c_str(), body_b64.c_str());
    const uint8_t* message_bytes = reinterpret_cast<const uint8_t*>(message.c_str());

    /* create the EVP_PKEY from raw 32-byte public key */
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_key, 32);
    if(!pkey)
    {
        mlog(CRITICAL, "Failed to create EVP_PKEY from public key");
        return false;
    }

    /* create and verify context */
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if(!ctx)
    {
        EVP_PKEY_free(pkey);
        mlog(CRITICAL, "Failed to create EVP_MD_CTX");
        return false;
    }

    /* verify digest */
    if(EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        mlog(CRITICAL, "Failed to initialize digest");
        return false;
    }

    /* verify signature (Ed25519 uses no digest, hence nullptr for md) */
    const vector<uint8_t> signature = StringLib::b64decode(*signature_str);
    const int result = EVP_DigestVerify(ctx, signature.data(), signature.size(), message_bytes, message.length());
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    /* check verification */
    if(result != 1) // 1 = valid
    {
        mlog(CRITICAL, "Signed request failed signature verification: %d", result);
        return false;
    }
#endif
    return true;
}



/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointObject::EndpointObject (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table)
{
}

/*----------------------------------------------------------------------------
 * str2verb
 *----------------------------------------------------------------------------*/
EndpointObject::verb_t EndpointObject::str2verb (const char* str)
{
    if(StringLib::match(str, "GET"))       return GET;
    if(StringLib::match(str, "HEAD"))      return HEAD;
    if(StringLib::match(str, "POST"))      return POST;
    if(StringLib::match(str, "PUT"))       return PUT;
    if(StringLib::match(str, "DELETE"))    return DELETE;
    if(StringLib::match(str, "TRACE"))     return TRACE;
    if(StringLib::match(str, "OPTIONS"))   return OPTIONS;
    if(StringLib::match(str, "CONNECT"))   return CONNECT;
    if(StringLib::match(str, "RAW"))       return RAW;
    return UNRECOGNIZED;
}

/*----------------------------------------------------------------------------
 * verb2str
 *----------------------------------------------------------------------------*/
const char* EndpointObject::verb2str (verb_t verb)
{
    switch(verb)
    {
        case GET:       return "GET";
        case HEAD:      return "HEAD";
        case POST:      return "POST";
        case PUT:       return "PUT";
        case DELETE:    return "DELETE";
        case TRACE:     return "TRACE";
        case OPTIONS:   return "OPTIONS";
        case CONNECT:   return "CONNECT";
        case RAW:       return "";
        default:        return "UNRECOGNIZED";
    }
}

/*----------------------------------------------------------------------------
 * str2code
 *----------------------------------------------------------------------------*/
EndpointObject::code_t EndpointObject::str2code (const char* str)
{
    if(StringLib::match(str, "OK"))                        return OK;
    if(StringLib::match(str, "Bad Request"))               return Bad_Request;
    if(StringLib::match(str, "Not Found"))                 return Not_Found;
    if(StringLib::match(str, "Method Not Allowed"))        return Method_Not_Allowed;
    if(StringLib::match(str, "Request Timeout"))           return Request_Timeout;
    if(StringLib::match(str, "Method Not Implemented"))    return Method_Not_Implemented;
    return Bad_Request;
}

/*----------------------------------------------------------------------------
 * code2str
 *----------------------------------------------------------------------------*/
const char* EndpointObject::code2str (code_t code)
{
    switch(code)
    {
        case OK:                        return "OK";
        case Bad_Request:               return "Bad Request";
        case Not_Found:                 return "Not Found";
        case Method_Not_Allowed:        return "Method Not Allowed";
        case Request_Timeout:           return "Request Timeout";
        case Internal_Server_Error:     return "Internal Server Error";
        case Method_Not_Implemented:    return "Method Not Implemented";
        default:                        break;
    }
    return "Error";
}

/*----------------------------------------------------------------------------
 * buildheader
 *----------------------------------------------------------------------------*/
int EndpointObject::buildheader (char hdr_str[MAX_HDR_SIZE], code_t code, const char* content_type, int content_length, const char* transfer_encoding, const char* server)
{
    char str_buf[MAX_HDR_SIZE];

    StringLib::format(hdr_str, MAX_HDR_SIZE, "HTTP/1.1 %d %s\r\n", code, code2str(code));

    if(server)              StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Server: %s\r\n",               server),            MAX_HDR_SIZE);
    if(content_type)        StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Content-Type: %s\r\n",         content_type),      MAX_HDR_SIZE);
    if(content_length)      StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Content-Length: %d\r\n",       content_length),    MAX_HDR_SIZE);
    if(transfer_encoding)   StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Transfer-Encoding: %s\r\n",    transfer_encoding), MAX_HDR_SIZE);

    StringLib::concat(hdr_str, "\r\n",  MAX_HDR_SIZE);

    return StringLib::size(hdr_str);
}
