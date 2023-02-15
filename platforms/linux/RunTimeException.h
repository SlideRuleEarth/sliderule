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

#ifndef __runtime_exception__
#define __runtime_exception__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <stdexcept>

/******************************************************************************
 * EXCEPTION
 ******************************************************************************/

#define RTE_INFO                        -1
#define RTE_ERROR                       0
#define RTE_TIMEOUT                     1
#define RTE_RESOURCE_DOES_NOT_EXIST     2
#define RTE_EMPTY_SUBSET                3

/******************************************************************************
 * EXCEPTION
 ******************************************************************************/

class RunTimeException : public std::runtime_error
{
    public:

        RunTimeException(event_level_t _lvl, int _rc, const char* _errmsg, ...) VARG_CHECK(printf, 4, 5);
        char const* what() const throw();
        event_level_t level (void) const;
        int code (void) const;

    private:

        static const int ERROR_MSG_LEN = 128;
        char errmsg[ERROR_MSG_LEN];
        const event_level_t lvl;
        const int rc;
};

#endif // __runtime_exception__