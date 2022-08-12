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

#ifndef __file_device__
#define __file_device__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "DeviceObject.h"

/******************************************************************************
 * FILE CLASS
 ******************************************************************************/

class File: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long DEFAULT_FILE_MAX_SIZE = 0x8000000;
        static const long FILENAME_MAX_CHARS = 512;
        static const long INFINITE_FILE_MAX_SIZE = -1;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            BINARY,
            ASCII,
            TEXT,
            FIFO,
            INVALID_TYPE
        } type_t;

        typedef enum {
            FLUSHED,
            CACHED,
            INVALID_IO
        } io_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);

                            File                (lua_State* L, const char* _filename, type_t _type, role_t _role, io_t _io=CACHED, long _max_file_size=DEFAULT_FILE_MAX_SIZE);
        virtual             ~File               ();

        bool                isConnected         (int num_open=0) override;   // is the file open
        void                closeConnection     (void) override;             // close the file
        int                 writeBuffer         (const void* buf, int len, int timeout=SYS_TIMEOUT) override;
        int                 readBuffer          (void* buf, int len, int timeout=SYS_TIMEOUT) override;
        int                 getUniqueId         (void) override;             // returns file descriptor
        const char*         getConfig           (void) override;             // returns filename with attribute list

        const char*         getFilename         (void);
        type_t              getType             (void);
        io_t                getIO               (void);

        static type_t       str2type            (const char* str);
        static const char*  type2str            (type_t _type);

        static io_t         str2io              (const char* str);
        static const char*  io2str              (io_t _io);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        fileptr_t       fp;
        char*           filename; // user supplied prefix
        char*           config; // <filename>(<type>,<access>,<io>)
        type_t          type;
        io_t            io;

        // Writer
        char            activeFile[FILENAME_MAX_CHARS]; // current path to file being written
        long            maxFileSize;
        long            fileCount;
        long            fileBytesWritten;

        // Reader
        char**          fileList; // list of files being read
        int             numFiles; // number of files in the list above
        int             currFile; // current file being read

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool            openNewFileForWriting       (void);
        int             createFileListForReading    (char* input_string, char** file_list);
        int             readFifo                    (unsigned char* buf, int len);
        virtual int     writeFileHeader             (void);
};

#endif  /* __file_device__ */
