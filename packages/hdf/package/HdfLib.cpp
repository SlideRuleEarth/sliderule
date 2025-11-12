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

#include <hdf5.h>
#include <assert.h>
#include <stdexcept>

#include "HdfLib.h"
#include "RecordObject.h"
#include "OsApi.h"

 /******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

static void close_hid(hid_t hid)
{
    const H5I_type_t type = H5Iget_type(hid);
    switch(type)
    {
        case H5I_FILE: H5Fclose(hid); break;
        case H5I_GROUP: H5Gclose(hid); break;
        case H5I_DATASET: H5Dclose(hid); break;
        default: break;
    }
}
 /******************************************************************************
 * CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * write
 *----------------------------------------------------------------------------*/
bool HdfLib::write (const char* filename, List<dataset_t>& datasets)
{
    List<hid_t> hid_stack;

    const auto cleanup_stack = [&hid_stack]() {
        while(!hid_stack.empty())
        {
            close_hid(hid_stack.top());
            hid_stack.pop();
        }
    };

    const hid_t file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    hid_stack.push(file_id);

    for(int i = 0; i < datasets.length(); i++)
    {
        const dataset_t& dataset = datasets[i];
        if(dataset.dataset_type == GROUP)
        {
            const hid_t group_id = H5Gcreate2(hid_stack.top(), dataset.name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            hid_stack.push(group_id);
        }
        else if(dataset.dataset_type == VARIABLE)
        {
            const int size_of_element = RecordObject::FIELD_TYPE_BYTES[dataset.data_type];
            const unsigned long number_of_elements = static_cast<unsigned long>(dataset.size / size_of_element);
            if(number_of_elements <= 0)
            {
                mlog(CRITICAL, "Invalid variable supplied: %s of size %ld bytes and type %d", dataset.name, dataset.size, dataset.data_type);
                cleanup_stack();
                return false;
            }
            hsize_t dims[1] = {number_of_elements};
            const hid_t dataspace_id = H5Screate_simple(1, dims, NULL);
            long h5tc; // hdf5 create
            long h5tw; // hdf5 write
            switch(dataset.data_type)
            {
                case RecordObject::INT8:    h5tc = H5T_STD_I8LE;    h5tw = H5T_NATIVE_INT8;     break;
                case RecordObject::INT16:   h5tc = H5T_STD_I16LE;   h5tw = H5T_NATIVE_INT16;    break;
                case RecordObject::INT32:   h5tc = H5T_STD_I32LE;   h5tw = H5T_NATIVE_INT32;    break;
                case RecordObject::INT64:   h5tc = H5T_STD_I64LE;   h5tw = H5T_NATIVE_INT64;    break;
                case RecordObject::UINT8:   h5tc = H5T_STD_U8LE;    h5tw = H5T_NATIVE_UINT8;    break;
                case RecordObject::UINT16:  h5tc = H5T_STD_U16LE;   h5tw = H5T_NATIVE_UINT16;   break;
                case RecordObject::UINT32:  h5tc = H5T_STD_U32LE;   h5tw = H5T_NATIVE_UINT32;   break;
                case RecordObject::UINT64:  h5tc = H5T_STD_U64LE;   h5tw = H5T_NATIVE_UINT64;   break;
                case RecordObject::FLOAT:   h5tc = H5T_IEEE_F32LE;  h5tw = H5T_NATIVE_FLOAT;    break;
                case RecordObject::DOUBLE:  h5tc = H5T_IEEE_F64LE;  h5tw = H5T_NATIVE_DOUBLE;   break;
                case RecordObject::TIME8:   h5tc = H5T_STD_I64LE;   h5tw = H5T_NATIVE_INT64;    break;
                case RecordObject::STRING:
                {
                    h5tc = H5Tcopy(H5T_C_S1);
                    H5Tset_size(h5tc, dataset.size);
                    H5Tset_strpad(h5tc, H5T_STR_NULLTERM);  // Pad with null terminator
                    h5tw = h5tc;
                    break;
                }
                default:
                {
                    mlog(CRITICAL, "Invalid variable type supplied for %s: %d", dataset.name, dataset.data_type);
                    cleanup_stack();
                    return false;
                }
            }
            const hid_t plist_id = H5Pcreate(H5P_DATASET_CREATE);
            const hsize_t chunk_dims[1] = { MIN(number_of_elements, 10000) };
            H5Pset_chunk(plist_id, 1, chunk_dims);
            H5Pset_deflate(plist_id, 4); // Enable gzip compression, level 4 (1=fastest, 9=best)
            if(dataset.data_type == RecordObject::INT8 || dataset.data_type == RecordObject::UINT8) H5Pset_shuffle(plist_id); // Enable shuffle
            const hid_t dataset_id = H5Dcreate2(hid_stack.top(), dataset.name, h5tc, dataspace_id, H5P_DEFAULT, plist_id, H5P_DEFAULT);
            const herr_t status = H5Dwrite(dataset_id, h5tw, H5S_ALL, H5S_ALL, H5P_DEFAULT, dataset.data);
            if(dataset.data_type == RecordObject::STRING) H5Tclose(h5tc);
            H5Sclose(dataspace_id);
            H5Pclose(plist_id);
            if(status < 0)
            {
                mlog(CRITICAL, "Failed to write variable %s of size %ld and type %d", dataset.name, number_of_elements, dataset.data_type);
                cleanup_stack();
                return false;
            }
            hid_stack.push(dataset_id);
        }
        else if(dataset.dataset_type == SCALAR)
        {
            const int size_of_element = RecordObject::FIELD_TYPE_BYTES[dataset.data_type];
            const unsigned long number_of_elements = static_cast<unsigned long>(dataset.size / size_of_element);
            if(number_of_elements <= 0)
            {
                mlog(CRITICAL, "Invalid scalar supplied: %s of size %ld bytes and type %d", dataset.name, dataset.size, dataset.data_type);
                cleanup_stack();
                return false;
            }
            const hid_t dataspace_id = H5Screate(H5S_SCALAR);
            hid_t datatype_id;
            switch(dataset.data_type)
            {
                case RecordObject::INT8:    datatype_id = H5T_NATIVE_INT8;     break;
                case RecordObject::INT16:   datatype_id = H5T_NATIVE_INT16;    break;
                case RecordObject::INT32:   datatype_id = H5T_NATIVE_INT32;    break;
                case RecordObject::INT64:   datatype_id = H5T_NATIVE_INT64;    break;
                case RecordObject::UINT8:   datatype_id = H5T_NATIVE_UINT8;    break;
                case RecordObject::UINT16:  datatype_id = H5T_NATIVE_UINT16;   break;
                case RecordObject::UINT32:  datatype_id = H5T_NATIVE_UINT32;   break;
                case RecordObject::UINT64:  datatype_id = H5T_NATIVE_UINT64;   break;
                case RecordObject::FLOAT:   datatype_id = H5T_NATIVE_FLOAT;    break;
                case RecordObject::DOUBLE:  datatype_id = H5T_NATIVE_DOUBLE;   break;
                case RecordObject::TIME8:   datatype_id = H5T_NATIVE_INT64;    break;
                case RecordObject::STRING:
                {
                    datatype_id = H5Tcopy(H5T_C_S1);
                    H5Tset_size(datatype_id, dataset.size);
                    H5Tset_strpad(datatype_id, H5T_STR_NULLTERM);
                    break;
                }
                default:
                {
                    mlog(CRITICAL, "Invalid scalar type supplied for %s: %d", dataset.name, dataset.data_type);
                    cleanup_stack();
                    return false;
                }
            }
            const hid_t dataset_id = H5Dcreate2(hid_stack.top(), dataset.name, datatype_id, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            const herr_t status = H5Dwrite(dataset_id, datatype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, dataset.data);
            if(dataset.data_type == RecordObject::STRING) H5Tclose(datatype_id);
            H5Sclose(dataspace_id);
            if(status < 0)
            {
                mlog(CRITICAL, "Failed to write scalar %s of size %ld and type %d", dataset.name, number_of_elements, dataset.data_type);
                cleanup_stack();
                return false;
            }
            hid_stack.push(dataset_id);
        }
        else if(dataset.dataset_type == ATTRIBUTE)
        {
            hid_t h5t; // hdf5 create
            switch(dataset.data_type)
            {
                case RecordObject::INT8:    h5t = H5T_NATIVE_INT8;     break;
                case RecordObject::INT16:   h5t = H5T_NATIVE_INT16;    break;
                case RecordObject::INT32:   h5t = H5T_NATIVE_INT32;    break;
                case RecordObject::INT64:   h5t = H5T_NATIVE_INT64;    break;
                case RecordObject::UINT8:   h5t = H5T_NATIVE_UINT8;    break;
                case RecordObject::UINT16:  h5t = H5T_NATIVE_UINT16;   break;
                case RecordObject::UINT32:  h5t = H5T_NATIVE_UINT32;   break;
                case RecordObject::UINT64:  h5t = H5T_NATIVE_UINT64;   break;
                case RecordObject::FLOAT:   h5t = H5T_NATIVE_FLOAT;    break;
                case RecordObject::DOUBLE:  h5t = H5T_NATIVE_DOUBLE;   break;
                case RecordObject::TIME8:   h5t = H5T_NATIVE_INT64;    break;
                case RecordObject::STRING:
                {
                    h5t = H5Tcopy(H5T_C_S1);
                    H5Tset_size(h5t, dataset.size);
                    H5Tset_strpad(h5t, H5T_STR_NULLTERM);  // Pad with null terminator
                    break;
                }
                default:
                {
                    mlog(CRITICAL, "Invalid atttribute type supplied for %s: %d", dataset.name, dataset.data_type);
                    cleanup_stack();
                    return false;
                }
            }
            const hid_t attr_dataspace_id = H5Screate(H5S_SCALAR);
            const hid_t attr_id = H5Acreate2(hid_stack.top(), dataset.name, h5t, attr_dataspace_id, H5P_DEFAULT, H5P_DEFAULT);
            H5Awrite(attr_id, h5t, dataset.data);
            H5Aclose(attr_id);
            H5Sclose(attr_dataspace_id);
            if(dataset.data_type == RecordObject::STRING) H5Tclose(h5t);
        }
        else if(dataset.dataset_type == PARENT)
        {
            close_hid(hid_stack.top());
            hid_stack.pop();
        }
    }
    cleanup_stack();
    return true;
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
HdfLib::info_t HdfLib::read (const char* filename, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows)
{
    info_t info;
    bool status = false;

    /* Start with Invalid Handles */
    const hid_t fapl = H5P_DEFAULT;
    hid_t file = INVALID_RC;
    hid_t dataset = INVALID_RC;
    hid_t memspace = H5S_ALL;
    hid_t dataspace = H5S_ALL;
    hid_t datatype = INVALID_RC;
    bool datatype_allocated = false;

    do
    {
        /* Open Filen */
        file = H5Fopen(filename, H5F_ACC_RDONLY, fapl);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open file: %s", filename);
            break;
        }

        /* Open Dataset */
        dataset = H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s", datasetname);
            break;
        }

        /* Open Dataspace */
        dataspace = H5Dget_space(dataset);
        if(dataspace < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s", datasetname);
            break;
        }

        /* Get Datatype */
        if(valtype == RecordObject::INTEGER)
        {
            datatype = H5T_NATIVE_INT;
        }
        else if(valtype == RecordObject::REAL)
        {
            datatype = H5T_NATIVE_DOUBLE;
        }
        else
        {
            datatype = H5Dget_type(dataset);
            datatype_allocated = true;
        }

        /* Get Type Size */
        const size_t typesize = H5Tget_size(datatype);

        /* Get Dimensions of Data */
        const int ndims = H5Sget_simple_extent_ndims(dataspace);
        hsize_t* dims = new hsize_t[ndims + 1];
        H5Sget_simple_extent_dims(dataspace, dims, NULL);

        /* Allocate Hyperspace Parameters */
        hsize_t* start = new hsize_t[ndims + 1];
        hsize_t* count = new hsize_t[ndims + 1];

        /* Readjust First Dimension */
        if(numrows != ALL_ROWS) dims[0] = MIN(numrows, (long)dims[0]);

        /* Create File Hyperspace to Read Selected Column */
        start[0] = startrow;
        start[1] = col;
        count[0] = dims[0];
        count[1] = 1;
        H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, NULL, count, NULL);

        /* Create Memory Hyperspace to Write Selected Column */
        dims[1] = 1; // readjust dimensions to reflect single column being read
        start[0] = 0; // readjust start to reflect writing from the beginning
        start[1] = 0; // readjust start to reflect writing to only a single column
        memspace = H5Screate_simple(ndims, dims, NULL);
        H5Sselect_hyperslab(memspace, H5S_SELECT_SET, start, NULL, count, NULL);

        /* Get Number of Elements */
        int elements = 1;
        for(int d = 0; d < ndims; d++)
        {
            elements *= (int32_t)dims[d];
        }

        /* Free Hyperspace Parameters */
        delete [] dims;
        delete [] start;
        delete [] count;

        /* Get Size of Data */
        const long datasize = elements * typesize;

        /* Allocate Data Buffer */
        uint8_t* data = NULL;
        try
        {
            data = new uint8_t[datasize];
        }
        catch (const std::bad_alloc& e)
        {
            mlog(CRITICAL, "Failed to allocate space for dataset: %d", elements);
            break;
        }

        /* Start Trace */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s %s", elements, datasize, filename, datasetname);
        const uint32_t parent_trace_id = EventLib::grabId();
        const uint32_t trace_id = start_trace(INFO, parent_trace_id, "HdfLib_read", "{\"filename\":\"%s\", \"dataset\":\"%s\"}", filename, datasetname);

        /* Read Dataset */
        if(H5Dread(dataset, datatype, memspace, dataspace, H5P_DEFAULT, data) >= 0)
        {
            /* Set Success */
            status = true;

            /* Set Info Return Structure */
            info.elements = elements;
            info.typesize = typesize;
            info.datasize = datasize;
            info.datatype = h5type2datatype(datatype, typesize);
            info.data = data;
        }
        else
        {
            /* Free Data and Log Error */
            mlog(CRITICAL, "Failed to read data from %s", datasetname);
            delete [] data;
        }

        /* Stop Trace */
        stop_trace(INFO, trace_id);
    }
    while(false);

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(datatype_allocated && datatype > 0) H5Tclose(datatype);
    if(dataspace != H5S_ALL) H5Sclose(dataspace);
    if(memspace != H5S_ALL) H5Sclose(memspace);
    if(dataset > 0) H5Dclose(dataset);

    /* Return Info */
    if(status)  return info;
    else        throw RunTimeException(CRITICAL, RTE_FAILURE, "HdfLib failed to read dataset");
}

/*----------------------------------------------------------------------------
 * h5type2datatype
 *----------------------------------------------------------------------------*/
RecordObject::fieldType_t HdfLib::h5type2datatype (int h5type, int typesize)
{
    if(h5type == H5T_NATIVE_INT)
    {
        if      (typesize == 1) return RecordObject::UINT8;
        else if (typesize == 2) return RecordObject::UINT16;
        else if (typesize == 4) return RecordObject::UINT32;
        else if (typesize == 8) return RecordObject::UINT64;
    }
    else if(h5type == H5T_NATIVE_DOUBLE)
    {
        if      (typesize == 4) return RecordObject::FLOAT;
        else if (typesize == 8) return RecordObject::DOUBLE;
    }

    return RecordObject::INVALID_FIELD;
}