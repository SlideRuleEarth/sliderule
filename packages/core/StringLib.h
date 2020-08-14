/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __string_lib__
#define __string_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "List.h"

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
                                ~String     (void);

                const char*     getString   (bool duplicate = false);
                long            getLength   (void);
                void            appendChar  (char c);
                int             findChar    (char c, int start=0);
                bool            setChar     (char c, int index);
                bool            replace     (const char* oldtxt, const char* newtxt);
                List<String>*   split       (char separator, bool strip=true);
                char            operator[]  (int index);
                String&         operator+=  (const String& rhs);
                String&         operator+=  (const char* rstr);
                String&         operator=   (const String& rhs);
                String&         operator=   (const char* rstr);

            private:

                char*   str;
                long    len;
                long    maxlen;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static char*    duplicate       (const char* str);
        static char*    concat          (const char* str1, const char* str2, const char* str3=NULL);
        static void     concat          (char* str1, const char* str2, int size);
        static char*    format          (char* dststr, int size, const char* _format, ...) VARG_CHECK(printf, 3, 4);
        static char*    copy            (char* dst, const char* src, int _size);
        static char*    find            (const char* big, const char* little, int len=MAX_STR_SIZE);
        static char*    find            (const char* str, const char c, bool first=true);
        static int      size            (const char* str, int len=MAX_STR_SIZE);
        static bool     match           (const char* str1, const char* str2, int len=MAX_STR_SIZE);
        static void     convertUpper    (char* str);
        static char*    convertUpper    (char* src, char* dst);
        static int      tokenizeLine    (const char* str, int str_size, char separator, int numtokens, char tokens[][MAX_STR_SIZE]);
        static int      getLine         (char* str, int* ret_len, int max_str_size, FILE* fptr);
        static bool     str2bool        (const char* str, bool* val);
        static bool     str2long        (const char* str, long* val, int base=0);
        static bool     str2ulong       (const char* str, unsigned long* val, int base=0);
        static bool     str2llong       (const char* str, long long* val, int base=0);
        static bool     str2ullong      (const char* str, unsigned long long* val, int base=0);
        static bool     str2double      (const char* str, double* val);
        static char*    checkNullStr    (const char* str);
};

/*---------------------------------------------------------------------------
 * Syntax Sugar
 *----------------------------------------------------------------------------*/

typedef StringLib::String SafeString;
SafeString operator+ (SafeString lhs, const SafeString& rhs);

#endif  /* __string_lib__ */
