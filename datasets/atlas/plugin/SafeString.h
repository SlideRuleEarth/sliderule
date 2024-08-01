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

#ifndef __safe_string__
#define __safe_string__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "List.h"

/******************************************************************************
 * SAFE STRING CLASS
 ******************************************************************************/

class SafeString
{
    public:

        static const long DEFAULT_STR_SIZE = 64;
        static const int MAX_REPLACEMENTS = 16;

                        SafeString  (long _maxlen=DEFAULT_STR_SIZE);
                        SafeString  (long _maxlen, const char* _str, ...) VARG_CHECK(printf, 3, 4);
        explicit        SafeString  (const char* _str);
                        SafeString  (const SafeString& other);
                        SafeString  (int base, unsigned char* buffer, int size);
                        ~SafeString (void);

        const char*     str         (bool duplicate = false);
        long            length      (void) const;
        long            bytes       (void) const;
        void            appendChar  (char c);
        int             findChar    (char c, int start=0);
        SafeString&     setChar     (char c, int index);
        bool            replace     (const char* oldtxt, const char* newtxt);
        bool            inreplace   (const char* oldtxt[], const char* newtxt[], int num_replacements);
        SafeString&     urlize      (void);
        List<string*>*  split       (char separator, bool strip=true);
        char            operator[]  (int index);
        SafeString&     operator+=  (const SafeString& rhs);
        SafeString&     operator+=  (const char* rstr);
        SafeString&     operator=   (const SafeString& rhs);
        SafeString&     operator=   (const char* rstr);
        void            reset       (void);

    private:

        char*   carray;
        long    len;
        long    maxlen;
};

#endif  /* __string_lib__ */
