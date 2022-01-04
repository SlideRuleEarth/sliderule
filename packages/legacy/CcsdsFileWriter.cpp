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

#include "CcsdsFileWriter.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsFileWriter::TYPE = "CcsdsFileWriter";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* CcsdsFileWriter::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    fmt_t       format = str2fmt(argv[0]);
    const char* prefix = StringLib::checkNullStr(argv[1]);
    const char* stream = StringLib::checkNullStr(argv[2]);
    const char* maxstr = NULL; // argv[3]

    if(format == INVALID)
    {
        mlog(CRITICAL, "Error: invalid format specified for file writer %s", name);
        return NULL;
    }

    if(prefix == NULL)
    {
        mlog(CRITICAL, "Error: prefix cannot be NULL");;
        return NULL;
    }

    if(stream == NULL)
    {
        mlog(CRITICAL, "Error: stream cannot be NULL");
        return NULL;
    }

    unsigned long filesize = CcsdsFileWriter::FILE_MAX_SIZE;
    if(argc == 4)
    {
        maxstr = argv[3];
        if(!StringLib::str2ulong(maxstr, &filesize))
        {
            mlog(CRITICAL, "Error: invalid max file size: %s", maxstr);
            return NULL;
        }
    }

    return new CcsdsFileWriter(cmd_proc, name, format, prefix, stream, filesize);
}

/*----------------------------------------------------------------------------
 * str2fmt
 *----------------------------------------------------------------------------*/
CcsdsFileWriter::fmt_t CcsdsFileWriter::str2fmt(const char* str)
{
         if(StringLib::match(str, "RAW_BINARY"))    return RAW_BINARY;
    else if(StringLib::match(str, "RAW_ASCII"))     return RAW_ASCII;
    else if(StringLib::match(str, "TEXT"))          return TEXT;
    else if(StringLib::match(str, "USER_DEFINED"))  return USER_DEFINED;
    else                                            return INVALID;
}

/*----------------------------------------------------------------------------
 * str2fmt
 *----------------------------------------------------------------------------*/
const char* CcsdsFileWriter::fmt2str(CcsdsFileWriter::fmt_t _fmt)
{
         if(_fmt == RAW_BINARY)     return "RAW_BINARY";
    else if(_fmt == RAW_ASCII)      return "RAW_ASCII";
    else if(_fmt == TEXT)           return "TEXT";
    else if(_fmt == USER_DEFINED)   return "USER_DEFINED";
    else                            return "INVALID";
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsFileWriter::CcsdsFileWriter(CommandProcessor* cmd_proc, const char* obj_name, fmt_t _fmt, const char* _prefix, const char* inq_name, unsigned int _maxFileSize):
    CcsdsMsgProcessor(cmd_proc, obj_name, TYPE, inq_name)
{
    assert(_prefix);

    fmt = _fmt;

    int len = (int)StringLib::size(_prefix) + 1;
    prefix = new char[len];
    StringLib::copy(prefix, _prefix, len);

    LocalLib::set(filename, 0, FILENAME_MAX_CHARS);

    outfp = NULL;

    recordsWritten = 0;
    fileCount = 0;
    fileBytesWritten = 0;
    maxFileSize = _maxFileSize;

    registerCommand("FLUSH", (cmdFunc_t)&CcsdsFileWriter::flushCmd,  0,  "");

    start(); // start processor
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsFileWriter::~CcsdsFileWriter(void)
{
    stop(); // stop processor

    delete [] prefix;
    if(outfp)
    {
        if(outfp != stdout)
        {
            fclose(outfp);
        }
        outfp = NULL;
    }
}

/*----------------------------------------------------------------------------
 * openNewFile
 *
 * Parameters:  prefix to prepend to file (may include path)
 *----------------------------------------------------------------------------*/
bool CcsdsFileWriter::openNewFile(void)
{
    /* Set Counters */
    fileBytesWritten = 0;
    fileCount++;

    /* Check for Standard Output */
    if((StringLib::match(prefix, "STDOUT")) || (StringLib::match(prefix, "stdout")))
    {
        outfp = stdout;
        return true;
    }
    else if((StringLib::match(prefix, "STDERR")) || (StringLib::match(prefix, "stderr")))
    {
        outfp = stderr;
        return true;
    }

    /* Open New File */
    if(outfp != NULL) fclose(outfp);
    StringLib::format(filename, FILENAME_MAX_CHARS, "%s_%ld.out", prefix, fileCount);
    if(isBinary())  outfp = fopen((const char*)filename, "wb");
    else            outfp = fopen((const char*)filename, "w");
    if(outfp == NULL)
    {
    	mlog(CRITICAL, "Error opening file: %s, err: %s", filename, LocalLib::err2str(errno));
        return false;
    }

    /* Process Open File */
    mlog(INFO, "Opened new file for writing: %s", filename);

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * writeMsg - VIRTUAL FUNCTION
 *----------------------------------------------------------------------------*/
int CcsdsFileWriter::writeMsg(void* msg, int size, bool with_header)
{
    (void)with_header;

    /* RAW BINARY */
    if(fmt == RAW_BINARY)
    {
        return (int)fwrite(msg, 1, size, outfp);
    }
    /* RAW ASCII */
    else if(fmt == RAW_ASCII)
    {
        unsigned char* pkt_buffer = (unsigned char*)msg;
        int bytes = 0, ret = 0;
        for(int i = 0; i < size; i++)
        {
            ret = fprintf(outfp, "%02X", pkt_buffer[i]);
            if(ret < 0) return ret;
            else        bytes += ret;
        }
        ret = fprintf(outfp, "\n");
        if( ret < 0)    return ret;
        else            bytes += ret;
        return bytes;
    }
    /* TEXT */
    else if(fmt == TEXT)
    {
        int bytes = fprintf(outfp, "%s", (const char*)msg);
        fflush(outfp); // no need to worry about performance
        return bytes;
    }
    /* SHOULD NEVER REACH HERE */
    else
    {
        return -1;
    }
}


/*----------------------------------------------------------------------------
 * isBinary
 *----------------------------------------------------------------------------*/
bool CcsdsFileWriter::isBinary (void)
{
         if(fmt == RAW_BINARY)  return true;
    else if(fmt == RAW_ASCII)   return false;
    else if(fmt == TEXT)        return false;
    else                        return true;
}

/*----------------------------------------------------------------------------
 * processMsg
 *
 *   implementation of virtual method for handling messages
 *----------------------------------------------------------------------------*/
bool CcsdsFileWriter::processMsg (unsigned char* msg, int bytes)
{
    bool write_header = false;

    /* Manage Files */
    if(outfp == NULL || fileBytesWritten > maxFileSize)
    {
        fileBytesWritten = 0;
        if(openNewFile() == false)
        {
            return false;
        }
        else
        {
            write_header = true;
        }
    }

    /* Write Record */
    int bytes_written = writeMsg(msg, bytes, write_header);

    /* Error Checking */
    if(bytes_written < 0)
    {
        mlog(CRITICAL, "Fatal error, unable to write file %s with error: %s... killing writer!", filename, LocalLib::err2str(errno));
        return false;
    }
    else
    {
        recordsWritten++;
        fileBytesWritten += bytes_written;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * flushCmd
 *----------------------------------------------------------------------------*/
int CcsdsFileWriter::flushCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    fflush(outfp);

    return 0;
}
