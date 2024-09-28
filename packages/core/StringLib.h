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
 * STRING LIBRARY CLASS
 ******************************************************************************/
class StringLib
{
    public:

        class FormattedString
        {
            public:

                explicit FormattedString (const char* _str, ...) VARG_CHECK(printf, 2, 3);
                ~FormattedString (void);

                const char*     c_str       (bool duplicate = false) const;
                long            length      (void) const;
                long            size        (void) const;

            private:

                char*   carray;
                long    bufsize;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static char*            duplicate       (const char* str);
        static char*            concat          (const char* str1, const char* str2, const char* str3=NULL);
        static void             concat          (char* str1, const char* str2, int size);
        static char*            format          (char* dststr, int size, const char* _format, ...) VARG_CHECK(printf, 3, 4);
        static int              formats         (char* dststr, int size, const char* _format, ...) VARG_CHECK(printf, 3, 4);
        static char*            copy            (char* dst, const char* src, int _size);
        static char*            find            (const char* big, const char* little);
        static char*            find            (const char* str, char c, bool first=true);
        static int              size            (const char* str);
        static int              nsize           (const char* str, int size);
        static bool             match           (const char* str1, const char* str2);
        static List<string*>*   split           (const char* str, int len, char separator, bool strip=true);
        static void             convertUpper    (char* str);
        static char*            convertUpper    (char* dst, const char* src);
        static void             convertLower    (char* str);
        static char*            convertLower    (char* dst, const char* src);
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
        static char*            b16encode       (const void* data, int size, bool lower_case, char* dst=NULL);
        static int              printify        (char* buffer, int size);
        static int              replace         (char* str, char oldchar, char newchar);
        static char*            replace         (const char* str, const char* oldtxt, const char* newtxt);
        static char*            replace         (const char* str, const char* oldtxt[], const char* newtxt[], int num_replacements);
        static char*            urlize          (const char* str);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_NUM_REPLACEMENTS = 16;
        static const char* B64CHARS;
        static const int B64INDEX[256];
};

/*---------------------------------------------------------------------------
 * Syntax Sugar
 *----------------------------------------------------------------------------*/

typedef StringLib::FormattedString FString;

#endif  /* __string_lib__ */
