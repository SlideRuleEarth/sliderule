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

        virtual bool        isConnected         (int num_open=0);   // is the file open
        virtual void        closeConnection     (void);             // close the file
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);             // returns file descriptor
        virtual const char* getConfig           (void);             // returns filename with attribute list

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
