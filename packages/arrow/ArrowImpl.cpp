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

/*
 * The order of the columns in the parquet file are:
 *  - Fields from the primary record
 *  - Geometry
 *  - Ancillary fields
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "ArrowImpl.h"


/*----------------------------------------------------------------------------
* swap_double - local utility fucntion
*----------------------------------------------------------------------------*/
static double swap_double(const double value)
{
    union {
        uint64_t i;
        double   d;
    } conv;

    conv.d = value;
    conv.i = __bswap_64(conv.i);
    return conv.d;
}


/*----------------------------------------------------------------------------
* convertWKBToPoint
*----------------------------------------------------------------------------*/
OGRPoint convertWKBToPoint(const std::string& wkb_data)
{
    wkbpoint_t point;

    if(wkb_data.size() < sizeof(wkbpoint_t))
    {
        throw std::runtime_error("Invalid WKB data size.");
    }

    // Byte order is the first byte
    size_t offset = 0;
    std::memcpy(&point.byteOrder, wkb_data.data() + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);

    // Next four bytes are wkbType
    std::memcpy(&point.wkbType, wkb_data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Convert to host byte order if necessary
    if(point.byteOrder == 0)
    {
        // Big endian
        point.wkbType = be32toh(point.wkbType);
    }
    else if(point.byteOrder == 1)
    {
        // Little endian
        point.wkbType = le32toh(point.wkbType);
    }
    else
    {
        throw std::runtime_error("Unknown byte order.");
    }

    // Next eight bytes are x coordinate
    std::memcpy(&point.x, wkb_data.data() + offset, sizeof(double));
    offset += sizeof(double);

    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
       (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.x = swap_double(point.x);
    }

    // Next eight bytes are y coordinate
    std::memcpy(&point.y, wkb_data.data() + offset, sizeof(double));
    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
       (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.y = swap_double(point.y);
    }

    OGRPoint poi(point.x, point.y, 0);
    return poi;
}

/*----------------------------------------------------------------------------
* parquetFileToTable
*----------------------------------------------------------------------------*/
std::shared_ptr<arrow::Table> parquetFileToTable(const char* file_path)
{
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));

    std::shared_ptr<arrow::Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

    return table;
}

/*----------------------------------------------------------------------------
* tableToParquetFile
*----------------------------------------------------------------------------*/
void tableToParquetFile(std::shared_ptr<arrow::Table> table, const char* file_path)
{
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(file_path));

    /* Create a Parquet writer properties builder */
    parquet::WriterProperties::Builder writer_props_builder;
    writer_props_builder.compression(parquet::Compression::SNAPPY);
    writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
    shared_ptr<parquet::WriterProperties> writer_properties = writer_props_builder.build();

    /* Create an Arrow writer properties builder to specify that we want to store Arrow schema */
    auto arrow_properties = parquet::ArrowWriterProperties::Builder().store_schema()->build();

    PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, table->num_rows(), writer_properties, arrow_properties));
}

/*----------------------------------------------------------------------------
* printParquetMetadata
*----------------------------------------------------------------------------*/
void printParquetMetadata(const char* file_path)
{
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));

    std::shared_ptr<parquet::FileMetaData> file_metadata = reader->parquet_reader()->metadata();
    std::cout << "***********************************************************" << std::endl;
    std::cout << "***********************************************************" << std::endl;
    std::cout << "***********************************************************" << std::endl;
    std::cout << "File Metadata:" << std::endl;
    std::cout << "  Version: " << file_metadata->version() << std::endl;
    std::cout << "  Num Row Groups: " << file_metadata->num_row_groups() << std::endl;
    std::cout << "  Num Columns: " << file_metadata->num_columns() << std::endl;
    std::cout << "  Num Row Groups: " << file_metadata->num_row_groups() << std::endl;
    std::cout << "  Num Rows: " << file_metadata->num_rows() << std::endl;
    std::cout << "  Created By: " << file_metadata->created_by() << std::endl;
    std::cout << "  Key Value Metadata:" << std::endl;
    for(int i = 0; i < file_metadata->key_value_metadata()->size(); i++)
    {
        std::string key = file_metadata->key_value_metadata()->key(i);
        std::string value = file_metadata->key_value_metadata()->value(i);

        if(key == "ARROW:schema") continue;
        std::cout << "    " << key << ": " << value << std::endl;
    }

    std::cout << "  Schema:" << std::endl;
    for(int i = 0; i < file_metadata->num_columns(); i++)
    {
        std::shared_ptr<parquet::schema::ColumnPath> path = file_metadata->schema()->Column(i)->path();
        std::cout << "    " << path->ToDotString() << std::endl;
    }
}
