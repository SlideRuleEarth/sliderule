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

#include "ReportDispatch.h"
#include "MetricRecord.h"
#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define REPORT_SPACE ""

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ReportDispatch::LuaMetaName = "ReportDispatch";
const struct luaL_Reg ReportDispatch::LuaMetaTable[] = {
    {"idxdisplay",  luaSetIndexDisplay},
    {"flushrow",    luaFlushRow},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS - REPORT DISPATCH
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate
 *
 *   <CSV|JSON> <filename(s)> [<buffer size>] [<field name table>]
 *
 *  where <filename(s)> is name of the file to be written or a list of regular
 *  expressions of filenames to be read from.  If being written, the filename is
 *  used as provided up to the max file size, and then after that new files are
 *  created with a .x extension appended to the end of the filename, where x is
 *  an incrementing number that starts with 2.  Note that STDOUT, STDERR, and STDIN
 *  are all supported filenames that refer to standard output, error, and input
 *  respectively.
 *
 *----------------------------------------------------------------------------*/
int ReportDispatch::luaCreate (lua_State* L)
{
    const char** columns = NULL;
    int num_results = 0;

    try
    {
        const char* format_str      = getLuaString(L, 1);
        const char* out_file_str    = getLuaString(L, 2);
        long        buffer_size     = getLuaInteger(L, 3, true, 0);

        /* Parse Metric Format */
        format_t file_format = str2format(format_str);
        if(file_format == INVALID_FORMAT)
        {
            mlog(CRITICAL, "Invalid file format provided: %s", format_str);
            throw RunTimeException(CRITICAL, RTE_ERROR, "parameter error");
        }

        /* Check Buffer Size */
        if(buffer_size < 0)
        {
            mlog(CRITICAL, "Invalid size provided for buffer: %ld", buffer_size);
            throw RunTimeException(CRITICAL, RTE_ERROR, "parameter error");
        }

        /* Parse Header Columns */
        int num_columns = lua_rawlen(L,4);
        if(lua_istable(L, 4) && num_columns > 0)
        {
            columns = new const char* [num_columns];
            for(int i = 0; i < num_columns; i++)
            {
                lua_rawgeti(L, 4, i+1);
                columns[i] = getLuaString(L, -1);
            }
        }

        /* Create Report Dispatch */
        num_results = createLuaObject(L, new ReportDispatch(L, out_file_str, file_format, buffer_size, columns, num_columns));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        num_results = returnLuaStatus(L, false);
    }

    /* Clean Up and Return */
    if(columns) delete [] columns;
    return num_results;
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * str2format
 *----------------------------------------------------------------------------*/
ReportDispatch::format_t ReportDispatch::str2format (const char* str)
{
         if(StringLib::match(str, "CSV"))   return CSV;
    else if(StringLib::match(str, "JSON"))  return JSON;
    else                                    return INVALID_FORMAT;
}

/*----------------------------------------------------------------------------
 * format2str
 *----------------------------------------------------------------------------*/
const char* ReportDispatch::format2str (format_t _format)
{
         if(_format == CSV)     return "CSV";
    else if(_format == JSON)    return "JSON";
    else                        return "INVALID";
}

/*----------------------------------------------------------------------------
 * str2display
 *----------------------------------------------------------------------------*/
ReportDispatch::indexDisplay_t ReportDispatch::str2display(const char* str)
{
         if(StringLib::match(str, "INT"))   return INT_DISPLAY;
    else if(StringLib::match(str, "GMT"))   return GMT_DISPLAY;
    else                                    return INVALID_DISPLAY;
}

/*----------------------------------------------------------------------------
 * display2str
 *----------------------------------------------------------------------------*/
const char* ReportDispatch::display2str(indexDisplay_t _display)
{
         if(_display == INT_DISPLAY)    return "INT";
    else if(_display == GMT_DISPLAY)    return "GMT";
    else                                return "INVALID";
}

/******************************************************************************
 * PUBLIC METHODS - REPORT FILE
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - ReportFile
 *----------------------------------------------------------------------------*/
ReportDispatch::ReportFile::ReportFile(lua_State* L, const char* _filename, format_t _format):
    File(L, _filename, File::TEXT, File::WRITER, File::FLUSHED),
    format(_format),
    values(Dictionary<char*>::DEFAULT_HASH_TABLE_SIZE, Dictionary<char*>::DEFAULT_HASH_TABLE_LOAD)
{
    headerInProgress = false;
    indexDisplay = INT_DISPLAY;
}

/*----------------------------------------------------------------------------
 * writeFileHeader
 *----------------------------------------------------------------------------*/
int ReportDispatch::ReportFile::writeFileHeader (void)
{
    if(headerInProgress) return 0;

    if(format == CSV)
    {
        /* Build Header String */
        SafeString header("Index");
        const char* column = values.first(NULL);
        while(column)
        {
            header += ",";
            header += column;
            column = values.next(NULL);
        }
        header += "\n";

        /* Write Header String */
        headerInProgress = true;
        int status = File::writeBuffer(header.str(false), header.length());
        headerInProgress = false;
        return status;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * writeFileData
 *----------------------------------------------------------------------------*/
int ReportDispatch::ReportFile::writeFileData (void)
{
    if(format == CSV)
    {
        /* Format Index Display */
        char index_str[MAX_INDEX_STR_SIZE];
        if(indexDisplay == ReportDispatch::INT_DISPLAY)
        {
            StringLib::format(index_str, MAX_INDEX_STR_SIZE, "%llu", (unsigned long long)index);
        }
        else if(indexDisplay == ReportDispatch::GMT_DISPLAY)
        {
            TimeLib::gmt_time_t index_time = TimeLib::gps2gmttime(index);
            StringLib::format(index_str, MAX_INDEX_STR_SIZE, "%d:%d:%d:%d:%d:%d", index_time.year, index_time.doy, index_time.hour, index_time.minute, index_time.second, index_time.millisecond);
        }
        else
        {
            index_str[0] = '\0';
        }

        /* Build Row String and Clear Values */
        SafeString row;
        row += index_str;
        const char* value = NULL;
        const char* column = values.first(&value);
        while(column)
        {
            row += ",";
            if(value)   row += value;
            const char* space = StringLib::duplicate(REPORT_SPACE);
            values.add(column, space);
            column = values.next(&value);
        }
        row += "\n";

        /* Write Row String */
        return File::writeBuffer(row.str(false), row.length());
    }
    else if(format == JSON)
    {
        /* Build JSON String */
        SafeString json("{\n");
        const char* value = NULL;
        const char* column = values.first(&value);
        while(column)
        {
            json += "\t\"";
            json += column;
            json += "\": \"";
            if(value) json += value;
            json += "\"";
            const char* space = StringLib::duplicate(REPORT_SPACE);
            values.add(column, space);
            column = values.next(&value);
            if(column)  json += ",\n";
            else        json += "\n}";
        }

        /* Write Row String */
        return File::writeBuffer(json.str(false), json.length());
    }

    return 0;
}

/******************************************************************************
 * PRIVATE METHODS - REPORT DISAPTCH
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - ReportDispatch
 *----------------------------------------------------------------------------*/
ReportDispatch::ReportDispatch (lua_State* L, const char* _filename, format_t _format, int buffer, const char** columns, int num_columns):
    DispatchObject(L, LuaMetaName, LuaMetaTable),
    report(L ,_filename, _format)
{
    /* Define Metric Record */
    RecordObject::defineRecord(MetricRecord::rec_type, NULL, sizeof(MetricRecord::metric_t), MetricRecord::rec_def, MetricRecord::rec_elem);

    /* Initialize Attributes */
    lastIndex = INVALID_KEY;
    fixedHeader = false;
    writeHeader = false;
    reportError = true;

    /* Initialize Entry Ordering */
    if(buffer > 0)  entries = new MgOrdering<entry_t*>(ReportDispatch::postEntry, this, buffer);
    else            entries = NULL;

    /* Populate Fixed Header If Provided */
    if(columns)
    {
        fixedHeader = true;
        for(int i = 0; i < num_columns; i++)
        {
            const char* space = StringLib::duplicate(REPORT_SPACE);
            report.values.add(columns[i], space);
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ReportDispatch::~ReportDispatch (void)
{
    if(entries) delete entries;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool ReportDispatch::processRecord (RecordObject* record, okey_t key)
{
    okey_t index = key;

    /* Sanity Check */
    if(!StringLib::match(record->getRecordType(), MetricRecord::rec_type))
    {
        if(reportError) mlog(CRITICAL, "%s incorrect record type provided to report: %s", ObjectType, record->getRecordType());
        reportError = false;
        return false;
    }

    /* Get Fields */
    const char* name = NULL;
    const char* value = NULL;
    try
    {
        name    = record->getValueText(record->getField("NAME"));
        value   = record->getValueText(record->getField("TEXT"));
        if(!name || !value) throw RunTimeException(CRITICAL, RTE_ERROR, "received incomplete metric");
    }
    catch(const RunTimeException& e)
    {
        if(reportError) mlog(e.level(), "%s failed to retrieve fields of record %s: %s", ObjectType, MetricRecord::rec_type, e.what());
        reportError = false;
        return false;
    }

    /* Create and Add/Post Entry */
    bool status = true;
    reportMut.lock();
    {
        entry_t* entry = new entry_t(index, name, value);
        if(entries) status = entries->add(index, entry);
        else        status = (ReportDispatch::postEntry(&entry, sizeof(entry_t*), this) > 0);
        if(!status) delete entry;
    }
    reportMut.unlock();

    /* Check and Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * postEntry
 *
 *  Notes: Called from MgOrdering or directly from processRecord
 *----------------------------------------------------------------------------*/
int ReportDispatch::postEntry(void* data, int size, void* parm)
{
    ReportDispatch* dispatch = (ReportDispatch*)parm;
    int status = size;
    entry_t* entry = *(entry_t**)data;
    okey_t index = entry->index;
    const char* name = entry->name;
    const char* value = StringLib::duplicate(entry->value);

    /* Flush Row on New Index */
    if(dispatch->lastIndex != index && dispatch->lastIndex != INVALID_KEY)
    {
        dispatch->flushRow();
    }

    /* Update Indexes */
    dispatch->lastIndex = index;
    dispatch->report.index = index;

    /* Update Value */
    if(dispatch->fixedHeader)
    {
        if(dispatch->report.values.find(name))
        {
            dispatch->report.values.add(name, value);
        }
    }
    else // dynamic header
    {
        int prev_num_values = dispatch->report.values.length();
        dispatch->report.values.add(name, value);
        if(dispatch->report.values.length() != prev_num_values)
        {
            dispatch->writeHeader = true;
        }
    }

    /* Set Error Reporting Back to True */
    dispatch->reportError = true;

    /* Free Entry and Return Status */
    if(status > 0) delete entry;
    return status;
}

/*----------------------------------------------------------------------------
 * flushRow
 *
 *  Notes: Must be called inside reportMut lock
 *----------------------------------------------------------------------------*/
bool ReportDispatch::flushRow(void)
{
    /* Write Header */
    if(writeHeader)
    {
        writeHeader = false;
        int hdr_written = report.writeFileHeader();
        if(hdr_written < 0)
        {
            if(reportError) mlog(CRITICAL, "%s failed to write file header", ObjectType);
            reportError = false;
            return false;
        }
    }

    /* Write Data */
    int row_written = report.writeFileData();
    if(row_written < 0)
    {
        if(reportError) mlog(CRITICAL, "%s failed to write file data", ObjectType);
        reportError = false;
        return false;
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * luaSetIndexDisplay - :idxdisplay(<display setting - "INT"|"GMT">)
 *----------------------------------------------------------------------------*/
int ReportDispatch::luaSetIndexDisplay(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        ReportDispatch* lua_obj = (ReportDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* display_str = getLuaString(L, 2);

        /* Convert String to Display Type */
        indexDisplay_t display = str2display(display_str);
        if(display == INVALID_DISPLAY)
        {
            mlog(CRITICAL, "Invalid index display selected: %s", display_str);
            throw RunTimeException(CRITICAL, RTE_ERROR, "parameter error");
        }

        /* Set Display Type */
        lua_obj->report.indexDisplay = display;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error configuring display: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaFlushRow - :flushrow([<scope - "ALL">])
 *----------------------------------------------------------------------------*/
int ReportDispatch::luaFlushRow(lua_State* L)
{
    bool status = false;
    bool flush_all = false;

    try
    {
        /* Get Self */
        ReportDispatch* lua_obj = (ReportDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* scope_str = getLuaString(L, 2, true, "ROW", &flush_all);

        /* Convert String to Scope */
        if(StringLib::match(scope_str, "ALL"))
        {
            flush_all = true;
        }

        /* Flush Entries */
        lua_obj->reportMut.lock();
        {
            lua_obj->reportError = true;
            if(flush_all && lua_obj->entries) lua_obj->entries->flush();
            lua_obj->flushRow();
            lua_obj->lastIndex = INVALID_KEY;
        }
        lua_obj->reportMut.unlock();

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error configuring display: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
