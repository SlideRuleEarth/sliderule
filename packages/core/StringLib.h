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

#ifndef __string_lib__
#define __string_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "List.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifndef MAX_STR_SIZE
#define MAX_STR_SIZE 1024
#endif

/******************************************************************************
 * STRING LIBRARY CLASS
 ******************************************************************************/

class StringLib
{
    public:

        /*--------------------------------------------------------------------
         * String (subclass)
         *--------------------------------------------------------------------*/

        class String
        {
            public:

                static const long DEFAULT_STR_SIZE = 64;

                                String      (long _maxlen=DEFAULT_STR_SIZE);
                                String      (const char* _str, ...) VARG_CHECK(printf, 2, 3);
                                String      (const String& other);
                                String      (int base, unsigned char* buffer, int size);
                                ~String     (void);

                const char*     getString   (bool duplicate = false);
                long            getLength   (void);
                void            appendChar  (char c);
                int             findChar    (char c, int start=0);
                String&         setChar     (char c, int index);
                bool            replace     (const char* oldtxt, const char* newtxt);
                String&         urlize      (void);
                List<String>*   split       (char separator, bool strip=true);
                char            operator[]  (int index);
                String&         operator+=  (const String& rhs);
                String&         operator+=  (const char* rstr);
                String&         operator=   (const String& rhs);
                String&         operator=   (const char* rstr);
                void            reset       (void);

            private:

                char*   str;
                long    len;
                long    maxlen;
        };

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef MgList<const char*, 256, true> TokenList;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static char*            duplicate       (const char* str, int size=MAX_STR_SIZE);
        static char*            concat          (const char* str1, const char* str2, const char* str3=NULL);
        static void             concat          (char* str1, const char* str2, int size);
        static char*            format          (char* dststr, int size, const char* _format, ...) VARG_CHECK(printf, 3, 4);
        static int              formats         (char* dststr, int size, const char* _format, ...) VARG_CHECK(printf, 3, 4);
        static char*            copy            (char* dst, const char* src, int _size);
        static char*            find            (const char* big, const char* little, int len=MAX_STR_SIZE);
        static char*            find            (const char* str, const char c, bool first=true);
        static int              size            (const char* str, int len=MAX_STR_SIZE);
        static bool             match           (const char* str1, const char* str2, int len=MAX_STR_SIZE);
        static TokenList*       split           (const char* str, int len, char separator, bool strip);
        static void             convertUpper    (char* str);
        static char*            convertUpper    (char* src, char* dst);
        static void             convertLower    (char* str);
        static char*            convertLower    (char* src, char* dst);
        static int              tokenizeLine    (const char* str, int str_size, char separator, int numtokens, char tokens[][MAX_STR_SIZE]);
        static int              getLine         (char* str, int* ret_len, int max_str_size, FILE* fptr);
        static bool             str2bool        (const char* str, bool* val);
        static bool             str2long        (const char* str, long* val, int base=0);
        static bool             str2ulong       (const char* str, unsigned long* val, int base=0);
        static bool             str2llong       (const char* str, long long* val, int base=0);
        static bool             str2ullong      (const char* str, unsigned long long* val, int base=0);
        static bool             str2double      (const char* str, double* val);
        static char*            checkNullStr    (const char* str);
        static char*            b64encode       (const void* data, int* size);
        static unsigned char*   b64decode       (const void* data, int* size);
        static int              printify        (char* buffer, int size);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* B64CHARS;
        static const int B64INDEX[256];
};

/*---------------------------------------------------------------------------
 * Syntax Sugar
 *----------------------------------------------------------------------------*/

typedef StringLib::String SafeString;
SafeString operator+ (SafeString lhs, const SafeString& rhs);

#endif  /* __string_lib__ */
