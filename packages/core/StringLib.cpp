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

#include "StringLib.h"
#include "OsApi.h"

#include <cstdarg>
#include <string.h>
#include <limits.h>

/******************************************************************************
 * STRING STATIC DATA
 ******************************************************************************/

const char* StringLib::B64CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const int StringLib::B64INDEX[256] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63,
    0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

/******************************************************************************
 * STRING PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SafeString::String(long _maxlen)
{
    if(_maxlen <= 0)    maxlen = DEFAULT_STR_SIZE;
    else                maxlen = _maxlen;
    str = new char[maxlen];
    LocalLib::set(str, 0, maxlen);
    len = 1;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SafeString::String(const char* _str, ...)
{
    if(_str != NULL)
    {
        va_list args;
        va_start(args, _str);
        len = vsnprintf(NULL, 0, _str, args) + 1; // get length
        va_end(args);
        str = new char[len]; // allocate memory
        maxlen = len;
        va_start(args, _str);
        vsprintf(str, _str, args); // copy in formatted contents
        va_end(args);
        str[maxlen - 1] ='\0'; // null terminate
    }
    else
    {
        maxlen = DEFAULT_STR_SIZE;
        str = new char[maxlen];
        LocalLib::set(str, 0, maxlen);
        len = 1;
    }
}

/*----------------------------------------------------------------------------
 * Constructor (COPY)
 *----------------------------------------------------------------------------*/
SafeString::String(const SafeString& other)
{
    maxlen = other.maxlen;
    str = new char[maxlen];
    LocalLib::set(str, 0, maxlen);
    len = other.len;
    StringLib::copy(str, other.str, len);
}

/*----------------------------------------------------------------------------
 * Constructor (ENCODE)
 *----------------------------------------------------------------------------*/
SafeString::String (int base, unsigned char* buffer, int size)
{
    if(base == 64)
    {
        int encoded_len = size;
        str = StringLib::b64encode(buffer, &encoded_len);
        maxlen = encoded_len;
        len = encoded_len;
    }
    else
    {
        maxlen = DEFAULT_STR_SIZE;
        str = new char[maxlen];
        LocalLib::set(str, 0, maxlen);
        len = 1;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SafeString::~String(void)
{
    delete [] str;
}

/*----------------------------------------------------------------------------
 * getString
 *----------------------------------------------------------------------------*/
const char* SafeString::getString(bool duplicate)
{
    if(duplicate)
    {
        char* new_str = new char[len];
        StringLib::copy(new_str, str, len);
        return new_str;
    }
    else
    {
        return str;
    }
}

/*----------------------------------------------------------------------------
 * getLength
 *----------------------------------------------------------------------------*/
long SafeString::getLength(void)
{
    return len;
}

/*----------------------------------------------------------------------------
 * appendChar
 *----------------------------------------------------------------------------*/
void SafeString::appendChar(char c)
{
    if(len >= maxlen)
    {
        maxlen *= 2; // optimization
        char* new_str = new char[maxlen];
        LocalLib::set(new_str, 0, maxlen);
        StringLib::copy(new_str, str, len);
        delete [] str;
        str = new_str;
    }

    str[len-1] = c;
    str[len] = '\0';
    len++;
}

/*----------------------------------------------------------------------------
 * findChar
 *----------------------------------------------------------------------------*/
int SafeString::findChar (char c, int start)
{
    if(start >= 0)
    {
        for(int i = start; i < len; i++)
        {
            if(str[i] == c)
            {
                return i;
            }
        }
    }

    return -1;
}

/*----------------------------------------------------------------------------
 * setChar
 *----------------------------------------------------------------------------*/
SafeString& SafeString::setChar (char c, int index)
{
    if(index >= 0 && index < len)
    {
        str[index] = c;
    }
    return *this;
}

/*----------------------------------------------------------------------------
 * replace
 *
 *  replaces all occurrences
 *----------------------------------------------------------------------------*/
bool SafeString::replace(const char* oldtxt, const char* newtxt)
{
    bool status = false;

    int newtxtlen = (int)strlen(newtxt);
    char* startptr = strstr(str, oldtxt);
    char* endptr = startptr + strlen(oldtxt);

    while(startptr != NULL)
    {
        status = true;

        maxlen += newtxtlen;
        char* newstr = new char[maxlen];
        LocalLib::set(newstr, 0, maxlen);

        LocalLib::copy(newstr, str, (startptr - str));
        LocalLib::copy(newstr + (startptr - str), newtxt, newtxtlen);
        LocalLib::copy(newstr + (startptr - str) + newtxtlen, endptr, len - (endptr - str));

        delete [] str;
        str = newstr;

        len = (int)strlen(str) + 1;

        startptr = strstr(str, oldtxt);
        endptr = startptr + strlen(oldtxt);
    }

    return status;
}

/*----------------------------------------------------------------------------
 * inreplace
 *
 *  replaces all occurrences in place
 *----------------------------------------------------------------------------*/
bool SafeString::inreplace (const char* oldtxt[], const char* newtxt[], int num_replacements)
{
    /* Check Number of Replacements */
    if(num_replacements > MAX_REPLACEMENTS)
    {
        return false;
    }

    /* Get Delta Sizes for each Replacement */
    int replacement_size_delta[MAX_REPLACEMENTS];
    for(int r = 0; r < num_replacements; r++)
    {
        replacement_size_delta[r] = strlen(newtxt[r]) - strlen(oldtxt[r]);
    }

    /* Count Number of Replacements */
    int replacement_count[MAX_REPLACEMENTS];
    for(int r = 0; r < num_replacements; r++)
    {
        replacement_count[r] = 0;
        int i = 0;
        while(i < (len - 1))
        {
            int j = i, k = 0;
            while((j < (len - 1)) && oldtxt[r][k] && (str[j] == oldtxt[r][k]))
            {
                j++;
                k++;
            }

            if(oldtxt[r][k] == '\0')
            {
                replacement_count[r]++;
                i = j;
            }
            else
            {
                i++;
            }
        }
    }

    /* Calculate Size of New String */
    int total_size_delta = 0;
    for(int r = 0; r < num_replacements; r++)
    {
        total_size_delta += replacement_size_delta[r] * replacement_count[r];
    }

    /* Set New Size */
    int new_size = len + total_size_delta;
    if(new_size > 0)
    {
        len = new_size;
        maxlen = new_size;
    }
    else
    {
        len = 1;
        maxlen = 1;
        str[0] = '\0';
        return true;
    }

    /* Allocate New String */
    char* newstr = new char [maxlen];

    /* Populate New String */
    int orig_i = 0, new_i = 0;
    while(str[orig_i])
    {
        /* For Each Possible Replacement */
        bool replacement = false;
        for(int r = 0; r < num_replacements; r++)
        {
            /* Check for Match */
            int j = orig_i, k = 0;
            while(str[j] && oldtxt[r][k] && (str[j] == oldtxt[r][k]))
            {
                j++;
                k++;
            }

            /* Replace Matched Text */
            if(oldtxt[r][k] == '\0')
            {
                int i = 0;
                while(newtxt[r][i])
                {
                    newstr[new_i++] = newtxt[r][i++];
                }
                orig_i = j;
                replacement = true;
                break;
            }
        }

        /* Copy Over and Advance String Indices */
        if(!replacement)
        {
            newstr[new_i++] = str[orig_i++];
        }
    }

    /* Terminate New String and Replace Existing String */
    newstr[new_i] = '\0';
    delete [] str;
    str = newstr;

    return true;
}

/*----------------------------------------------------------------------------
 * urlize
 *
 * ! 	    # 	    $ 	    & 	    ' 	    ( 	    ) 	    * 	    +
 * %21 	    %23 	%24 	%26 	%27 	%28 	%29 	%2A 	%2B
 *
 * ,        / 	    : 	    ; 	    = 	    ? 	    @ 	    [ 	    ]
 * %2C      %2F 	%3A 	%3B 	%3D 	%3F 	%40 	%5B 	%5D
 *----------------------------------------------------------------------------*/
SafeString& SafeString::urlize(void)
{
    /* Allocate enough room to hold urlized string */
    if(maxlen < (len * 3))
    {
        maxlen = len * 3;
    }

    /* Setup pointers to new and old strings */
    char* alloc_str = new char [maxlen];
    char* new_str = alloc_str;
    char* old_str = str;

    /* Translate old string into new string */
    while(*old_str != '\0')
    {
        switch (*old_str)
        {
            case '!':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '1';   break;
            case '#':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '3';   break;
            case '$':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '4';   break;
            case '&':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '6';   break;
            case '\'':  *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '7';   break;
            case '(':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '8';   break;
            case ')':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = '9';   break;
            case '*':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = 'A';   break;
            case '+':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = 'B';   break;
            case ',':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = 'C';   break;
            case '/':   *(new_str++) = '%';   *(new_str++) = '2';   *(new_str++) = 'F';   break;
            case ':':   *(new_str++) = '%';   *(new_str++) = '3';   *(new_str++) = 'A';   break;
            case ';':   *(new_str++) = '%';   *(new_str++) = '3';   *(new_str++) = 'B';   break;
            case '=':   *(new_str++) = '%';   *(new_str++) = '3';   *(new_str++) = 'D';   break;
            case '?':   *(new_str++) = '%';   *(new_str++) = '3';   *(new_str++) = 'F';   break;
            case '@':   *(new_str++) = '%';   *(new_str++) = '4';   *(new_str++) = '0';   break;
            case '[':   *(new_str++) = '%';   *(new_str++) = '5';   *(new_str++) = 'B';   break;
            case ']':   *(new_str++) = '%';   *(new_str++) = '5';   *(new_str++) = 'D';   break;
            default:    *(new_str++) = *old_str;  break;
        }
        old_str++;
    }
    *(new_str++) = '\0';

    /* Calculate length of new string */
    len = new_str - alloc_str;

    /* Replace member string */
    delete [] str;
    str = alloc_str;

    /* Return Self */
    return *this;
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
List<SafeString>* SafeString::split(char separator, bool strip)
{
    List<SafeString>* tokens = new List<SafeString>;

    char token[MAX_STR_SIZE];
    int i = 0;

    while(i < len && str[i] != '\0')
    {
        /* Create Token */
        int t = 0;
        while( (i < len) && (str[i] != '\0') && (str[i] == separator) ) i++; // find first character
        while( (i < len) && (str[i] != '\0') && (str[i] != separator) && (t < (MAX_STR_SIZE - 1))) token[t++] = str[i++]; // copy characters in
        token[t++] = '\0';

        /*  Strip Leading and Trailing Spaces */
        int s1 = 0, s2 = t-1;
        if(strip)
        {
            while( (s1 < t) && isspace(token[s1]) ) s1++;
            while( (s2 > s1) && isspace(token[s2]) ) s2--;
            token[++s2] = '\0';
        }

        /* Add Token to List */
        SafeString ss("%s", &token[s1]);
        if(ss.getLength() > 1) tokens->add(ss);
    }

    return tokens;
}

/*----------------------------------------------------------------------------
 * operator[]
 *----------------------------------------------------------------------------*/
char SafeString::operator[](int index)
{
    if(index >= 0 && index < len)
    {
        return str[index];
    }
    else
    {
        return '\0';
    }
}

/*----------------------------------------------------------------------------
 * operator+=
 *----------------------------------------------------------------------------*/
SafeString& SafeString::operator+=(const SafeString& rhs)
{
    if((len + rhs.len) < (maxlen - 1))
    {
        StringLib::concat(str, rhs.str, maxlen);
        len += rhs.len;
    }
    else
    {
        maxlen += rhs.maxlen;
        char* new_str = new char[maxlen];
        LocalLib::set(new_str, 0, maxlen);
        StringLib::copy(new_str, str, maxlen);
        StringLib::concat(new_str, rhs.str, maxlen);
        len = (int)strlen(new_str) + 1;
        delete [] str;
        str = new_str;
    }

    return *this;
}

/*----------------------------------------------------------------------------
 * operator+=
 *----------------------------------------------------------------------------*/
SafeString& SafeString::operator+=(const char* rstr)
{
    int rlen = (int)strlen(rstr);

    if((len + rlen) < (maxlen - 1))
    {
        StringLib::concat(str, rstr, maxlen);
        len += rlen;
    }
    else
    {
        maxlen += rlen + 1;
        maxlen *= 2; // optimization
        char* new_str = new char[maxlen];
        LocalLib::set(new_str, 0, maxlen);
        StringLib::copy(new_str, str, maxlen);
        StringLib::concat(new_str, rstr, maxlen);
        len = (int)strlen(new_str) + 1;
        delete [] str;
        str = new_str;
    }

    return *this;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
SafeString& SafeString::operator=(const SafeString& rhs)
{
    if(maxlen < rhs.len)
    {
        delete [] str;
        maxlen = rhs.maxlen;
        str = new char[maxlen];
    }

    StringLib::copy(str, rhs.str, rhs.len);
    len = rhs.len;

    return *this;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
SafeString& SafeString::operator=(const char* rstr)
{
    if(rstr)
    {
        int rlen = (int)strlen(rstr);
        if(rlen <= 0)
        {
            str[0] = '\0';
            len = 1;
        }
        else if(maxlen < rlen)
        {
            delete [] str;
            maxlen = rlen;
            str = new char[maxlen];
        }

        StringLib::copy(str, rstr, rlen);
        len = rlen + 1;
    }
    else
    {
        delete [] str;
        maxlen = DEFAULT_STR_SIZE;
        str = new char[maxlen];
        LocalLib::set(str, 0, maxlen);
        len = 1;
    }

    return *this;
}

/*----------------------------------------------------------------------------
 * operator+
 *----------------------------------------------------------------------------*/
SafeString operator+(SafeString lhs, const SafeString& rhs)
{
    lhs += rhs;
    return lhs;
}

/*----------------------------------------------------------------------------
 * reset
 *----------------------------------------------------------------------------*/
void SafeString::reset(void)
{
    delete [] str;
    maxlen = DEFAULT_STR_SIZE;
    str = new char[maxlen];
    LocalLib::set(str, 0, maxlen);
    len = 1;
}


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * duplicate
 *----------------------------------------------------------------------------*/
char* StringLib::duplicate(const char* str1, int size)
{
    int len;
    if(str1 == NULL) return NULL;
    if(size > 0) len = (int)strnlen(str1, size - 1) + 1;
    else len = (int)strlen(str1) + 1;
    if(len < 1) return NULL;
    char* dup = new char[len];
    StringLib::copy(dup, str1, len);
    return dup;
}

/*----------------------------------------------------------------------------
 * concat
 *
 *  caller is responsible for freeing memory
 *----------------------------------------------------------------------------*/
char* StringLib::concat(const char* str1, const char* str2, const char* str3)
{
    int str1len = 0;
    int str2len = 0;
    int str3len = 0;

    if(str1 != NULL) str1len = (int)strlen(str1);
    if(str2 != NULL) str2len = (int)strlen(str2);
    if(str3 != NULL) str3len = (int)strlen(str3);

    int newstrlen = str1len + str2len + str3len + 1;
    char* newstr = new char[newstrlen];
    newstr[0] = '\0';

    if(str1 != NULL) StringLib::concat(newstr, str1, newstrlen);
    if(str2 != NULL) StringLib::concat(newstr, str2, newstrlen);
    if(str3 != NULL) StringLib::concat(newstr, str3, newstrlen);
    newstr[newstrlen - 1] = '\0';

    return newstr;
}

/*----------------------------------------------------------------------------
 * concat
 *
 *  no memory allocated, copies into str1
 *----------------------------------------------------------------------------*/
void StringLib::concat(char* str1, const char* str2, int size)
{
    strncat(str1, str2, size);
}

/*----------------------------------------------------------------------------
 * format
 *
 *  no memory allocated
 *  returns dststr for syntax convenience
 *----------------------------------------------------------------------------*/
char* StringLib::format(char* dststr, int size, const char* _format, ...)
{
    if (dststr == NULL) return NULL;
    va_list args;
    va_start(args, _format);
    int vlen = vsnprintf(dststr, size, _format, args);
    int slen = MIN(vlen, size - 1);
    va_end(args);
    if (slen < 1) return NULL;
    dststr[slen] = '\0';
    return dststr;
}

/*----------------------------------------------------------------------------
 * format
 *
 *  no memory allocated
 *  returns size of formatted string (not including null terminator)
 *----------------------------------------------------------------------------*/
int StringLib::formats(char* dststr, int size, const char* _format, ...)
{
    if (dststr == NULL) return 0;
    va_list args;
    va_start(args, _format);
    int vlen = vsnprintf(dststr, size - 1, _format, args);
    int slen = MIN(vlen, size - 1);
    va_end(args);
    if (slen < 1) return 0;
    dststr[slen] = '\0';
    return slen;
}

/*----------------------------------------------------------------------------
 * copy
 *----------------------------------------------------------------------------*/
char* StringLib::copy(char* str1, const char* str2, int _size)
{
    if(str1 && str2 && (_size > 0))
    {
        char* nptr = (char*)memccpy(str1, str2, 0, _size);
        if(!nptr) str1[_size - 1] = '\0';
    }
    else if(str1 && (_size > 0))
    {
        str1[0] = '\0';
    }

    return str1;
}

/*----------------------------------------------------------------------------
 * find
 *
 *  returns -1 if string not found
 *----------------------------------------------------------------------------*/
char* StringLib::find(const char* big, const char* little, int len)
{
    int little_len = strnlen(little, len);
    int big_len = strnlen(big, len);

    if(little_len > 0)
    {
        for(int i=0; i<=(big_len - little_len); i++)
        {
            if((big[i] == little[0]) && StringLib::match(&big[i], little, little_len))
            {
                return (char*)&big[i];
            }
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * find
 *
 *  assumes that str is null terminated
 *----------------------------------------------------------------------------*/
char* StringLib::find(const char* str, const char c, bool first)
{
    if(first)   return (char*)strchr(str, c);
    else        return (char*)strrchr(str, c);
}

/*----------------------------------------------------------------------------
 * size
 *----------------------------------------------------------------------------*/
int StringLib::size(const char* str, int len)
{
    return strnlen(str, len);
}

/*----------------------------------------------------------------------------
 * match
 *
 *  exact match only
 *----------------------------------------------------------------------------*/
bool StringLib::match(const char* str1, const char* str2, int len)
{
    return strncmp(str1, str2, len) == 0;
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
StringLib::TokenList* StringLib::split(const char* str, int len, char separator, bool strip)
{
    TokenList* tokens = new TokenList;

    int i = 0;
    while(i < len && str[i] != '\0')
    {
        /* Create Token */
        char* token = new char[MAX_STR_SIZE];
        int t = 0;
        while( (i < len) && (str[i] != '\0') && (str[i] == separator) ) i++; // find first character
        while( (i < len) && (str[i] != '\0') && (str[i] != separator) && (t < (MAX_STR_SIZE - 1))) token[t++] = str[i++]; // copy characters in
        token[t++] = '\0';

        /*  Strip Leading and Trailing Spaces */
        int s1 = 0, s2 = t-1;
        if(strip)
        {
            while( (s1 < t) && isspace(token[s1]) ) s1++;
            while( (s2 > s1) && isspace(token[s2]) ) s2--;
            token[++s2] = '\0';
        }

        /* Add Token to List */
        if(t > 1) tokens->add(token);
        else delete [] token;
    }

    return tokens;
}

/*----------------------------------------------------------------------------
 * convertUpper
 *
 *  converts in place
 *----------------------------------------------------------------------------*/
void StringLib::convertUpper(char* str1)
{
    int slen = (int)strlen(str1) + 1;
    for(int i = 0; i < slen; i++)
    {
        if(isalpha(str1[i]) && islower(str1[i]))
        {
            str1[i] = toupper(str1[i]);
        }
    }
}

/*----------------------------------------------------------------------------
 * convertUpper
 *
 *  converts from src to dst, returns dst for c syntax convenience
 *----------------------------------------------------------------------------*/
char* StringLib::convertUpper(char* dst, char* src)
{
    int slen = (int)strlen(src) + 1;
    for(int i = 0; i < slen; i++)
    {
        if(isalpha(src[i]) && islower(src[i]))
        {
            dst[i] = toupper(src[i]);
        }
        else
        {
            dst[i] = src[i];
        }
    }
    return dst;
}

/*----------------------------------------------------------------------------
 * convertLower
 *
 *  converts in place
 *----------------------------------------------------------------------------*/
void StringLib::convertLower(char* str1)
{
    int slen = (int)strlen(str1) + 1;
    for(int i = 0; i < slen; i++)
    {
        if(isalpha(str1[i]) && isupper(str1[i]))
        {
            str1[i] = tolower(str1[i]);
        }
    }
}

/*----------------------------------------------------------------------------
 * convertLower
 *
 *  converts from src to dst, returns dst for c syntax convenience
 *----------------------------------------------------------------------------*/
char* StringLib::convertLower(char* dst, char* src)
{
    int slen = (int)strlen(src) + 1;
    for(int i = 0; i < slen; i++)
    {
        if(isalpha(src[i]) && isupper(src[i]))
        {
            dst[i] = tolower(src[i]);
        }
        else
        {
            dst[i] = src[i];
        }
    }
    return dst;
}

/*----------------------------------------------------------------------------
 * tokenizeLine
 *
 *  characters inside quotation marks will ignore separator
 *----------------------------------------------------------------------------*/
int StringLib::tokenizeLine(const char* str1, int str_size, char separator, int numtokens, char tokens[][MAX_STR_SIZE])
{
    int t = 0, i = 0, j = 0;

    if(str1 == NULL || tokens == NULL) return 0;

    while(t < numtokens)
    {
        // find first character
        while( ((i < str_size) && (str1[i] != '\0')) && ((str1[i] == separator) || !isgraph(str1[i])) ) i++; // find first character
        if((i >= str_size) || (str1[i] == '\0')) break; // end of string

        // read in token
        if(str1[i] != '"')
        {
            while( (i < str_size) && (str1[i] != '\0') && (str1[i] != separator) && isgraph(str1[i]) )
            {
                if(j >= (MAX_STR_SIZE - 1)) break; // leave room for null terminator
                tokens[t][j++] = str1[i++]; // copy characters in until next separator
            }
        }
        else
        {
            i++; // go past opening quotation
            while( (i < str_size) && (str1[i] != '\0') && (str1[i] != '"') && (isgraph(str1[i]) || (str1[i] == ' ')) )
            {
                if(j >= (MAX_STR_SIZE - 1)) break; // leave room for null terminator
                tokens[t][j++] = str1[i++]; // copy characters in until closing quote
            }
            if((i < str_size) && (str1[i] == '"')) i++; // go past closing quotation
        }

        // terminate token and go to next
        tokens[t][j] = '\0';
        j = 0;
        t++;
    }

    return t;
}

/*----------------------------------------------------------------------------
 * getLine
 *
 *  reads a line of text from file
 *----------------------------------------------------------------------------*/
int StringLib::getLine(char* str, int* ret_len, int max_str_size, FILE* fptr)
{
    int c = 0, i = 0;

    if (str == NULL || max_str_size < 1) return 0;

    while (i < max_str_size - 1)
    {
        c = fgetc(fptr);
        str[i++] = c;
        if (c == '\n' || c == EOF) break;
    }
    str[i++] = '\0';

    if (ret_len != NULL)
    {
        *ret_len = i;
    }

    if (c != EOF) return 0;
    else          return -1;
}

/*----------------------------------------------------------------------------
 * str2bool
 *----------------------------------------------------------------------------*/
bool StringLib::str2bool(const char* str, bool* val)
{
    if(str == NULL) return false;

    if(StringLib::match(str, "ENABLE") || StringLib::match(str, "enable"))
    {
        *val = true;
    }
    else if(StringLib::match(str, "DISABLE") || StringLib::match(str, "disable"))
    {
        *val = false;
    }
    else if(StringLib::match(str, "TRUE") || StringLib::match(str, "true"))
    {
        *val = true;
    }
    else if(StringLib::match(str, "FALSE") || StringLib::match(str, "false"))
    {
        *val = false;
    }
    else
    {
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * str2long
 *----------------------------------------------------------------------------*/
bool StringLib::str2long(const char* str, long* val, int base)
{
    if(str == NULL) return false;
    char *endptr;
    errno = 0;
    long result = strtol(str, &endptr, base);
    if( (endptr == str) ||
        ((result == LONG_MAX || result == LONG_MIN) && errno == ERANGE) )
    {
        return false;
    }
    *val = result;
    return true;
}

/*----------------------------------------------------------------------------
 * str2ulong
 *----------------------------------------------------------------------------*/
bool StringLib::str2ulong(const char* str, unsigned long* val, int base)
{
    if(str == NULL) return false;
    char *endptr;
    errno = 0;
    unsigned long result = strtoul(str, &endptr, base);
    if( (endptr == str) ||
        ((result == ULONG_MAX || result == 0) && errno == ERANGE) )
    {
        return false;
    }
    *val = result;
    return true;
}

/*----------------------------------------------------------------------------
 * str2llong
 *----------------------------------------------------------------------------*/
bool StringLib::str2llong(const char* str, long long* val, int base)
{
    if(str == NULL) return false;
    char *endptr;
    errno = 0;
    long long result = strtoll(str, &endptr, base);
    if( (endptr == str) ||
        ((result == LLONG_MAX || result == LLONG_MIN) && errno == ERANGE))
    {
        return false;
    }
    *val = result;
    return true;
}

/*----------------------------------------------------------------------------
 * str2ullong
 *----------------------------------------------------------------------------*/
bool StringLib::str2ullong(const char* str, unsigned long long* val, int base)
{
    if(str == NULL) return false;
    char *endptr;
    errno = 0;
    unsigned long long result = strtoull(str, &endptr, base);
    if( (endptr == str) ||
        ((result == ULLONG_MAX || result == 0) && errno == ERANGE))
    {
        return false;
    }
    *val = result;
    return true;
}

/*----------------------------------------------------------------------------
 * str2double
 *----------------------------------------------------------------------------*/
bool StringLib::str2double(const char* str, double* val)
{
    if(str == NULL) return false;
    char *endptr;
    errno = 0;
    double result = strtod(str, &endptr);
    if(endptr == str)
    {
        return false;
    }
    *val = result;
    return true;
}

/*----------------------------------------------------------------------------
 * checkNullStr
 *----------------------------------------------------------------------------*/
char* StringLib::checkNullStr (const char* str)
{
    if((str == NULL) || StringLib::match(str, "NULL") || StringLib::match(str, "null") || StringLib::match(str, "NILL") || StringLib::match(str, "nill"))
    {
        return NULL;
    }
    else
    {
        return (char*)str;
    }
}

/*----------------------------------------------------------------------------
 * b64encode
 *
 * Code based on https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c/13935718
 * Author: polfosol
 * License: assumed to be CC BY-SA 3.0
 *----------------------------------------------------------------------------*/
char* StringLib::b64encode(const void* data, int* size)
{
    assert(size);

    int len = *size;
    int encoded_len = (len + 2) / 3 * 4;
    char* str = new char [encoded_len + 1];
    str[encoded_len] = '\0';
    str[encoded_len - 1] = '=';
    str[encoded_len - 2] = '=';

    unsigned char *p = (unsigned  char*) data;
    size_t j = 0, pad = len % 3;
    const size_t last = len - pad;

    for (size_t i = 0; i < last; i += 3)
    {
        int n = int(p[i]) << 16 | int(p[i + 1]) << 8 | p[i + 2];
        str[j++] = B64CHARS[n >> 18];
        str[j++] = B64CHARS[n >> 12 & 0x3F];
        str[j++] = B64CHARS[n >> 6 & 0x3F];
        str[j++] = B64CHARS[n & 0x3F];
    }

    if (pad)  /// Set padding
    {
        int n = --pad ? int(p[last]) << 8 | p[last + 1] : p[last];
        str[j++] = B64CHARS[pad ? n >> 10 & 0x3F : n >> 2];
        str[j++] = B64CHARS[pad ? n >> 4 & 0x03F : n << 4 & 0x3F];
        str[j++] = pad ? B64CHARS[n << 2 & 0x3F] : '=';
    }

    *size = encoded_len + 1;
    return str;
}

/*----------------------------------------------------------------------------
 * b64encode
 *
 * Code based on https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c/13935718
 * Author: polfosol
 * License: assumed to be CC BY-SA 3.0
 *----------------------------------------------------------------------------*/
unsigned char* StringLib::b64decode(const void* data, int* size)
{
    assert(size);

    int len = *size;
    if (len == 0) return (unsigned char*)"";

    unsigned char *p = (unsigned char*) data;
    size_t j = 0,
        pad1 = len % 4 || p[len - 1] == '=',
        pad2 = pad1 && (len % 4 > 2 || p[len - 2] != '=');
    const size_t last = (len - pad1) / 4 << 2;

    int decoded_len = last / 4 * 3 + pad1 + pad2;
    unsigned char* str = new unsigned char [decoded_len];
    str[decoded_len - 1] = '\0';
    str[decoded_len - 2] = '\0';

    for (size_t i = 0; i < last; i += 4)
    {
        int n = B64INDEX[p[i]] << 18 | B64INDEX[p[i + 1]] << 12 | B64INDEX[p[i + 2]] << 6 | B64INDEX[p[i + 3]];
        str[j++] = n >> 16;
        str[j++] = n >> 8 & 0xFF;
        str[j++] = n & 0xFF;
    }

    if (pad1)
    {
        int n = B64INDEX[p[last]] << 18 | B64INDEX[p[last + 1]] << 12;
        str[j++] = n >> 16;
        if (pad2)
        {
            n |= B64INDEX[p[last + 2]] << 6;
            str[j++] = n >> 8 & 0xFF;
        }
    }

    *size = decoded_len;
    return str;
}

/*----------------------------------------------------------------------------
 * b16encode
 *----------------------------------------------------------------------------*/
char* StringLib::b16encode(const void* data, int size, bool lower_case, char* dst)
{
    assert(data);
    assert(size > 0);

    static const char* lower_digits = "0123456789abcdef";
    static const char* upper_digits = "0123456789ABCDEF";

    const char* digits;
    if(lower_case) digits = lower_digits;
    else digits = upper_digits;

    int encoded_len = size * 2;
    char* str = dst;
    if(!str) str = new char [encoded_len + 1];
    str[encoded_len] = '\0';

    uint8_t* data_ptr = (uint8_t*)data;
    for(int i = 0, j = 0; i < size; i++)
    {
        str[j++] = digits[data_ptr[i] >> 4];
        str[j++] = digits[data_ptr[i] & 0x0F];
    }

    return str;
}

/*----------------------------------------------------------------------------
 * printify
 *----------------------------------------------------------------------------*/
int StringLib::printify (char* buffer, int size)
{
    int replacements = 0;
    for(int i = 0; i < size; i++)
    {
        /* Check if in Printable Range */
        if(buffer[i] < 32 || buffer[i] > 126)
        {
            replacements++;
            buffer[i] = '.';
        }
    }
    buffer[size - 1] = '\0';
    return replacements;
}
