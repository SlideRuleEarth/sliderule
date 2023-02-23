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
#include <stdarg.h>

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RunTimeException::RunTimeException(event_level_t _lvl, int _rc, const char* _errmsg, ...):
    std::runtime_error("RunTimeException"),
    lvl(_lvl),
    rc(_rc)
{
    errmsg[0] = '\0';

    va_list args;
    va_start(args, _errmsg);
    int vlen = vsnprintf(errmsg, ERROR_MSG_LEN - 1, _errmsg, args);
    int msglen = MIN(vlen, ERROR_MSG_LEN - 1);
    va_end(args);

    if (msglen >= 0) errmsg[msglen] = '\0';
}

/*----------------------------------------------------------------------------
 * what
 *----------------------------------------------------------------------------*/
char const* RunTimeException::what() const throw()
{
    return errmsg;
};

/*----------------------------------------------------------------------------
 * level
 *----------------------------------------------------------------------------*/
event_level_t RunTimeException::level(void) const
{
    return lvl;
};

/*----------------------------------------------------------------------------
 * code
 *----------------------------------------------------------------------------*/
int RunTimeException::code (void) const
{
    return rc;
}
