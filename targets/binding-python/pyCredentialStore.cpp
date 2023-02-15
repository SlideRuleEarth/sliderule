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

#include <pybind11/pybind11.h>
#include "StringLib.h"
#include "TimeLib.h"
#include "CredentialStore.h"
#include "pyCredentialStore.h"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * pyCredentialStore Class
 ******************************************************************************/

/*--------------------------------------------------------------------
 * Constructor
 *--------------------------------------------------------------------*/
pyCredentialStore::pyCredentialStore (const std::string &_asset)
{
    asset = StringLib::duplicate(_asset.c_str());
}

/*--------------------------------------------------------------------
 * Destructor
 *--------------------------------------------------------------------*/
pyCredentialStore::~pyCredentialStore (void)
{
    if(asset) delete [] asset;
}

/*--------------------------------------------------------------------
 * provide
 *--------------------------------------------------------------------*/
bool pyCredentialStore::provide (const py::dict& credentials)
{
    PyObject* accessKeyId = PyUnicode_AsEncodedString(PyObject_Repr(PyDict_GetItem(credentials.ptr(), py::str("accessKeyId").ptr())), "utf-8", "~E~");
    PyObject* secretAccessKey = PyUnicode_AsEncodedString(PyObject_Repr(PyDict_GetItem(credentials.ptr(), py::str("secretAccessKey").ptr())), "utf-8", "~E~");
    PyObject* sessionToken = PyUnicode_AsEncodedString(PyObject_Repr(PyDict_GetItem(credentials.ptr(), py::str("sessionToken").ptr())), "utf-8", "~E~");
    PyObject* expiration = PyUnicode_AsEncodedString(PyObject_Repr(PyDict_GetItem(credentials.ptr(), py::str("expiration").ptr())), "utf-8", "~E~");

    SafeString _accessKeyId("%s", PyBytes_AS_STRING(accessKeyId));
    SafeString _secretAccessKey("%s", PyBytes_AS_STRING(secretAccessKey));
    SafeString _sessionToken("%s", PyBytes_AS_STRING(sessionToken));
    SafeString _expiration("%s", PyBytes_AS_STRING(expiration));

    _accessKeyId.replace("'", "");
    _secretAccessKey.replace("'", "");
    _sessionToken.replace("'", "");
    _expiration.replace("'", "");

    CredentialStore::Credential credential;
    credential.provided = true;
    credential.accessKeyId = _accessKeyId.str(true);
    credential.secretAccessKey = _secretAccessKey.str(true);
    credential.sessionToken = _sessionToken.str(true);
    credential.expiration = _expiration.str(true);
    credential.expirationGps = TimeLib::str2gpstime(credential.expiration);

    return CredentialStore::put(asset, credential);
}

/*--------------------------------------------------------------------
 * provide
 *--------------------------------------------------------------------*/
py::dict pyCredentialStore::retrieve (void)
{
    py::dict results;

    CredentialStore::Credential credential = CredentialStore::get(asset);

    if(credential.provided)
    {
        results[py::str("accessKeyId")] = credential.accessKeyId;
        results[py::str("secretAccessKey")] = credential.secretAccessKey;
        results[py::str("sessionToken")] = credential.sessionToken;
        results[py::str("expiration")] = credential.expiration;
    }

    return results;
}