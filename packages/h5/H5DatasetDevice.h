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

#ifndef __h5_dataset__
#define __h5_dataset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "RecordObject.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 DATASET HANDLER
 ******************************************************************************/

class H5DatasetDevice: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* recType;
        static const RecordObject::fieldDef_t recDef[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int64_t     id;
            uint32_t    datatype; // RecordObject::valType_t
            uint32_t    offset;
            uint32_t    size;
        } h5dataset_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RecordObject*   recObj;
        h5dataset_t*    recData;
        char*           fileName;
        const char*     dataName;
        uint8_t*        dataBuffer;
        int             dataSize;
        int             dataOffset;
        bool            rawMode;

        bool            connected;
        char*           config; // <filename>(<type>,<access>,<io>)

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            H5DatasetDevice     (lua_State* L, role_t _role, const char* filename, const char* dataset_name, long id, bool raw_mode, 
                                                    RecordObject::valType_t datatype, long col, long startrow, long numrows);
                            ~H5DatasetDevice    (void);

        virtual bool        isConnected         (int num_open=0);   // is the file open
        virtual void        closeConnection     (void);             // close the file
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);             // returns file descriptor
        virtual const char* getConfig           (void);             // returns filename with attribute list
};

#endif  /* __h5_dataset__ */
