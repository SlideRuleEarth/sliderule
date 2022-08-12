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

#include <glob.h>

#include "OsApi.h"
#include "File.h"
#include "StringLib.h"
#include "EventLib.h"
#include "DeviceObject.h"

/******************************************************************************
 * FILE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - file(<role>, <format>, <filename(s)>, [<file i/o>], [<max file size>])
 *
 *  <role> is either core.READER or core.WRITER
 *
 *  <format> is core.BINARY, core.ASCII, core.TEXT, or core.FIFO
 *
 *  <filename(s)> is name of the file to be written or a list of regular
 *  expressions of filenames to be read from.  If being written, the filename is
 *  used as provided up to the max file size, and then after that new files are
 *  created with a .x extension appended to the end of the filename, where x is
 *  an incrementing number that starts with 2.  Note that STDOUT, STDERR, and STDIN
 *  are all supported filenames that refer to standard output, error, and input
 *  respectively.
 *
 *  <file i/o> is either core.FLUSHED or core.CACHED.  Flushed means that the file
 *  descriptor is flushed after every write, and cached means that the file descriptor
 *  is flushed when the operating system decides to perform the flush.
 *  It is only specified for writers.
 *
 *  <max file size> is the size of the file to be written.  It is only
 *  specified for writers. Once reached causes the file to be closed and a new
 *  file to be opened.
 *----------------------------------------------------------------------------*/
int File::luaCreate(lua_State* L)
{
    try
    {
        /* Get Parameters */
        int         role      = (int)getLuaInteger(L, 1);
        int         format    = (int)getLuaInteger(L, 2);
        const char* file_str  = getLuaString(L, 3);
        File::io_t  file_io   = (File::io_t)getLuaInteger(L, 4, true, File::FLUSHED);
        long        max_file  = getLuaInteger(L, 5, true, File::DEFAULT_FILE_MAX_SIZE);

        /* Check Access Type */
        if(role != File::READER && role != File::WRITER)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "unrecognized file access specified: %d", role);
        }

        /* Return File Device Object */
        return createLuaObject(L, new File(L, file_str, (File::type_t)format, (File::role_t)role, (File::io_t)file_io, max_file));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating File: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
File::File (lua_State* L, const char* _filename, type_t _type, role_t _role, io_t _io, long _max_file_size):
    DeviceObject(L, _role)
{
    assert(_filename);

    fp = NULL;

    /* Set Filename */
    filename = StringLib::duplicate(_filename);

    /* Set File Attributes */
    type = _type;
    io = _io;

    /* Writer Attributes */
    activeFile[0] = '\0';
    maxFileSize = type == FIFO ? INFINITE_FILE_MAX_SIZE : _max_file_size;
    fileCount = 0;
    fileBytesWritten = 0;

    /* Reader Attributes */
    fileList = NULL;
    numFiles = 0;
    currFile = 0;
    if(role == READER)
    {
        int num_files = createFileListForReading(filename, NULL);
        if(num_files > 0)
        {
            fileList = new char* [num_files];
            numFiles = createFileListForReading(filename, fileList);
            if(numFiles != num_files)
            {
                mlog(CRITICAL, "Unable to process initial set of files for %s", filename);
            }
        }
        else
        {
            mlog(CRITICAL, "No files found for file %s", filename);
        }
    }

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s (%s, %s, %s)", filename, type2str(type), role == READER ? "READER" : "WRITER", io2str(io)) + 1;
    config = new char[cfglen];
    sprintf(config, "%s (%s, %s, %s)", filename, type2str(type), role == READER ? "READER" : "WRITER", io2str(io));
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
File::~File (void)
{
    if(filename) delete [] filename;
    if(config) delete [] config;
    for(int i = 0; i < numFiles; i++) if(fileList[i]) delete [] fileList[i];
    if(fileList) delete [] fileList;
    closeConnection();
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool File::isConnected (int num_open)
{
    if(fp == NULL) return false;

    if(role == READER)
    {
        if(currFile+1 >= num_open)  return true;
        else                        return false;
    }
    else if(role == WRITER)
    {
        if(fileCount >= num_open)   return true;
        else                        return false;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void File::closeConnection (void)
{
    if(fp != NULL)
    {
        if(fp != stdout && fp != stderr)
        {
            fclose(fp);
            fileCount++;
        }
        fp = NULL;
    }
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int File::writeBuffer (const void* buf, int len, int timeout)
{
    (void)timeout;

    int bytes_written = INVALID_RC;

    /* Check Access */
    if(role != WRITER)
    {
        return ACC_ERR_RC;
    }

    /* Check for Timeout */
    if(buf == NULL || len <= 0)
    {
        fflush(fp);
        return TIMEOUT_RC;
    }

    /* Manage Files */
    if(fp == NULL || ((fileBytesWritten > maxFileSize) && (maxFileSize != INFINITE_FILE_MAX_SIZE)))
    {
        fileBytesWritten = 0;
        if(openNewFileForWriting() == false)
        {
            return INVALID_RC;
        }
        else
        {
            /* Write File Header */
            int hdr_bytes = writeFileHeader();
            if(hdr_bytes < 0)
            {
                return hdr_bytes;
            }
            else
            {
                fileBytesWritten += hdr_bytes;
            }
        }
    }

    /* Write Buffer */
    if(type == BINARY || type == FIFO)
    {
        /* Write Binary Data */
        bytes_written = (int)fwrite(buf, 1, len, fp);
    }
    else if(type == ASCII)
    {
        /* Write Converted Binary Values */
        unsigned char* pkt_buffer = (unsigned char*)buf;
        int  ret = 0;
        for(int i = 0; i < len; i++)
        {
            ret = fprintf(fp, "%02X", pkt_buffer[i]);
            if(ret < 0)
            {
                bytes_written = ret;
                break;
            }
            else
            {
                bytes_written += ret;
            }
        }

        /* Append New Line */
        if(ret >= 0)
        {
            ret = fprintf(fp, "\n");
            if(ret < 0)
            {
                bytes_written = ret;
            }
            else
            {
                bytes_written += ret;
            }
        }
    }
    else if(type == TEXT)
    {
        /* Write String Data */
        bytes_written = fprintf(fp, "%s", (const char*)buf);
    }

    /* IO Flushing/Caching Check */
    if(bytes_written >= 0)
    {
        fileBytesWritten += bytes_written;
        if(io == FLUSHED)
        {
            fflush(fp);
        }
    }
    else
    {
        mlog(CRITICAL, "Fatal error, unable to write file %s with error: %s", activeFile, LocalLib::err2str(errno));
    }

    return bytes_written;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int File::readBuffer (void* buf, int len, int timeout)
{
    (void)timeout;

    /* Check Access */
    if(role != READER)
    {
        return ACC_ERR_RC;
    }
    else if(currFile >= numFiles)
    {
        return SHUTDOWN_RC;
    }

    /* Manage Files */
    if(fp == NULL)
    {
        /* Open Next File */
        if(StringLib::match(fileList[currFile], "STDIN") || StringLib::match(fileList[currFile], "stdin"))
        {
            fp = stdin;
        }
        else
        {
            if(type == BINARY || type == FIFO)  fp = fopen(fileList[currFile], "rb");
            else                                fp = fopen(fileList[currFile], "r");
        }

        /* Check Error */
        if(fp == NULL)
        {
            mlog(CRITICAL, "Unable to open file %s: %s", fileList[currFile], LocalLib::err2str(errno));
            return INVALID_RC;
        }
        else
        {
            mlog(INFO, "Opened file %s", fileList[currFile]);
        }
    }

    /* Read File */
    int recv_bytes = 0;
    if(type == BINARY || type == TEXT)
    {
        /* Read Next Data */
        recv_bytes = (int)fread(buf, 1, len, fp);
        if(recv_bytes < len)
        {
            currFile++;
            fclose(fp);
            fp = NULL;
        }
    }
    else if(type == ASCII)
    {
        /* Read Next Packet */
        unsigned char* pkt_buffer = (unsigned char*)buf;
        char b[5] = {'0', 'x', '\0', '\0', '\0'};
        int ch = EOF;
        int n = 0;
        while(recv_bytes < len)
        {
            ch = getc(fp);
            if(ch == '\n')
            {
                break;
            }
            else if(ch == EOF)
            {
                currFile++;
                fclose(fp);
                fp = NULL;
                break;
            }
            else
            {
                b[2 + (n++ % 2)] = (char)ch;
                if(n % 2 == 0)
                {
                    char *endptr;
                    errno = 0;
                    unsigned long result = strtoul(b, &endptr, 0);
                    if( (endptr == b) ||
                        ((result == ULONG_MAX || result == 0) && errno == ERANGE) )
                    {
                        mlog(CRITICAL, "Read invalid data from file designated as an ASCII HEXDUMP: %s", b);
                        return INVALID_RC;
                    }
                    pkt_buffer[recv_bytes++] = (unsigned char)result;
                }
            }
        }
    }
    else if(type == FIFO)
    {
        recv_bytes = (int)fread(buf, 1, len, fp);
    }
    else
    {
        return INVALID_RC;
    }

    /* Return Bytes Read */
    return recv_bytes;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int File::getUniqueId (void)
{
    return fileno(fp);
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* File::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * getFilename
 *----------------------------------------------------------------------------*/
const char* File::getFilename (void)
{
    return filename;
}

/*----------------------------------------------------------------------------
 * getType
 *----------------------------------------------------------------------------*/
File::type_t File::getType (void)
{
    return type;
}

/*----------------------------------------------------------------------------
 * getIO
 *----------------------------------------------------------------------------*/
File::io_t File::getIO (void)
{
    return io;
}

/*----------------------------------------------------------------------------
 * str2type
 *----------------------------------------------------------------------------*/
File::type_t File::str2type (const char* str)
{
         if(StringLib::match(str, "BINARY"))    return BINARY;
    else if(StringLib::match(str, "ASCII"))     return ASCII;
    else if(StringLib::match(str, "TEXT"))      return TEXT;
    else if(StringLib::match(str, "FIFO"))      return FIFO;
    else                                        return INVALID_TYPE;
}

/*----------------------------------------------------------------------------
 * type2str
 *----------------------------------------------------------------------------*/
const char* File::type2str (type_t _type)
{
         if(_type == BINARY)    return "BINARY";
    else if(_type == ASCII)     return "ASCII";
    else if(_type == TEXT)      return "TEXT";
    else if(_type == FIFO)      return "FIFO";
    else                        return "INVALID";
}

/*----------------------------------------------------------------------------
 *str2io
 *----------------------------------------------------------------------------*/
File::io_t File::str2io (const char* str)
{
         if(StringLib::match(str, "FLUSHED"))   return FLUSHED;
    else if(StringLib::match(str, "CACHED"))    return CACHED;
    else                                        return INVALID_IO;
}

/*----------------------------------------------------------------------------
 * io2str
 *----------------------------------------------------------------------------*/
const char* File::io2str (io_t _io)
{
         if(_io == FLUSHED) return "FLUSHED";
    else if(_io == CACHED)  return "CACHED";
    else                    return "INVALID";
}

/*----------------------------------------------------------------------------
 * openNewFileForWriting
 *----------------------------------------------------------------------------*/
bool File::openNewFileForWriting(void)
{
    /* Set Counters */
    fileBytesWritten = 0;
    fileCount++;

    /* Check for Standard Output and Error */
    if(StringLib::match(filename, "STDOUT") || StringLib::match(filename, "stdout"))
    {
        fp = stdout;
        return true;
    }
    else if(StringLib::match(filename, "STDERR") || StringLib::match(filename, "stderr"))
    {
        fp = stderr;
        return true;
    }

    /* Close Previous File */
    if(fp != NULL) fclose(fp);

    /* Create Active File Name */
    int slen = FILENAME_MAX_CHARS;
    if(fileCount == 1)  slen = snprintf(activeFile, FILENAME_MAX_CHARS - 1, "%s", filename);
    else                slen = snprintf(activeFile, FILENAME_MAX_CHARS - 1, "%s.%ld", filename, fileCount);
    int len = MIN(slen, FILENAME_MAX_CHARS - 1);
    if (len < 0) len = 0;
    activeFile[len] = '\0';

    /* Open Active File */
    if(type == BINARY)  fp = fopen(activeFile, "wb");
    else                fp = fopen(activeFile, "w");

    /* Check for Errors */
    if(fp == NULL)
    {
    	mlog(CRITICAL, "Error opening file: %s, err: %s", activeFile, LocalLib::err2str(errno));
        return false;
    }

    /* Return Success */
    mlog(INFO, "Opened new file for writing: %s", activeFile);
    return true;
}

/*----------------------------------------------------------------------------
 * createFileListForReading
 *----------------------------------------------------------------------------*/
int File::createFileListForReading (char* input_string, char** file_list)
{
    int num_files = 0;

    int i = 0;
    while(input_string[i] != '\0')
    {
        // Go to first non-whitespace
        while(input_string[i] != '\0' && isspace(input_string[i])) i++;
        char* new_buf = &filename[i];
        if(new_buf[0] != '\0')
        {
            // Find end of new buffer
            while(input_string[i] != '\0' && !isspace(input_string[i])) i++;
            input_string[i] = '\0'; // terminate new buffer

            // Treat it as a regular expression
            char* regex = new_buf;
            glob_t glob_buffer;
            glob(regex, 0, NULL, &glob_buffer);
            for (unsigned int m = 0; m < glob_buffer.gl_pathc; m++)
            {
                const char* new_file = glob_buffer.gl_pathv[m];
                if(file_list)
                {
                    mlog(INFO, "Adding %s to file list", new_file);
                    file_list[num_files] = StringLib::duplicate(new_file);
                }
                num_files++;
            }
            globfree( &glob_buffer );

            // Set end of new buffer back to space
            input_string[i] = ' ';
        }
    }

    return num_files;
}

/*----------------------------------------------------------------------------
 * writeFileHeader
 *----------------------------------------------------------------------------*/
int File::writeFileHeader(void)
{
    return 0;
}

