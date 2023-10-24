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

#include "SafeString.h"
#include "StringLib.h"
#include "OsApi.h"

#include <cstdarg>
#include <string.h>
#include <limits.h>


/******************************************************************************
 * SAFE STRING PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SafeString::SafeString(long _maxlen)
{
    if(_maxlen <= 0)    maxlen = DEFAULT_STR_SIZE;
    else                maxlen = _maxlen;
    carray = new char[maxlen];
    carray[0] ='\0';
    len = 1;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SafeString::SafeString(long _maxlen, const char* _str, ...)
{
    if(_str != NULL)
    {
        va_list args;
        va_start(args, _str);
        len = vsnprintf(NULL, _maxlen, _str, args) + 1; // get length
        va_end(args);
        carray = new char[len]; // allocate memory
        maxlen = len;
        va_start(args, _str);
        vsprintf(carray, _str, args); // copy in formatted contents
        va_end(args);
        carray[maxlen - 1] ='\0'; // null terminate
    }
    else
    {
        maxlen = DEFAULT_STR_SIZE;
        carray = new char[maxlen];
        carray[0] = '\0';
        len = 1;
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SafeString::SafeString(const char* _str)
{
    maxlen = strlen(_str) + 1;
    carray = new char[maxlen];
    len = maxlen;
    StringLib::copy(carray, _str, len);
    carray[len - 1] = '\0';
}

/*----------------------------------------------------------------------------
 * Constructor (COPY)
 *----------------------------------------------------------------------------*/
SafeString::SafeString(const SafeString& other)
{
    maxlen = other.maxlen;
    carray = new char[maxlen];
    len = other.len;
    StringLib::copy(carray, other.carray, len);
    carray[len - 1] = '\0';
}

/*----------------------------------------------------------------------------
 * Constructor (ENCODE)
 *----------------------------------------------------------------------------*/
SafeString::SafeString (int base, unsigned char* buffer, int size)
{
    if(base == 64)
    {
        int encoded_len = size;
        carray = StringLib::b64encode(buffer, &encoded_len);
        maxlen = encoded_len;
        len = encoded_len;
    }
    else
    {
        maxlen = DEFAULT_STR_SIZE;
        carray = new char[maxlen];
        memset(carray, 0, maxlen);
        len = 1;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SafeString::~SafeString(void)
{
    delete [] carray;
}

/*----------------------------------------------------------------------------
 * getString
 *----------------------------------------------------------------------------*/
const char* SafeString::str(bool duplicate)
{
    if(duplicate)
    {
        char* new_str = new char[len];
        StringLib::copy(new_str, carray, len);
        return new_str;
    }

    return carray;
}

/*----------------------------------------------------------------------------
 * length - number of non-null characters in string
 *----------------------------------------------------------------------------*/
long SafeString::length(void) const
{
    return len - 1; // remove null terminator in length
}

/*----------------------------------------------------------------------------
 * bytes - size of memory needed to store string (includes null terminator)
 *----------------------------------------------------------------------------*/
long SafeString::bytes(void) const
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
        memset(new_str, 0, maxlen);
        StringLib::copy(new_str, carray, len);
        delete [] carray;
        carray = new_str;
    }

    carray[len-1] = c;
    carray[len] = '\0';
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
            if(carray[i] == c)
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
        carray[index] = c;
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
    char* startptr = strstr(carray, oldtxt);
    char* endptr = startptr + strlen(oldtxt);

    while(startptr != NULL)
    {
        status = true;

        maxlen += newtxtlen;
        char* newstr = new char[maxlen];
        memset(newstr, 0, maxlen);

        memcpy(newstr, carray, (startptr - carray));
        memcpy(newstr + (startptr - carray), newtxt, newtxtlen);
        memcpy(newstr + (startptr - carray) + newtxtlen, endptr, len - (endptr - carray));

        delete [] carray;
        carray = newstr;

        len = (int)strlen(carray) + 1;

        startptr = strstr(carray, oldtxt);
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
            int j = i;
            int k = 0;
            while((j < (len - 1)) && oldtxt[r][k] && (carray[j] == oldtxt[r][k]))
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
        carray[0] = '\0';
        return true;
    }

    /* Allocate New String */
    char* newstr = new char [maxlen];

    /* Populate New String */
    int orig_i = 0;
    int new_i = 0;
    while(carray[orig_i])
    {
        /* For Each Possible Replacement */
        bool replacement = false;
        for(int r = 0; r < num_replacements; r++)
        {
            /* Check for Match */
            int j = orig_i;
            int k = 0;
            while(carray[j] && oldtxt[r][k] && (carray[j] == oldtxt[r][k]))
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
            newstr[new_i++] = carray[orig_i++];
        }
    }

    /* Terminate New String and Replace Existing String */
    newstr[new_i] = '\0';
    delete [] carray;
    carray = newstr;

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
    char* old_str = carray;

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
    delete [] carray;
    carray = alloc_str;

    /* Return Self */
    return *this;
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
List<string*>* SafeString::split(char separator, bool strip)
{
    List<string*>* tokens = new List<string*>;

    int i = 0;
    while(i < length() && carray[i] != '\0')
    {
        char token[MAX_STR_SIZE];
        int t = 0;

        /* Create Token */
        while( (i < length()) && (carray[i] != '\0') && (carray[i] == separator) ) i++; // find first character
        while( (i < length()) && (carray[i] != '\0') && (carray[i] != separator) && (t < (MAX_STR_SIZE - 1))) token[t++] = carray[i++]; // copy characters in
        token[t++] = '\0';

        /*  Strip Leading and Trailing Spaces */
        int s1 = 0;
        if(strip)
        {
            int s2 = t-1;
            while( (s1 < t) && isspace(token[s1]) ) s1++;
            while( (s2 > s1) && isspace(token[s2]) ) s2--;
            token[++s2] = '\0';
        }

        /* Add Token to List */
        if(t > 1)
        {
            string* s = new string(&token[s1]);
            tokens->add(s);
        }
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
        return carray[index];
    }

    return '\0';
}

/*----------------------------------------------------------------------------
 * operator+=
 *----------------------------------------------------------------------------*/
SafeString& SafeString::operator+=(const SafeString& rhs)
{
    if((len + rhs.len) < (maxlen - 1))
    {
        StringLib::concat(carray, rhs.carray, maxlen);
        len += rhs.len;
    }
    else
    {
        maxlen += rhs.maxlen;
        char* new_str = new char[maxlen];
        memset(new_str, 0, maxlen);
        StringLib::copy(new_str, carray, maxlen);
        StringLib::concat(new_str, rhs.carray, maxlen);
        len = (int)strlen(new_str) + 1;
        delete [] carray;
        carray = new_str;
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
        StringLib::concat(carray, rstr, maxlen);
        len += rlen;
    }
    else
    {
        maxlen += rlen + 1;
        maxlen *= 2; // optimization
        char* new_str = new char[maxlen];
        memset(new_str, 0, maxlen);
        StringLib::copy(new_str, carray, maxlen);
        StringLib::concat(new_str, rstr, maxlen);
        len = (int)strlen(new_str) + 1;
        delete [] carray;
        carray = new_str;
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
        delete [] carray;
        maxlen = rhs.maxlen;
        carray = new char[maxlen];
    }

    StringLib::copy(carray, rhs.carray, rhs.len);
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
            carray[0] = '\0';
            len = 1;
        }
        else if(maxlen < rlen)
        {
            delete [] carray;
            maxlen = rlen;
            carray = new char[maxlen];
        }

        StringLib::copy(carray, rstr, rlen);
        len = rlen + 1;
    }
    else
    {
        delete [] carray;
        maxlen = DEFAULT_STR_SIZE;
        carray = new char[maxlen];
        memset(carray, 0, maxlen);
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
    delete [] carray;
    maxlen = DEFAULT_STR_SIZE;
    carray = new char[maxlen];
    memset(carray, 0, maxlen);
    len = 1;
}
