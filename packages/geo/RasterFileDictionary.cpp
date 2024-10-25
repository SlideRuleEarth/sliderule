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

#include "RasterFileDictionary.h"
#include "EventLib.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Mutex RasterFileDictionary::mutex;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*
 *  NOTE: only add() method is thread safe
 */

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
uint64_t RasterFileDictionary::add(const string& fileName, bool sample)
{
    uint64_t id;
    mutex.lock();
    {
        if(!fileDict.find(fileName.c_str(), &id))
        {
            id = keySpace | fileVector.size();
            fileDict.add(fileName.c_str(), id);
            fileVector.push_back(fileName);
        }

        if(sample)
        {
            sampleIdSet.insert(id);
        }
    }
    mutex.unlock();
    return id;
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
const char* RasterFileDictionary::get(uint64_t fileId)
{
    const char* fileName = "";

    /* Mask upper 32 bits to get index into vector */
    const uint32_t index = static_cast<uint32_t>(fileId & 0xFFFFFFFF);
    if(index < fileVector.size())
    {
        fileName = fileVector[index].c_str();
    }
    return fileName;
}

/*----------------------------------------------------------------------------
 * setSample
 *----------------------------------------------------------------------------*/
void RasterFileDictionary::setSample(uint64_t sampleFileId)
{
    /* Make sure sampleFileId is valid */
    const char* fileName = get(sampleFileId);
    if(fileName != NULL)
    {
        sampleIdSet.insert(sampleFileId);
    }
    else
    {
        mlog(ERROR, "Invalid sampleFileId: %lu", sampleFileId);
    }
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
void RasterFileDictionary::clear(void)
{
    fileDict.clear();
    fileVector.clear();
    sampleIdSet.clear();
}

/*----------------------------------------------------------------------------
 * getSampleIds
 *----------------------------------------------------------------------------*/
const std::set<uint64_t>& RasterFileDictionary::getSampleIds(void) const
{
    return sampleIdSet;
}

/*----------------------------------------------------------------------------
 * copy
 *----------------------------------------------------------------------------*/
RasterFileDictionary RasterFileDictionary::copy(void)
{
    /* Create a new instance of RasterFileDictionary */
    RasterFileDictionary copyDict(keySpace >> 32);

    /* Copy fileDict */
    Dictionary<uint64_t>::Iterator iter = Dictionary<uint64_t>::Iterator(fileDict);
    for (int i = 0; i < iter.length; ++i)
    {
        /* Add each key-value pair from the original to the copy */
        copyDict.fileDict.add(iter[i].key, iter[i].value);
    }

    /* Copy the fileVector */
    copyDict.fileVector = fileVector;

    /* Copy the sampleIdSet */
    copyDict.sampleIdSet = sampleIdSet;

    return copyDict;
}
