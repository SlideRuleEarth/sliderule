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

/*
 * TODO
 *  1. Handle Enumerations,  DiscreteConversions, Aliases, ExpressionConversions, ExpressionAlgorithm
 *  2. Add conversion property to field class
 *  3. Handle Mnemonics
 *  4. Sort Packets in alphabetical order
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "ItosRecordParser.h"

#include "ccsds.h"
#include "core.h"
#include "legacy.h"

#ifdef _LINUX_
#include <glob.h>
#endif

#include <float.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define HANDBOOK_PATH "../reports/handbook"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

using namespace Itos;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ItosRecordParser::TYPE = "ItosRecordParser";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * CommandableObject  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* ItosRecordParser::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    return new ItosRecordParser(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * getDictionary  -
 *----------------------------------------------------------------------------*/
MgDictionary<Record*>* ItosRecordParser::getDictionary(void)
{
    return &dictionary;
}

/*----------------------------------------------------------------------------
 * etPackets  -
 *----------------------------------------------------------------------------*/
MgList<Packet*>* ItosRecordParser::getPackets(void)
{
    return &packets;
}

/*----------------------------------------------------------------------------
 * kt2str  -
 *----------------------------------------------------------------------------*/
const char* ItosRecordParser::pkt2str(unsigned char* packet)
{
    #define PKT_STR_SIZE    1024

    int apid = CCSDS_GET_APID(packet);
    if(CCSDS_IS_CMD(packet))
    {
        int fc = CCSDS_GET_FC(packet);
        for(int p = 0; p < cmdPackets[apid].length(); p++)
        {
            CommandPacket* command_packet = (CommandPacket*)cmdPackets[apid][p];
            const char* fc_str = command_packet->getProperty(CommandPacket::fcDesignation, "value", 0);
            long tmpfc = -1;
            StringLib::str2long(fc_str, &tmpfc);
            if(fc == tmpfc)
            {
                if(command_packet->populate(packet))
                {
                    const char* command_str = command_packet->serialize(Packet::READABLE_FMT, PKT_STR_SIZE);
                    return command_str;
                }
            }
        }
    }
    else if(CCSDS_IS_TLM(packet))
    {
        for(int p = 0; p < tlmPackets[apid].length(); p++)
        {
            TelemetryPacket* telemetry_packet = (TelemetryPacket*)tlmPackets[apid][p];
            if(telemetry_packet->populate(packet))
            {
                // TODO: parse the APPLY WHEN list ... should go down into populate function
                const char* telemetry_str = telemetry_packet->serialize(Packet::READABLE_FMT, PKT_STR_SIZE);
                return telemetry_str;
            }
        }
    }

    mlog(ERROR, "Invalid packet detected!\n");
    return NULL;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
ItosRecordParser::ItosRecordParser(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Initialize Options */
    optFullPktDetails = false;
    optUserEditable = false;
    optRemoteContent = false;

    /* Register Commands */
    registerCommand("LOAD_REC_FILES",        (cmdFunc_t)&ItosRecordParser::loadRecFilesCmd,    -1, "<regular expression>");
    registerCommand("LOAD_FILTER_TBL",       (cmdFunc_t)&ItosRecordParser::loadFilterTblCmd,    1, "<path to filter tablen>");
    registerCommand("APPLY_FILTER_TBL",      (cmdFunc_t)&ItosRecordParser::applyFilterTblCmd,   0, "");
    registerCommand("SET_DESIGNATIONS",      (cmdFunc_t)&ItosRecordParser::setDesignationsCmd,  3, "<command apid designation> <command fc designation> <telemetry apid designation>");
    registerCommand("BUILD_DATABASE",        (cmdFunc_t)&ItosRecordParser::buildDatabaseCmd,    0, "");
    registerCommand("BUILD_RECORDS",         (cmdFunc_t)&ItosRecordParser::buildRecordsCmd,     0, "");
    registerCommand("DATASRV_EXPORT",        (cmdFunc_t)&ItosRecordParser::datasrvExportCmd,    3, "<db version> <data filename> <calibration filename>");
    registerCommand("PRINT_TOKENS",          (cmdFunc_t)&ItosRecordParser::printTokensCmd,      0, "");
    registerCommand("PRINT_KEYS",            (cmdFunc_t)&ItosRecordParser::printKeysCmd,        0, "");
    registerCommand("PRINT_PACKETS",         (cmdFunc_t)&ItosRecordParser::printPacketsCmd,     0, "");
    registerCommand("PRINT_FILTERS",         (cmdFunc_t)&ItosRecordParser::printFiltersCmd,     0, "");
    registerCommand("GENERATE_REPORT",       (cmdFunc_t)&ItosRecordParser::generateReportCmd,   3, "<report template filename> <summary template name> <output path prefix>");
    registerCommand("GENERATE_DOCUMENTS",    (cmdFunc_t)&ItosRecordParser::generateDocsCmd,     2, "<document template name> <output path directory>");
    registerCommand("REPORT_FULL_DETAILS",   (cmdFunc_t)&ItosRecordParser::reportFullCmd,       1, "<ENABLE|DISABLE>");
    registerCommand("REPORT_USER_EDITABLE",  (cmdFunc_t)&ItosRecordParser::makeEditableCmd,     1, "<ENABLE|DISABLE>");
    registerCommand("REPORT_REMOTE_CONTENT", (cmdFunc_t)&ItosRecordParser::useRemoteContentCmd, 1, "<ENABLE|DISABLE>");
    registerCommand("LIST",                  (cmdFunc_t)&ItosRecordParser::listCmd,             1, "<packet name>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ItosRecordParser::~ItosRecordParser(void)
{
}

/*----------------------------------------------------------------------------
 * readFile  -
 *----------------------------------------------------------------------------*/
SafeString* ItosRecordParser::readFile(const char* fname)
{
    /* Open File */
    FILE* fp = fopen(fname, "r");
    if(fp == NULL)
    {
        mlog(ERROR, "unable to open file %s due to error: %s\n", fname, strerror(errno));
        return NULL;
    }

    /* Get File Size */
    fseek(fp, 0L, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    /* Allocate Contents */
    SafeString* fcontents = new SafeString(fsize);

    /* Read In Contents */
    int c = 0;
    while((c = fgetc(fp)) != EOF)
    {
        fcontents->appendChar(c);
    }

    /* Close File */
    fclose(fp);
    fp = NULL;

    return fcontents;
}

/*----------------------------------------------------------------------------
 * parseFilterTbl  -
 *----------------------------------------------------------------------------*/
bool ItosRecordParser::parseFilterTbl(SafeString* fcontents)
{
    if(fcontents == NULL) return false;

    List<SafeString>* dynlines = fcontents->split('\n');
    List<SafeString>& lines = *dynlines; // alias
    for(int l = 0; l < lines.length(); l++)
    {
        mlog(DEBUG, "PARSING: %s\n", lines[l].getString());
        SafeString line("%s", lines[l].getString());
        List<SafeString>* dynatoms = line.split(' ');
        List<SafeString>& atoms = *dynatoms;

        const char* start_str = atoms[0].getString();
        if(start_str[0] == '!')
        {
            continue;
        }

        long            q           = 0; // atoms[0]
        long            spw         = 0; // atoms[1]
        const char*     fsw_define  = atoms[3].getString();
        long            sid         = 0; // atoms[4]
        double          rate        = 0; // atoms[5]
        const char*     type        = atoms[6].getString();
        const char*     sender      = atoms[7].getString();
        const char*     task        = atoms[8].getString();
        const char**    sources     = NULL;

        StringLib::str2long(atoms[0].getString(),   &q);
        StringLib::str2long(atoms[1].getString(),   &spw);
        StringLib::str2long(atoms[4].getString(),   &sid);
        StringLib::str2double(atoms[5].getString(), &rate);

        int num_sources = atoms.length() - 9;
        if(num_sources > 0)
        {
            sources = new const char * [num_sources + 1];
            for(int a = 0; a < num_sources; a++)
            {
                sources[a] = atoms[a + 9].getString(true);
            }
            sources[num_sources] = NULL;
        }

        Filter* f = new Filter(q, spw, fsw_define, sid, rate, type, sender, task, sources);
        filters.add(f);

        if(num_sources > 0)
        {
            for(int a = 0; a < num_sources; a++)
            {
                delete [] sources[a];
            }
            delete [] sources;
        }

        delete dynatoms;
    }

    delete dynlines;

    return true;
}

/*----------------------------------------------------------------------------
 * parseRecTokens  -
 *----------------------------------------------------------------------------*/
bool ItosRecordParser::parseRecTokens(SafeString* fcontents)
{
    if(fcontents == NULL) return false;

    char token[Record::MAX_TOKEN_SIZE];
    long tindex = 0;
    long findex = 0;
    long fsize = fcontents->getLength();
    bool offset_hack = false;

    #define FROMEND(c) ((findex) < ((fsize) - (c)))

    while(findex < fsize)
    {
        /* Consume Block Comments */
        if(FROMEND(1) && (*fcontents)[findex] == '/' && (*fcontents)[findex+1] == '*')
        {
            /* Capture Preceeding Record Comments */
            if(FROMEND(5) && (*fcontents)[findex] == '/' && (*fcontents)[findex+1] == '*' && (*fcontents)[findex+2] == '*' &&
                             ( ((*fcontents)[findex+3] == '<' && (*fcontents)[findex+4] == '-' && (*fcontents)[findex+5] == '-') ||
                               ((*fcontents)[findex+3] == '-' && (*fcontents)[findex+4] == '-' && (*fcontents)[findex+5] == '>') ) )
            {
                memset(token, 0, Record::MAX_TOKEN_SIZE); // null terminates string
                tindex = 0;
                findex += 3; // skip over start of comment to <-- or -->
                while(FROMEND(2))
                {
                    if((*fcontents)[findex] == '*' && (*fcontents)[findex+1] == '/')
                    {
                        findex += 2;
                        break;
                    }
                    else
                    {
                        token[tindex++] = (*fcontents)[findex++];
                    }
                }
                SafeString ss("%s", token);
                if(ss.getLength() > 1) tokens.add(ss);
            }
            else
            {
                /* None-Record Comments */
                while(FROMEND(2))
                {
                    findex++;
                    if((*fcontents)[findex] == '*' && (*fcontents)[findex+1] == '/')
                    {
                        findex += 2;
                        break;
                    }
                }
            }
        }
        /* Consume Line Comments */
        else if(FROMEND(1) && (*fcontents)[findex] == '/' && (*fcontents)[findex+1] == '/')
        {
            while(findex < fsize)
            {
                if((*fcontents)[findex++] == '\n') break;
            }
        }
        /* Consume White Space */
        else if(isspace((*fcontents)[findex]))
        {
            while(FROMEND(0) && isspace((*fcontents)[findex]))
            {
                findex++;
            }
        }
        /* Create Token */
        else
        {
            /* Initialize Token */
            memset(token, 0, Record::MAX_TOKEN_SIZE); // null terminates string
            tindex = 0;

            /* Single Character Tokens */
            if( ((*fcontents)[findex] == '{') ||
                ((*fcontents)[findex] == '}') ||
                ((*fcontents)[findex] == '=') )
            {
                token[tindex++] = (*fcontents)[findex++];
            }
            /* Double Character Tokens */
            else if(FROMEND(1) && ((*fcontents)[findex] == '+') && ((*fcontents)[findex+1] == '=') )
            {
                token[tindex++] = (*fcontents)[findex++];
                token[tindex++] = (*fcontents)[findex++];
            }
            /* String Tokens */
            else if((*fcontents)[findex] == '"')
            {
                token[tindex++] = (*fcontents)[findex++]; // get first string character
                while(FROMEND(0) && ((*fcontents)[findex] != '"'))
                {
                    token[tindex++] = (*fcontents)[findex++];
                }

                if(FROMEND(0))
                {
                    token[tindex++] = (*fcontents)[findex++]; // get last string character
                }
                else
                {
                    mlog(ERROR, "Could not find closing string!\n");
                }
            }
            /* Everything Else */
            else
            {
                while(FROMEND(0) && !isspace((*fcontents)[findex]) && ((*fcontents)[findex] != '"') && ((*fcontents)[findex] != '=') && ((*fcontents)[findex] != '{') && ((*fcontents)[findex] != '}') && ((*fcontents)[findex] != '+'))
                {
                    if((*fcontents)[findex] == ',')
                    {
                        if(offset_hack == false)    findex++; // drop commas
                        else                        offset_hack = false;
                        break; // treat what has been read thus far as the first token
                    }
                    else
                    {
                        token[tindex++] = (*fcontents)[findex++];
                    }
                }
            }

            /* Add Token to List */
            if(tindex > 0)
            {
                if(strcmp(token, "offset") == 0)
                {
                    offset_hack = true;
                }

                SafeString ss("%s", token);
                if(ss.getLength() > 1) tokens.add(ss);
            }
            else
            {
                mlog(DEBUG, "Null token (%c) detected at offset %ld of %ld\n", (*fcontents)[findex], findex, fsize);
            }
        }
    }

    return true;
}

/*----------------------------------------------------------------------------
 * isStr  -
 *----------------------------------------------------------------------------*/
bool ItosRecordParser::isStr (int i, const char* str)
{
    if(i < tokens.length())
    {
        SafeString& tok = tokens[i];
        if(strncmp(tok.getString(), str, Record::MAX_TOKEN_SIZE) == 0)
        {
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * startStr  -
 *----------------------------------------------------------------------------*/
bool ItosRecordParser::startStr (int i, const char* str)
{
    if(i < tokens.length())
    {
        SafeString& tok = tokens[i];
        if(strstr(tok.getString(), str) != NULL)
        {
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * checkComment  -
 *----------------------------------------------------------------------------*/
bool ItosRecordParser::checkComment (int* c_index, Record* c_record, int* index)
{
    if(startStr(*index , "<--"))
    {
        if(c_record != NULL)
        {
            c_record->setComment(tokens[*index].getString());
            c_record = NULL;
        }
        else
        {
            mlog(ERROR, "Unable to find record to associate to comment [%s]\n", tokens[*index].getString());
        }
        return true;
    }
    else if(startStr(*index , "-->"))
    {
        *c_index = *index;
        return true;
    }
    return false;
}

/*----------------------------------------------------------------------------
 * createRecords  -
 *----------------------------------------------------------------------------*/
Record* ItosRecordParser::createRecord (Record* container, int* index)
{
    #define INVALID_COMMENT_INDEX -1
    static int      commentIndex    = INVALID_COMMENT_INDEX;
    static Record*  commentRecord   = NULL;

    Record* record          = NULL;
    char*   record_name     = NULL;
    char*   record_type     = NULL;
    bool    is_prototype    = false;
    bool    is_value        = false;
    int     bracket_level   = 0;

    /*Check for Record Comment */
    if(checkComment(&commentIndex, commentRecord, index))
    {
        (*index) += 1;
    }

    /* Set Prototype */
    if(container != NULL) is_prototype = container->isPrototype();

    /* If Value */
    if(isStr(*index + 1, "="))
    {
        record_type = (char*)"#";
        record_name = (char*)tokens[*index].getString();
        is_value = true;
        (*index) += 2; // move past "="
    }
    /* If Appended Value */
    else if(isStr(*index + 1, "+="))
    {
        record_type = (char*)"$";
        record_name = (char*)tokens[*index].getString();
        is_value = true;
        (*index) += 2; // move past "+="
    }
    /* Else If Redefinition */
    else if(isStr(*index + 1, "{"))
    {
        record_type = (char*)"@";
        record_name = (char*)tokens[*index].getString();
        (*index) += 1; // move index to "{"
    }
    /* Else Container */
    else
    {
        if(isStr(*index, "prototype"))
        {
            is_prototype = true;
            (*index)++; // move past "prototype"
        }

        record_type = (char*)tokens[(*index)++].getString();
        record_name = (char*)tokens[(*index)++].getString();
    }

    /* Create Record */
    char* dictionary_name = NULL;
    if(container != NULL)
    {
        int dictlen = (int)strlen(record_name) + (int)strlen(container->getName()) + 2;
        dictionary_name = new char[dictlen]; // null terminator and '.'
        snprintf(dictionary_name, dictlen, "%s.%s", container->getName(), record_name);
    }
    else
    {
        dictionary_name = StringLib::duplicate(record_name);
    }
    mlog(INFO, "Creating Record: %d %s %s\n", is_prototype, record_type, dictionary_name);
    record = new Record(is_prototype, record_type, dictionary_name);
    delete [] dictionary_name;

    /* Set Comment Record and Index */
    if(commentIndex != INVALID_COMMENT_INDEX)
    {
        record->setComment(tokens[commentIndex].getString());
        commentIndex = INVALID_COMMENT_INDEX;
    }

    /* Populate Record */
    do
    {
        if(isStr(*index, "{"))
        {
            bracket_level++;
        }
        else if(isStr(*index, "}"))
        {
            bracket_level--;
        }
        else if(is_value)
        {
            record->addValue((char*)tokens[*index].getString());
        }
        else if(!checkComment(&commentIndex, commentRecord, index))
        {
            Record* subrecord = createRecord(record, index);
            if(subrecord) record->addSubRecord(subrecord);
        }

        (*index)++;

    } while(*index < tokens.length() && bracket_level > 0);

    /* Add Record to Dictionary and Return Record */
    (*index)--; // whatever terminated current record is needed for parent record
    dictionary.add(record->getName(), record);
    commentRecord = record;
    return record;
}

/*----------------------------------------------------------------------------
 * createRecords  -
 *
 *   Notes: parent function for recursive createRecord(..)
 *----------------------------------------------------------------------------*/
void ItosRecordParser::createRecords (void)
{
    int i = 0;
    while(i < tokens.length())
    {
        Record* record = createRecord(NULL, &i);
        if(record) declarations.add(record);
        i++;
    }
}

/*----------------------------------------------------------------------------
 * populatePacket  -
 *----------------------------------------------------------------------------*/
void ItosRecordParser::populatePacket (Record* subrec, Packet* pkt, Record* conrec, int conindex)
{
    /* Check for Redefinition */
    if(subrec->isRedefinition())
    {
        for(int j = 0; j < subrec->getNumSubRecords(); j++)
        {
            Record* valrec = subrec->getSubRecord(j);
            if(valrec->isValue())
            {
                const char* property = valrec->getUnqualifiedName();
                for(int k = 0; k < valrec->getNumSubValues(); k++)
                {
                    const char* val = valrec->getSubValue(k);
                    pkt->setProperty(subrec->getUnqualifiedName(), property, val, k);
                }
            }
            else
            {
                mlog(WARNING, "Ignored subrecord <%s> of redifinition <%s>\n", valrec->getName(), subrec->getName());
            }
        }
    }
    else if(subrec->isValue())
    {
        /* Handle Special Case of applyWhen */
        if(strcmp(subrec->getUnqualifiedName(), "applyWhen") == 0)
        {
            char* field = NULL;
            char* range = NULL;
            bool field_set = false;
            bool range_set = false;
            int s = 0, n = subrec->getNumSubValues();
            while(s < n)
            {
                const char* val = subrec->getSubValue(s);

                /* Parse Statement */
                if(strcmp(val, "FieldInRange") == 0)
                {
                    if(n < s + 6) mlog(ERROR, "Invalid applyWhen statement - not enough subrecords following FieldInRange: %d\n", n - s);
                    s++; // ignore FieldInRange and move to next record
                }
                else if(strcmp(val, "field") == 0)
                {
                    if(n < s + 2)                                       mlog(ERROR, "Invalid applyWhen statement - not enough subrecords following field keyword: %d\n", n - s);
                    else if(strcmp(subrec->getSubValue(s+1), "=") != 0) mlog(ERROR, "Invalid applyWhen statement - field keyword not followed by equals sign\n");
                    else if(field_set == true)                          mlog(ERROR, "Invalid applyWhen statement - field is already set\n");
                    field = (char*)subrec->getSubValue(s + 2);
                    s += 3;
                    field_set = true;
                }
                else if(strcmp(val, "range") == 0)
                {
                    if(n < s + 2)                                       mlog(ERROR, "Invalid applyWhen statement - not enough subrecords following range keyword: %d\n", n - s);
                    else if(strcmp(subrec->getSubValue(s+1), "=") != 0) mlog(ERROR, "Invalid applyWhen statement - range keyword not followed by equals sign\n");
                    else if(range_set == true)                          mlog(ERROR, "Invalid applyWhen statement - range is already set\n");
                    range = (char*)subrec->getSubValue(s + 2);
                    s += 3;
                    range_set = true;
                }
                else
                {
                    mlog(WARNING, "Invalid applyWhen statement - unrecognized keyword: %s\n", val);
                    s++;
                }

                /* Set Packet Property */
                if(field_set && range_set)
                {
                    /* Reset State */
                    field_set = false;
                    range_set = false;

                    /* Set Individual Field in Packet Property*/
                    pkt->setProperty(field, "range", range, Field::UNINDEXED_PROP); // sets individual field property

                    /* Set Packet Property */
                    #define APPLY_WHEN_STR_MAX_SIZE 256
                    char apply_when_str[APPLY_WHEN_STR_MAX_SIZE];
                    snprintf(apply_when_str, APPLY_WHEN_STR_MAX_SIZE, "%s=%s", field, range);
                    pkt->setPktProperty("applyWhen", apply_when_str);

                    /* Reset Field and Range */
                    field = NULL;
                    range = NULL;
                }
            }
        }
        else if(subrec->getNumSubValues() == 1)
        {
            if(pkt->setPktProperty(subrec->getUnqualifiedName(), subrec->getSubValue(0)) == false)
            {
                mlog(ERROR, "Unable to set packet property: %s (%s) <-- %s\n", subrec->getName(), subrec->getUnqualifiedName(), subrec->getSubValue(0));
            }
        }
        else
        {
            mlog(ERROR, "Unhandled packet property: %s\n", subrec->getName());
        }
    }
    /* Add Fields to Packet */
    else
    {
        if      (subrec->isType("U1"))          pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 8,  true);
        else if (subrec->isType("U12"))         pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 16, true);
        else if (subrec->isType("U1234"))       pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 32, true);
        else if (subrec->isType("U12345678"))   pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 64, true);
        else if (subrec->isType("I1"))          pkt->addField(subrec, conrec, conindex, Field::INTEGER,  8,  true);
        else if (subrec->isType("I12"))         pkt->addField(subrec, conrec, conindex, Field::INTEGER,  16, true);
        else if (subrec->isType("I1234"))       pkt->addField(subrec, conrec, conindex, Field::INTEGER,  32, true);
        else if (subrec->isType("I12345678"))   pkt->addField(subrec, conrec, conindex, Field::INTEGER,  64, true);
        else if (subrec->isType("F1234"))       pkt->addField(subrec, conrec, conindex, Field::FLOAT,    32, true);
        else if (subrec->isType("F12345678"))   pkt->addField(subrec, conrec, conindex, Field::FLOAT,    64, true);
        else if (subrec->isType("U21"))         pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 16, false);
        else if (subrec->isType("U4321"))       pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 32, false);
        else if (subrec->isType("U87654321"))   pkt->addField(subrec, conrec, conindex, Field::UNSIGNED, 64, false);
        else if (subrec->isType("I21"))         pkt->addField(subrec, conrec, conindex, Field::INTEGER,  16, false);
        else if (subrec->isType("I4321"))       pkt->addField(subrec, conrec, conindex, Field::INTEGER,  32, false);
        else if (subrec->isType("I87654321"))   pkt->addField(subrec, conrec, conindex, Field::INTEGER,  64, false);
        else if (subrec->isType("F4321"))       pkt->addField(subrec, conrec, conindex, Field::FLOAT,    32, false);
        else if (subrec->isType("F87654321"))   pkt->addField(subrec, conrec, conindex, Field::FLOAT,    64, false);
        else if (subrec->isType("S1"))          pkt->addField(subrec, conrec, conindex, Field::STRING,   8,  true);
        else if (subrec->isType("VariableRaw")) {} // Perform Nothing
        else if (subrec->isType("U") ||
                 subrec->isType("I") ||
                 subrec->isType("F") ||
                 subrec->isType("S"))
        {
            mnemonics.add(subrec);
        }
        else
        {
            mlog(ERROR, "Unsupported type <%s> in record <%s>\n", subrec->getType(), subrec->getName());
        }
    }
}

/*----------------------------------------------------------------------------
 * createPacket  -
 *----------------------------------------------------------------------------*/
Packet* ItosRecordParser::createPacket (Record* declaration, Packet* pkt, Record** system_declaration, Record** struct_declaration, int struct_index)
{
    assert(declaration != NULL);

    /* Instantiate Packet */
    if(declaration->isType("atlasCmd"))
    {
        assert(pkt == NULL);
        pkt = new CommandPacket(CommandPacket::ATLAS);
    }
    else if(declaration->isType("atlasTlm"))
    {
        assert(pkt == NULL);
        pkt = new TelemetryPacket(TelemetryPacket::ATLAS);
    }
    else if(declaration->isType("CCSDSCommandPacket"))
    {
        assert(pkt == NULL);
        pkt = new CommandPacket(CommandPacket::STANDARD, false);
    }
    else if(declaration->isType("CCSDSTelemetryPacket"))
    {
        assert(pkt == NULL);
        pkt = new TelemetryPacket(TelemetryPacket::STANDARD);
    }
    else if(declaration->isType("Structure"))
    {
        // do nothing - skip to below
    }
    else if(declaration->isType("R0"))
    {
        // do nothing - skip to below
    }
    else if(declaration->isType("System"))
    {
        *system_declaration = declaration;
        for(int s = 0; s < declaration->getNumSubRecords(); s++)
        {
            Record* srec = declaration->getSubRecord(s);
            if(srec->isType("Structure") == false)
            {
                srec->setPrototype((*system_declaration)->isPrototype());

                mlog(DEBUG, "SYSTEM DECLARATION: %d, %s, %s\n", srec->isPrototype(), srec->getType(), srec->getName());
                assert(srec->isValue() == false);

                Packet* syspkt = createPacket(srec, NULL, system_declaration, struct_declaration, 0);
                if(syspkt != NULL) packets.add(syspkt);
            }
        }
        return NULL;
    }
    else if(declaration->isType("ExpressionAlgorithm"))
    {
        TypeConversion* expalgo = createConversion(TypeConversion::EXP_ALGO, declaration);
        if(expalgo != NULL) conversions.add(expalgo);
        return NULL;
    }
    else if(declaration->isType("ExpressionConversion"))
    {
        TypeConversion* expconv = createConversion(TypeConversion::EXP_CONV, declaration);
        if(expconv != NULL) conversions.add(expconv);
        return NULL;
    }
    else if(declaration->isType("PolynomialConversion"))
    {
        TypeConversion* plyconv = createConversion(TypeConversion::PLY_CONV, declaration);
        if(plyconv != NULL) conversions.add(plyconv);
        return NULL;
    }
    else if(declaration->isType("Enumeration"))
    {
        TypeConversion* cmdenum = createConversion(TypeConversion::CMD_ENUM, declaration);
        if(cmdenum != NULL) conversions.add(cmdenum);
        return NULL;
    }
    else if(declaration->isType("DiscreteConversion"))
    {
        TypeConversion* tlmconv = createConversion(TypeConversion::TLM_CONV, declaration);
        if(tlmconv != NULL) conversions.add(tlmconv);
        return NULL;
    }
    else if(declaration->isType("Alias"))
    {
        aliases.add(declaration);
        return NULL; // do no additional work for Alias at the moment
    }
    else if(declaration->isType("U") ||
            declaration->isType("I") ||
            declaration->isType("F") ||
            declaration->isType("S"))
    {
        if(!declaration->isPrototype())
        {
            mnemonics.add(declaration);
        }
        return NULL;
    }
    else /* Recurse on definition type */
    {
        Record* type_record = NULL;
        try
        {
            type_record = dictionary[declaration->getType()];
        }
        catch(std::out_of_range& e)
        {
            (void)e;
            if(strcmp(declaration->getType(), "LimitSet") == 0)
            {
                mlog(DEBUG, "LimitSet records are not currently supported, %s ignored\n", declaration->getName());
            }
            else
            {
                mlog(ERROR, "Type %s for record %s not found\n", declaration->getType(), declaration->getName());
            }
        }

        if(type_record == NULL) return NULL;
        mlog(DEBUG, "Recursing on type: %s for record %s\n", declaration->getType(), declaration->getName());
        pkt = createPacket(type_record, pkt, system_declaration, struct_declaration, 0);

    }

    /* Process Regular Packet */
    if(pkt != NULL)
    {
        /* Set Declaration */
        if(*struct_declaration == NULL)
        {
            pkt->setName(declaration->getName());
            pkt->setDeclaration(declaration);
        }

        /* Process Through Subrecords */
        for(int i = 0; i < declaration->getNumSubRecords(); i++)
        {
            Record* subrec = declaration->getSubRecord(i);
            Record* structrec = NULL;
            try { structrec = dictionary[subrec->getType()]; }
            catch(std::out_of_range& e) { (void)e; }

            /* Check For System Definition of Structure */
            if(structrec == NULL && (*system_declaration) != NULL)
            {
                char systype[Record::MAX_TOKEN_SIZE];
                snprintf(systype, Record::MAX_TOKEN_SIZE, "%s.%s", (*system_declaration)->getName(), subrec->getType());
                try { structrec = dictionary[systype]; }
                catch(std::out_of_range& e) { (void)e; }
            }

            /* Populate Packet */
            if(structrec != NULL)
            {
                *struct_declaration = subrec;
                for(int e = 0; e < subrec->getNumArrayElements(); e++)
                {
                    pkt = createPacket(structrec, pkt, system_declaration, struct_declaration, e);
                }
                *struct_declaration = NULL;
            }
            else
            {
                populatePacket(subrec, pkt, *struct_declaration, struct_index);
            }
        }
    }
    /* Process System Declaration */
    else if(*system_declaration != NULL)
    {
        const char* sysname = (*system_declaration)->getName();
        if(!instantiations.find(sysname))
        {
            declaration->getName();
            List<Record*>* instlist = new List<Record*>();
            instlist->add(declaration);
            instantiations.add(sysname, instlist);
        }
        else
        {
            List<Record*>* instlist = instantiations[sysname];
            instlist->add(declaration);
        }

        /* Process Sub Records */
        for(int s = 0; s < declaration->getNumSubRecords(); s++)
        {
            Record* instantiated_rec = declaration->getSubRecord(s);
            char* dotptr = strstr((char*)instantiated_rec->getName(), ".");
            char syspkt_name[Record::MAX_TOKEN_SIZE];
            snprintf(syspkt_name, Record::MAX_TOKEN_SIZE, "%s.%s", (*system_declaration)->getName(), dotptr + 1);

            mlog(DEBUG, "SYSTEM INSTANTIATION of %s from %s\n", instantiated_rec->getName(), syspkt_name);
            assert(instantiated_rec->isRedefinition() == true);

            /* Find Packet */
            Packet* orig_syspkt = findPacket(syspkt_name);
            if(orig_syspkt != NULL)
            {
                /* Create Copy of Packet Definition */
                Packet* syspkt = orig_syspkt->duplicate();

                /* Change Name */
                syspkt->setName(instantiated_rec->getName());

                /* Update Declaration */
                syspkt->setDeclaration(instantiated_rec);

                /* Update Fields */
                for(int j = 0; j < instantiated_rec->getNumSubRecords(); j++)
                {
                    Record* instantiated_subrec = instantiated_rec->getSubRecord(j);
                    populatePacket(instantiated_subrec, syspkt, *struct_declaration, 0);
                }

                /* Add Instantiated Packet to List of Packets */
                packets.add(syspkt);
            }
            else
            {
                mlog(WARNING, "Unable to find packet %s to instantiate\n", syspkt_name);
            }
        }
    }

    return pkt;
}

/*----------------------------------------------------------------------------
 * createPackets  -
 *----------------------------------------------------------------------------*/
void ItosRecordParser::createPackets (void)
{
    for(int r = 0; r < declarations.length(); r++)
    {
        Record* declaration = declarations[r];
        mlog(DEBUG, "DECLARATION: %d, %s, %s\n", declaration->isPrototype(), declaration->getType(), declaration->getName());
        assert(declaration->isValue() == false);
        Record* system = NULL;
        Record* structure = NULL;
        Packet* packet = createPacket(declaration, NULL, &system, &structure, 0);
        if(packet != NULL) packets.add(packet);
    }

    // Sort Packets
    for(int i = 1; i < packets.length(); i++)
    {
        Packet* tmp1 = packets[i];
        int j = i;
        while(j > 0)
        {
            Packet* tmp2 = packets[j - 1];
            if(strcmp(tmp2->getName(), tmp1->getName()) < 0)
            {
                break;
            }
            packets.set(j, tmp2, false);
            j--;
        }
        packets.set(j, tmp1, false);
    }
}

/*----------------------------------------------------------------------------
 * createMnemonics  -
 *----------------------------------------------------------------------------*/
void ItosRecordParser::createMnemonics (void)
{
    /* Transverse through Mnemonics */
    for(int u = 0; u < mnemonics.length(); u++)
    {
        Record* mnem = mnemonics[u];
        mlog(INFO, "Generating definition for mnemonic: %s\n", mnem->getName());

        /* Create Name List */
        List<const char*> namelist;
        bool instantiated = false;
        char* tmpname = (char*)mnem->getName();
        char* dotptr = strstr(tmpname, ".");
        if(dotptr)
        {
            *dotptr = '\0';
            if(instantiations.find(tmpname))
            {
                instantiated = true;
                List<Record*>* instlist = instantiations[tmpname];
                for(int i = 0; i < instlist->length(); i++)
                {
                    Record* instrec = instlist->get(i);
                    char newname[Record::MAX_TOKEN_SIZE];
                    snprintf(newname, Record::MAX_TOKEN_SIZE, "%s.%s", instrec->getName(), dotptr + 1);
                    const char* instname = StringLib::duplicate(newname);
                    namelist.add(instname);
                }
            }
        }

        /* Not Instantiated */
        if(!instantiated)
        {
            const char* instname = mnem->getName();
            namelist.add(instname);
        }

        /* Loop Through Names */
        for(int n = 0; n < namelist.length(); n++)
        {
            /* Initialize Mnemonic Definition */
            Mnemonic* def = new Mnemonic;
            def->name = namelist[n];
            def->type = NULL;
            def->source = NULL;
            def->source_packet = NULL;
            def->conversion = NULL;

            /* Populate Source and Conversion Definitions */
            for(int s = 0; s < mnem->getNumSubRecords(); s++)
            {
                Record* sub = mnem->getSubRecord(s);
                if(sub->isValue())
                {
                    if(strcmp(sub->getDisplayName(), "sourceFields") == 0)
                    {
                        SafeString sourceFields;
                        for(int sf = 0; sf < sub->getNumSubValues(); sf++)
                        {
                            sourceFields += sub->getSubValue(sf);
                        }
                        def->source = sourceFields.getString(true);
                    }
                    else if(strcmp(sub->getDisplayName(), "conversion") == 0)
                    {
                        const char* tcname = sub->getSubValue(0);
                        for(int c = 0; c < conversions.length(); c++)
                        {
                            TypeConversion* tc = conversions[c];
                            if(strcmp(tc->getName(), tcname) == 0)
                            {
                                def->conversion = tc;
                                break;
                            }
                        }
                    }
                    else if(strcmp(sub->getDisplayName(), "initialValue") == 0)
                    {
                        def->initial_value = StringLib::duplicate(sub->getSubValue(0));
                    }
                    else if(strcmp(sub->getDisplayName(), "limit") == 0)
                    {
                        mlog(DEBUG, "Limit subrecord <%s> of mnemonic <%s> is not supported\n", sub->getName(), mnem->getName());
                    }
                    else
                    {
                        mlog(ERROR, "Unrecognized subrecord <%s> of mnemonic <%s>\n", sub->getName(), mnem->getName());
                    }
                }
                else
                {
                    mlog(ERROR, "Ignored subrecord <%s> of mnemonic <%s>\n", sub->getName(), mnem->getName());
                }
            }

            /* Populate Type Definition */
                 if(strcmp(mnem->getType(), "U") == 0)  def->type = "Unsigned Integer";
            else if(strcmp(mnem->getType(), "I") == 0)  def->type = "Integer";
            else if(strcmp(mnem->getType(), "F") == 0)  def->type = "Floating Point";
            else if(strcmp(mnem->getType(), "S") == 0)  def->type = "String";
            else                                        def->type = mnem->getType();

            /*
             * Populate Source Packet
             *    reverse through dot notation looking for
             *    first match against a telemetry packet
             *    EXAMPLE SOURCE - handbook_A_PCE1_FM.HK.html#A_PCE1_FM.HK
             */
            if(def->source)
            {
                mlog(INFO, "Looking for source packet for field: %s\n", def->source);
                int l = strnlen(def->source, MAX_STR_SIZE);
                while(l > 0)
                {
                    char pkt_name[MAX_STR_SIZE];
                    while(l > 0 && def->source[l] != '.') l--;
                    if(l > 0)
                    {
                        LocalLib::copy(pkt_name, def->source, l);
                        pkt_name[l] = '\0';
                        l--; // move to next character
                        for(int p = 0; p < packets.length(); p++)
                        {
                            Packet* pkt = packets.get(p);
                            if(pkt->isType(Packet::TELEMETRY))
                            {
                                if(strcmp(pkt->getName(), pkt_name) == 0)
                                {
                                    def->source_packet = pkt->getName();
                                    l = 0; // breaks out of parent loops
                                    break; // breaks out of for loop through packets
                                }
                            }
                        }
                    }
                }
            }

            /* Try again with the name */
            if(def->source_packet == NULL)
            {
                mlog(INFO, "Looking for source packet for field: %s\n", def->name);
                int l = strnlen(def->name, MAX_STR_SIZE);
                while(l > 0)
                {
                    char pkt_name[MAX_STR_SIZE];
                    while(l > 0 && def->name[l] != '.') l--;
                    if(l > 0)
                    {
                        LocalLib::copy(pkt_name, def->name, l);
                        pkt_name[l] = '\0';
                        l--; // move to next character
                        for(int p = 0; p < packets.length(); p++)
                        {
                            Packet* pkt = packets.get(p);
                            if(pkt->isType(Packet::TELEMETRY))
                            {
                                if(strcmp(pkt->getName(), pkt_name) == 0)
                                {
                                    def->source_packet = pkt->getName();
                                    l = 0; // breaks out of parent loops
                                    break; // breaks out of for loop through packets
                                }
                            }
                        }
                    }
                }
            }

            /* Add Mnemonic to Definition List */
            mneDefinitions.add(def);
        }
    }

    /* Sort Mnemonic Definitions */
    for(int i = 1; i < mneDefinitions.length(); i++)
    {
        Mnemonic* tmp1 = mneDefinitions[i];
        int j = i;
        while(j > 0)
        {
            Mnemonic* tmp2 = mneDefinitions[j - 1];
            if(strcmp(tmp2->name, tmp1->name) < 0)
            {
                break;
            }
            mneDefinitions.set(j, tmp2);
            j--;
        }
        mneDefinitions.set(j, tmp1);
    }
}

/*----------------------------------------------------------------------------
 * createCmdTlmLists  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
void ItosRecordParser::createCmdTlmLists(void)
{
    /* Get Packets */
    MgList<Packet*>* pkts = getPackets();
    for(int p = 0; p < pkts->length(); p++)
    {
        Packet* pkt = (Packet*)pkts->get(p);
        int apid = pkt->getApid();
        if(apid >= 0 && apid < CCSDS_NUM_APIDS)
        {
            if(pkt->isType(Packet::COMMAND))
            {
                cmdPackets[apid].add(pkt);
            }
            else if(pkt->isType(Packet::TELEMETRY))
            {
                tlmPackets[apid].add(pkt);
            }
        }
        else
        {
            mlog(WARNING, "Invalid APID %d provided for packet %s\n", apid, pkt->getName());
        }
    }
}

/*----------------------------------------------------------------------------
 * createConversion  -
 *----------------------------------------------------------------------------*/
TypeConversion* ItosRecordParser::createConversion (TypeConversion::type_conv_t _type, Record* declaration)
{
    const char* conv_name = declaration->getUnqualifiedName();
    TypeConversion* type_conv = new TypeConversion(_type, conv_name);

    for(int i = 0; i < declaration->getNumSubRecords(); i++)
    {
        Record* subrec = (Record*)declaration->getSubRecord(i);
        const char* name = subrec->getUnqualifiedName();
        const char* value = NULL;
        if(subrec->isValue())
        {
            SafeString valcat(256);
            for(int v = 0; v < subrec->getNumSubValues(); v++)
            {
                valcat += subrec->getSubValue(v);
                valcat += " ";
            }
            value = valcat.getString(true);
        }
        else
        {
            Record* valrec = (Record*)subrec->getSubRecord(0);
            value = StringLib::duplicate(valrec->getSubValue(0));
        }

        if(value)
        {
            type_conv->addEnumLookup(name, value);
            mlog(INFO, "ADDING CONVERSION %s: %s --> %s\n", conv_name, name, value);
        }
        else
        {
            mlog(ERROR, "Unable to add conversion %s to %s\n", name, conv_name);
        }
    }

    return type_conv;
}

/*----------------------------------------------------------------------------
 * createCTSummary  -
 *----------------------------------------------------------------------------*/
const char* ItosRecordParser::createCTSummary (const char* pkttype, bool local)
{
    char tmp[Record::MAX_TOKEN_SIZE];
 	SafeString html = SafeString(1000);

    if(strcmp(pkttype, "cmd") == 0)
    {
        /* Build Command Summary */
        html += "<h3>CCSDS Command Packet Summary</h3>\n";
        html += "<table id=\"table-cmd\">\n";
        html += "	<thead>\n";
        html += "		<th>PACKET NAME</th>\n";
        html += "		<th>TYPE</th>\n";
        html += "		<th>APID</th>\n";
        html += "		<th>FC</th>\n";
        html += "		<th>DESTINATION</th>\n";
        html += "		<th>DESCRIPTION</th>\n";
        html += "	</thead>\n";
        html += "	<tbody>\n";
        for(int p = 0; p < packets.length(); p++)
        {
            Packet* packet = packets[p];
            if(packet->isPrototype() == false)
            {
                if(packet->isType(Packet::COMMAND))
                {
                    html += "		<tr>\n";
                    if(local)
                    {
                    /* MNEMONIC*/           snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><a href=\"#%s\">%s</a></td>\n", packet->getName(), packet->getName());  html += tmp;
                    }
                    else
                    {
                    /* MNEMONIC*/           snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><a href=\"%s_%s.html#%s\">%s</a></td>\n", HANDBOOK_PATH, packet->getName(), packet->getName(), packet->getName());  html += tmp;

                    }
                    /* TYPE*/               snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", packet->getPktProperty("criticality")); html += tmp;
                    /* APID*/               snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", packet->getProperty(CommandPacket::apidDesignation, "defaultValue", 0)); html += tmp;
                    /* FC*/                 snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", packet->getProperty(CommandPacket::fcDesignation, "defaultValue", 0)); html += tmp;
                    /* DESTINATION*/        snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><div id=\"%s_divid_srcdest\"></div></td>\n", packet->getUndottedName()); html += tmp;
                    /* DESCRIPTION*/        snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><div id=\"%s_divid_description\"></div></td>\n", packet->getUndottedName()); html += tmp;
                    html += "		</tr>\n";
                }
            }
        }
        html += "	</tbody>\n";
        html += "	</table>\n";
    }

    if(strcmp(pkttype, "tlm") == 0)
    {
        /* Build Telemetry Summary */
        html += "<h3>CCSDS Telemetry Packet Summary</h3>\n";
        html += "<table id=\"table-tlm\">\n";
        html += "	<thead>\n";
        html += "		<th>PACKET NAME</th>\n";
        html += "		<th>TYPE</th>\n";
        html += "		<th>APID</th>\n";
        html += "		<th>GEN RATE</th>\n";
        html += "		<th>RT RATE</th>\n";
        html += "		<th>SIZE</th>\n";
        html += "		<th>SOURCE</th>\n";
        html += "		<th>DESCRIPTION</th>\n";
        html += "	</thead>\n";
        html += "	<tbody>\n";
        for(int p = 0; p < packets.length(); p++)
        {
            Packet* packet = packets[p];
            if(packet->isPrototype() == false)
            {
                if(packet->isType(Packet::TELEMETRY))
                {
                    TelemetryPacket* tlm = (TelemetryPacket*)packet;

                    html += "		<tr>\n";
                    if(local)
                    {
                    /* MNEMONIC*/           snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><a href=\"#%s\">%s</a></td>\n", packet->getName(), packet->getName());  html += tmp;
                    }
                    else
                    {
                    /* MNEMONIC*/           snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><a href=\"%s_%s.html#%s\">%s</a></td>\n", HANDBOOK_PATH, packet->getName(), packet->getName(), packet->getName());  html += tmp;
                    }
                    /* TYPE*/               snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", tlm->getFilterProperty("type")); html += tmp;
                    /* APID*/               snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", packet->getProperty(TelemetryPacket::apidDesignation, "defaultValue", 0)); html += tmp;
                    /* GEN RATE*/           snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", tlm->getFilterProperty("rate")); html += tmp;
                    /* RT RATE*/            {
                                                const char* apid_str = packet->getProperty(TelemetryPacket::apidDesignation, "defaultValue", 0);
                                                long apid = 0;
                                                StringLib::str2long(apid_str, &apid);
                                                int size = packet->getNumBytes();
                                                if(apid >= 0x470 || size > 256)
                                                {
                                                    snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>n/a</td>\n"); html += tmp;
                                                }
                                                else
                                                {
                                                    snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", tlm->getFilterProperty("rtrate")); html += tmp;
                                                }
                                            }
                    /* SIZE*/               snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%d</td>\n", packet->getNumBytes()); html += tmp;
                    /* SOURCE*/             snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td>%s</td>\n", tlm->getFilterProperty("source")); html += tmp;
                    /* DESCRIPTION*/        snprintf(tmp, Record::MAX_TOKEN_SIZE, "			<td><div id=\"%s_divid_description\"></div></td>\n", packet->getUndottedName()); html += tmp;
                    html += "		</tr>\n";
                }
            }
        }
        html += "	</tbody>\n";
        html += "	</table>\n";
    }

    html += "<script src=\"summary.js\"></script>\n";

    /* Return SafeString */
    return html.getString(true);
}

/*----------------------------------------------------------------------------
 * createPacketDetails  -
 *----------------------------------------------------------------------------*/
const char* ItosRecordParser::createPacketDetails (Packet* packet)
{
    #define MAX_CT_DETIALS_STRING_SIZE  5000
    char        tmp[MAX_CT_DETIALS_STRING_SIZE];
 	SafeString  html = SafeString(1000);

    if(packet->isPrototype() == false)
    {
        mlog(INFO, "Generating detailed report for packet: %s\n", packet->getName());

        if(optUserEditable) { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<form action=\"pyedit.py/pktedit\" method=\"POST\" onSubmit=\"popupform(this, 'EDIT')\">"); html += tmp; }
        snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<h3><a id=\"%s\">%s</a> ", packet->getName(), packet->getName()); html += tmp;
        if(optUserEditable) { html += "<input type=\"submit\" value=\"EDIT\">"; }
        if(optUserEditable) { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<input type=\"hidden\" name=\"packet\" value=\"%s\">", packet->getUndottedName()); html += tmp; }
        if(optUserEditable) { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<input type=\"hidden\" name=\"selection\" value=\"%s\">", packet->isType(Packet::COMMAND) ? "command" : "telemetry"); html += tmp; }
        html += "</h3>";
        if(optUserEditable) { html += "</form>"; }

        if(packet->isType(Packet::TELEMETRY))
        {
            TelemetryPacket* tlm = (TelemetryPacket*)packet;

            html += "<table id=\"table-description\">\n";
            /* TYPE      */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Telemetry Type:</b></td><td>%s</td></tr>\n", tlm->getFilterProperty("type"));                        html += tmp;
            /* APID      */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Application ID:</b></td><td>%s</td></tr>\n", packet->getProperty(TelemetryPacket::apidDesignation, "defaultValue", 0));  html += tmp;
            /* SIZE      */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Size:</b></td><td>%d</td></tr>\n", packet->getNumBytes());                                           html += tmp;
            /* GEN RATE  */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Generation Rate:</b></td><td>%s</td></tr>\n", tlm->getFilterProperty("rate"));                       html += tmp;
            /* RT RATE   */ {
                                const char* apid_str = packet->getProperty(TelemetryPacket::apidDesignation, "defaultValue", 0);
                                long apid = 0;
                                StringLib::str2long(apid_str, &apid);
                                int size = packet->getNumBytes();
                                if(apid >= 0x470 || size > 256)
                                {
                                    snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, " <td><b>Real Time Rate:</b></td><td>n/a</td>\n"); html += tmp;
                                }
                                else
                                {
                                    snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<td><b>Real Time Rate:</b></td><td>%s</td>\n", tlm->getFilterProperty("rtrate")); html += tmp;
                                }
                            }
            /* SENDER    */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Source Card:</b></td><td>%s</td></tr>\n", tlm->getFilterProperty("sender"));                         html += tmp;
            /* TASK      */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Source Task:</b></td><td>%s</td></tr>\n", tlm->getFilterProperty("task"));                           html += tmp;
            /* SOURCE    */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Source Data:</b></td><td>%s</td></tr>\n", tlm->getFilterProperty("source"));                         html += tmp;
            html += "	<tr><td><br /></td></tr>\n";
            html += "</table>\n";
        }
        else if(packet->isType(Packet::COMMAND))
        {
            html += "<table id=\"table-description\">\n";
            /* TYPE        */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Command Type:</b></td><td>%s</td></tr>\n", packet->getPktProperty("criticality")); html += tmp;
            /* APID        */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Application ID:</b></td><td>%s</td></tr>\n", packet->getProperty(CommandPacket::apidDesignation, "defaultValue", 0));  html += tmp;
            /* FC          */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Function Code:</b></td><td>%s</td></tr>\n", packet->getProperty(CommandPacket::fcDesignation, "defaultValue", 0));   html += tmp;
            /* DESTINATION */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Destination:</b></td><td><div id=\"%s_divid_srcdest\"></div></td></tr>\n", packet->getUndottedName());  html += tmp;
            /* LENGTH      */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><b>Length:</b></td><td>%d</td></tr>\n", packet->getNumBytes());  html += tmp;
            html += "	<tr><td><br /></td></tr>\n";
            html += "</table>\n";
        }

        html += "<table id=\"table-description\">\n";
        html += "	<tr><td><b>Description:</b></td><td></td></tr>\n";
        html += "	<tr><td><br /></td></tr>\n";
        /* DESCRIPTION */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td><div id=\"%s_divid_description\"></div>\n</td></tr>\n", packet->getUndottedName());  html += tmp;
        html += "	<tr><td><br /></td></tr>\n";
        html += "</table>\n";

        const char* comment = packet->getComment();
        if(comment != NULL)
        {
            SafeString safe_comment("%s", comment + 3);
            safe_comment.replace("\n", "</br>");
            html += "<table id=\"table-description\">\n";
            html += "	<tr><td><b>Database Comments:</b></td><td></td></tr>\n";
            html += "	<tr><td><br /></td></tr>\n";
            /* COMMENT */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td>%s</td></tr>\n", safe_comment.getString(false));  html += tmp;
            html += "	<tr><td><br /></td></tr>\n";
            html += "</table>\n";
        }

        html += "<table id=\"table-description\">\n";
        html += "	<tr><td><b>Format:</b></td><td></td></tr>\n";
        html += "	<tr><td><br /></td></tr>\n";
        html += "</table>\n";

        if(packet->isType(Packet::COMMAND))
        {
            char* raw = packet->serialize(Packet::RAW_STOL_CMD_FMT, MAX_CT_DETIALS_STRING_SIZE);
            char* stol = packet->serialize(Packet::STOL_CMD_FMT, MAX_CT_DETIALS_STRING_SIZE);

            html += "<table id=\"table-description\">\n";
            snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td>%s</td><td></td></tr>\n", raw); html += tmp;
            html += "	<tr><td><br /></td></tr>\n";
            snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "	<tr><td>%s</td><td></td></tr>\n", stol); html += tmp;
            html += "	<tr><td><br /></td></tr>\n";
            html += "</table>\n";
        }

        /* FORMAT TABLE HEADER */
        if(packet->isType(Packet::COMMAND))
        {
            html += "<table id=\"table-cmd\">\n";
            html += "	<thead>\n";
            html += "		<th>PARM</th>\n";
            html += "		<th>FIELD NAME</th>\n";
            html += "		<th>OFFSET</th>\n";
            html += "		<th>BITS</th>\n";
            html += "		<th>BIT_MASK</th>\n";
            html += "		<th>DATA_TYPE</th>\n";
            html += "		<th>RANGE</th>\n";
            html += "		<th>DESCRIPTION</th>\n";
            if(optUserEditable) html += "       <th>EDIT</th>\n";
            html += "	</thead>\n";
        }
        else if(packet->isType(Packet::TELEMETRY))
        {
            html += "<table id=\"table-tlm\">\n";
            html += "	<thead>\n";
            html += "		<th>FIELD NAME</th>\n";
            html += "		<th>OFFSET</th>\n";
            html += "		<th>BITS</th>\n";
            html += "		<th>BIT_MASK</th>\n";
            html += "		<th>DATA_TYPE</th>\n";
            html += "		<th>DESCRIPTION</th>\n";
            if(optUserEditable) html += "       <th>EDIT</th>\n";
            html += "	</thead>\n";
        }

        /* FORMAT TABLE FIELDS */
        html += "	<tbody>\n";
        int parm_num = 0;
        for(int f = 0; f < packet->getNumFields(); f++)
        {
            Field* field = (Field*)packet->getField(f);

            if(field->isPayload() == false && (optFullPktDetails == false || packet->isType(Packet::COMMAND))) continue;

            html += "		<tr>\n";
            if(packet->isType(Packet::COMMAND))
            {
                if(field->isPayload())
                {
                    if(field->getLengthInBits() % 8 == 0)
                    {
                        /* PARM */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>%c</td>\n", Packet::parmSymByte[parm_num++ % Packet::NumParmSyms]); html += tmp;
                    }
                    else
                    {
                        /* PARM */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>%c</td>\n", Packet::parmSymBit[parm_num++ % Packet::NumParmSyms]); html += tmp;
                    }
                }
                else
                {
                    /* PARM */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td></td>\n"); html += tmp;
                }
            }

            /* Set Common Fields for Commands and Telemetry */
            char namebuf[Record::Record::MAX_TOKEN_SIZE];
            /* MNEMONIC/FIELD NAME  */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>%s</td>\n",     field->getDisplayName(namebuf)); html += tmp;
            /* OFFSET               */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>%d</td>\n",     field->getByteOffset()); html += tmp;
            /* BITS                 */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>%d</td>\n",     field->getLengthInBits() * field->getNumElements()); html += tmp;
            if     (field->getBaseSizeInBits() == 0)   /* BIT_MASK  */  { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>---</td>\n"); html += tmp; }
            else if(field->getBaseSizeInBits() == 8)   /* BIT_MASK  */  { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>0x%0lX</td>\n", field->getBitMask()); html += tmp; }
            else if(field->getBaseSizeInBits() == 16)  /* BIT_MASK  */  { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>0x%04lX</td>\n", field->getBitMask()); html += tmp; }
            else if(field->getBaseSizeInBits() == 32)  /* BIT_MASK  */  { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>0x%08lX</td>\n", field->getBitMask()); html += tmp; }
            else if(field->getBaseSizeInBits() == 64)  /* BIT_MASK  */  { snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>0x%016lX</td>\n", field->getBitMask()); html += tmp; }

            /* Print Data Type */
            const char* fconv = field->getConversion();
            TypeConversion* conv = findConversion(fconv);
            if(conv == NULL)
            {
                if(fconv != NULL)
                {
                    mlog(ERROR, "Did not find definition for %s for field: %s", fconv, field->getName());
                }
                /* DATA_TYPE */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>%s</td>\n",     field->getType()); html += tmp;
            }
            else
            {
                /* DATA_TYPE */ snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td><b>%s(%s)</b>%s</td>\n", conv->getName(), field->getType(), conv->getAsHtml()); html += tmp;
            }

            /* Set the Range for Commands */
            if(packet->isType(Packet::COMMAND))
            {
                bool range_set = false;
                char min_range_str[128], max_range_str[128];
                memset(min_range_str, 0, sizeof(min_range_str));
                memset(max_range_str, 0, sizeof(max_range_str));

                if(field->isType(Field::INTEGER))
                {
                    IntegerField* ifield = (IntegerField*)field;
                    const char* min_str = ifield->getProperty("minRange");
                    const char* max_str = ifield->getProperty("maxRange");
                    long min_range = INT_MIN;
                    long max_range = INT_MAX;
                    StringLib::str2long(min_str, &min_range);
                    StringLib::str2long(max_str, &max_range);
                    if(min_range != INT_MIN) { snprintf(min_range_str, 128, "%ld", min_range); range_set = true; }
                    if(max_range != INT_MAX) { snprintf(max_range_str, 128, "%ld", max_range); range_set = true; }
                }
                else if(field->isType(Field::UNSIGNED))
                {
                    UnsignedField* ufield = (UnsignedField*)field;
                    const char* min_str = ufield->getProperty("minRange");
                    const char* max_str = ufield->getProperty("maxRange");
                    unsigned long min_range = 0;
                    unsigned long max_range = UINT_MAX;
                    StringLib::str2ulong(min_str, &min_range);
                    StringLib::str2ulong(max_str, &max_range);
                    if(min_range != 0)           { snprintf(min_range_str, 128, "%lu", min_range); range_set = true; }
                    if(max_range != UINT_MAX)    { snprintf(max_range_str, 128, "%lu", max_range); range_set = true; }
                }
                else if(field->isType(Field::FLOAT))
                {
                    FloatField* ffield = (FloatField*)field;
                    const char* min_str = ffield->getProperty("minRange");
                    const char* max_str = ffield->getProperty("maxRange");
                    double min_range = DBL_MIN;
                    double max_range = DBL_MAX;
                    StringLib::str2double(min_str, &min_range);
                    StringLib::str2double(max_str, &max_range);
                    if(min_range != DBL_MIN) { snprintf(min_range_str, 128, "%.3lf", min_range); range_set = true; }
                    if(max_range != DBL_MAX) { snprintf(max_range_str, 128, "%.3lf", max_range); range_set = true; }
                }

                if(range_set && strcmp(min_range_str, max_range_str) == 0)
                {
                    /* RANGE */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>[%s]</td>\n", min_range_str); html += tmp;
                }
                else if(range_set)
                {
                    /* RANGE */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>[%s, %s]</td>\n", min_range_str, max_range_str); html += tmp;
                }
                else if(field->isType(Field::STRING))
                {
                    /* RANGE */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>char[%d]</td>\n", field->getNumElements()); html += tmp;
                }
                else
                {
                    /* RANGE */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td>---</td>\n"); html += tmp;
                }
            }

            const char* field_comment = field->getComment();
            if(field_comment != NULL)
            {
            /* SHORT     */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td><div id=\"%s_divid_short\"></div></br>%s</td>\n", field->getUndottedName(), field_comment + 1); html += tmp;
            }
            else
            {
            /* SHORT     */  snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "			<td><div id=\"%s_divid_short\"></div></td>\n", field->getUndottedName()); html += tmp;
            }
            /* EDIT      */
            if(optUserEditable)
            {
                html += "<td><form action=\"pyedit.py/fldedit\" method=\"POST\" onSubmit=\"popupform(this, 'EDIT')\">";
                html += "<input type=\"submit\" value=\"EDIT\">";
                snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<input type=\"hidden\" name=\"packet\" value=\"%s\">", packet->getUndottedName()); html += tmp;
                snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<input type=\"hidden\" name=\"field\" value=\"%s\">", field->getUndottedName()); html += tmp;
                snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<input type=\"hidden\" name=\"selection\" value=\"%s\">", packet->isType(Packet::COMMAND) ? "command" : "telemetry"); html += tmp;
                html += "</form></td>";
            }
            html += "		</tr>\n";
        }
        html += "	</tbody>\n";
        html += "</table>\n";

        html += "<script src=\"summary.js\"></script>\n";
        snprintf(tmp, MAX_CT_DETIALS_STRING_SIZE, "<script src=\"%s.js\"></script>\n", packet->getUndottedName()); html += tmp;
    }

    return html.getString(true);
}

/*----------------------------------------------------------------------------
 * createMNSummary  -
 *----------------------------------------------------------------------------*/
const char* ItosRecordParser::createMNSummary (bool local)
{
    char tmp[Record::MAX_TOKEN_SIZE * 4];
    SafeString html(1000);

    /* Build Telemetry Summary */
    html += "<h3>CCSDS Mnemonic Summary</h3>\n";
    html += "<table id=\"table-mne\">\n";
    html += "	<thead>\n";
    html += "		<th>NAME</th>\n";
    html += "		<th>TYPE</th>\n";
    html += "		<th>SOURCE</th>\n";
    html += "		<th>CONVERSION</th>\n";
    html += "	</thead>\n";
    html += "	<tbody>\n";

    for(int u = 0; u < mneDefinitions.length(); u++)
    {
        Mnemonic* mnem = mneDefinitions[u];

        html += "		<tr>\n";

        /* MNEMONIC */
        sprintf(tmp, "			<td>%s</td>\n", mnem->name);  html += tmp;

        /* TYPE */
        sprintf(tmp, "			<td>%s</td>\n", mnem->type); html += tmp;

        /* SOURCE */
        if(mnem->source != NULL)
        {
            if(mnem->source_packet != NULL)
            {
                if(local) { sprintf(tmp, "			<td><a href=\"#%s\">%s</a></td>\n", mnem->source_packet, mnem->source); html += tmp; }
                else      { sprintf(tmp, "			<td><a href=\"%s_%s.html#%s\">%s</a></td>\n", HANDBOOK_PATH, mnem->source_packet, mnem->source_packet, mnem->source); html += tmp; }
            }
            else
            {
                sprintf(tmp, "			<td>%s</td>\n", mnem->source); html += tmp;
            }
        }
        else
        {
            sprintf(tmp, "			<td>--</td>\n"); html += tmp;
        }

        /* CONVERSION */
        if(mnem->conversion != NULL)
        {
            sprintf(tmp, "			<td><b>%s</b>(%s)%s</td>\n", mnem->conversion->getName(), mnem->conversion->getType(), mnem->conversion->getAsHtml(false)); html += tmp;
        }
        else
        {
            sprintf(tmp, "			<td>--</td>\n"); html += tmp;
        }

        html += "		</tr>\n";
    }
    html += "	</tbody>\n";
    html += "	</table>\n";

    html += "<script src=\"summary.js\"></script>\n";

    /* Return SafeString */
    return html.getString(true);
}

/*----------------------------------------------------------------------------
 * findPacket  -
 *----------------------------------------------------------------------------*/
Packet* ItosRecordParser::findPacket (const char* name)
{
    for(int p = 0; p < packets.length(); p++)
    {
        Packet* packet = packets[p];
        if(packet->isName(name))
        {
            return packet;
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * findConversion  -
 *----------------------------------------------------------------------------*/
TypeConversion* ItosRecordParser::findConversion (const char* name)
{
    if(name != NULL)
    {
        for(int c = 0; c < conversions.length(); c++)
        {
            TypeConversion* conv = conversions[c];
            if(conv->isName(name))
            {
                return conv;
            }
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * writeFile  -
 *----------------------------------------------------------------------------*/
bool ItosRecordParser::writeFile (const char* fname, const char* fcontents, long fsize)
{
    FILE* fptr = fopen(fname, "w");
    if(fptr == NULL)
    {
        mlog(ERROR, "unable to open file %s\n", fname);
        return false;
    }

    fwrite(fcontents, 1, fsize, fptr);
    fclose(fptr);

    return true;
}

/*----------------------------------------------------------------------------
 * generateReport  -
 *----------------------------------------------------------------------------*/
void ItosRecordParser::generateReport(const char* reporttemplate, const char* summarytemplate, const char* outputpath)
{
    char fullreportname[256], cmdreportname[256], tlmreportname[256], mnereportname[256], pktreportname[512];

    /* Get Time String */
    char timestr[25] = {'\0'};
    TimeLib::gmt_time_t timeinfo = TimeLib::gettime();
    snprintf (timestr, 25, "%d:%d", timeinfo.year, timeinfo.day);

    /* Generate Content */
    List<const char*> pktptrs;
    SafeString pktreport(1000);
    for(int p = 0; p < packets.length(); p++)
    {
        Packet* packet = packets[p];
        const char* pkthtml = createPacketDetails(packet);
        pktptrs.add(pkthtml);
        pktreport += pkthtml;
    }

    /* Generate Report from Template */
    SafeString* report = readFile(reporttemplate);
    if(report == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate report\n", reporttemplate);
    }
    else
    {
        report->replace("$DATE", timestr);
        report->replace("$APPENDIX_A1", createCTSummary("cmd"));
        report->replace("$APPENDIX_A2", createCTSummary("tlm"));
        report->replace("$APPENDIX_A3", createMNSummary(true));
        report->replace("$APPENDIX_B", pktreport.getString(true));
        snprintf(fullreportname, 256, "%s.html", outputpath);
        writeFile(fullreportname, report->getString(), report->getLength());
        delete report;
    }

    /* Generate Command Summary from Template */
    SafeString* cmdsummary = readFile(summarytemplate);
    if(cmdsummary == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate summary\n", summarytemplate);
    }
    else
    {
        cmdsummary->replace("$DATE", timestr);
        cmdsummary->replace("$APPENDIX_CONTENT", createCTSummary("cmd", false));
        snprintf(cmdreportname, 256, "%s_cmd.html", outputpath);
        writeFile(cmdreportname, cmdsummary->getString(), cmdsummary->getLength());
        delete cmdsummary;
    }

    /* Generate Telemetry Summary from Template */
    SafeString* tlmsummary = readFile(summarytemplate);
    if(tlmsummary == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate summary\n", summarytemplate);
    }
    else
    {
        tlmsummary->replace("$DATE", timestr);
        tlmsummary->replace("$APPENDIX_CONTENT", createCTSummary("tlm", false));
        snprintf(tlmreportname, 256, "%s_tlm.html", outputpath);
        writeFile(tlmreportname, tlmsummary->getString(), tlmsummary->getLength());
        delete tlmsummary;
    }

    /* Generate Command Summary from Template */
    SafeString* mnesummary = readFile(summarytemplate);
    if(mnesummary == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate summary\n", summarytemplate);
    }
    else
    {
        mnesummary->replace("$DATE", timestr);
        mnesummary->replace("$APPENDIX_CONTENT", createMNSummary(false));
        sprintf(mnereportname, "%s_mne.html", outputpath);
        writeFile(mnereportname, mnesummary->getString(), mnesummary->getLength());
        delete mnesummary;
    }

    /* Generate Packet Summary from Template */
    for(int i = 0; i < pktptrs.length(); i++)
    {
        const char* pkthtml = (const char*)pktptrs[i];
        const char* pktname = packets[i]->getName();
        SafeString* pktsummary = readFile(summarytemplate);
        if(pktsummary == NULL)
        {
            mlog(ERROR, "unable to open template file %s... unable to generate summary\n", summarytemplate);
        }
        else
        {
            pktsummary->replace("$DATE", timestr);
            pktsummary->replace("$APPENDIX_CONTENT", pkthtml);
            snprintf(pktreportname, 256, "%s_%s.html", outputpath, pktname);
            writeFile(pktreportname, pktsummary->getString(), pktsummary->getLength());
            delete pktsummary;
        }
    }
}

/*----------------------------------------------------------------------------
 * generateDocuments  -
 *
 *   Notes: This is used to create html files meant for making into PDFs
 *----------------------------------------------------------------------------*/
void ItosRecordParser::generateDocuments(const char* documenttemplate, const char* outputpath)
{
    char cmddocname[256], tlmdocname[256], mnedocname[256];

    /* Get Time String */
    char timestr[25] = {'\0'};
    TimeLib::gmt_time_t timeinfo = TimeLib::gettime();
    snprintf (timestr, 25, "%d:%d", timeinfo.year, timeinfo.day);

    /* Generate Content */
    SafeString cmdpktdoc = SafeString(1000);
    SafeString tlmpktdoc = SafeString(1000);
    for(int p = 0; p < packets.length(); p++)
    {
        Packet* packet = packets[p];
        if(packet->isType(Packet::COMMAND))
        {
            cmdpktdoc += createPacketDetails(packet);;
        }
        else if(packet->isType(Packet::TELEMETRY))
        {
            tlmpktdoc += createPacketDetails(packet);;
        }
    }

    /* Generate Command Document from Template */
    SafeString* cmddoc = readFile(documenttemplate);
    if(cmddoc == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate summary\n", documenttemplate);
    }
    else
    {
        cmddoc->replace("$DATE", timestr);
        cmddoc->replace("$SUMMARY", createCTSummary("cmd", true));
        cmddoc->replace("$DESCRIPTIONS", cmdpktdoc.getString(true));
        snprintf(cmddocname, 256, "%s/AtlasCommandHandbook.html", outputpath);
        writeFile(cmddocname, cmddoc->getString(), cmddoc->getLength());
        delete cmddoc;
    }

    /* Generate Telemetry Document from Template */
    SafeString* tlmdoc = readFile(documenttemplate);
    if(tlmdoc == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate summary\n", documenttemplate);
    }
    else
    {
        tlmdoc->replace("$DATE", timestr);
        tlmdoc->replace("$SUMMARY", createCTSummary("tlm", true));
        tlmdoc->replace("$DESCRIPTIONS", tlmpktdoc.getString(true));
        snprintf(tlmdocname, 256, "%s/AtlasTelemetryHandbook.html", outputpath);
        writeFile(tlmdocname, tlmdoc->getString(), tlmdoc->getLength());
        delete tlmdoc;
    }

    /* Generate Mnemonic Document from Template */
    SafeString* mnedoc = readFile(documenttemplate);
    if(mnedoc == NULL)
    {
        mlog(ERROR, "unable to open template file %s... unable to generate summary\n", documenttemplate);
    }
    else
    {
        mnedoc->replace("$DATE", timestr);
        mnedoc->replace("$SUMMARY", createMNSummary(true));
        mnedoc->replace("$DESCRIPTIONS", "");
        sprintf(mnedocname, "%s/AtlasMnemonicHandbook.html", outputpath);
        writeFile(mnedocname, mnedoc->getString(), mnedoc->getLength());
    }
}

/*----------------------------------------------------------------------------
 * loadRecFilesCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::loadRecFilesCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

#ifdef _LINUX_
    /* Tokenize Files */
    glob_t glob_buffer;
    for(int i = 0; i < argc; i++)
    {
        glob(argv[i] , 0 , NULL , &glob_buffer);

        for (unsigned int m = 0; m < glob_buffer.gl_pathc; m++)
        {
            char* fname = glob_buffer.gl_pathv[m];
            mlog(INFO, "Parsing: %s\n", fname);

            SafeString* fcontents = readFile(fname);
            if(fcontents == NULL)
            {
                mlog(CRITICAL, "Unable to open file: %s\n", fname);
                return -1;
            }
            else
            {
                mlog(DEBUG, " ... size = %ld\n", fcontents->getLength());
            }

            parseRecTokens(fcontents);
            mlog(DEBUG, " ... total tokens = %d\n", tokens.length());

            delete fcontents;
        }

        globfree( &glob_buffer );
    }
#else
    for (int i = 0; i < argc; i++)
    {
        char* fname = argv[i];
        mlog(INFO, "Parsing: %s\n", fname);

        SafeString* fcontents = readFile(fname);
        if (fcontents == NULL)
        {
            mlog(CRITICAL, "Unable to open file: %s\n", fname);
            return -1;
        }
        else
        {
            mlog(DEBUG, " ... size = %ld\n", fcontents->getLength());
        }

        parseRecTokens(fcontents);
        mlog(DEBUG, " ... total tokens = %d\n", tokens.length());

        delete fcontents;
    }
#endif

    return 0;
}

/*----------------------------------------------------------------------------
 * loadFilterTblCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::loadFilterTblCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    char* fname = argv[0];
    mlog(INFO, "Parsing: %s\n", fname);

    SafeString* fcontents = readFile(fname);
    if(fcontents == NULL)
    {
        mlog(CRITICAL, "Unable to open file: %s\n", fname);
        return -1;
    }
    else
    {
        mlog(DEBUG, " ... size = %ld\n", fcontents->getLength());
    }

    parseFilterTbl(fcontents);
    mlog(DEBUG, " ... total filters = %d\n", filters.length());

    delete fcontents;

    return 0;
}

/*----------------------------------------------------------------------------
 * applyFilterTblCmd  -
 *----------------------------------------------------------------------------*/
int ItosRecordParser::applyFilterTblCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    for(int f = 0; f < filters.length(); f++)
    {
        Filter* filter = filters[f];
        for(int p = 0; p < packets.length(); p++)
        {
            Packet* packet = packets[p];
            int apid = packet->getApid();
            if(apid != Packet::INVALID_APID)
            {
                if(packet->isType(Packet::TELEMETRY) && filter->onApid(apid))
                {
                    TelemetryPacket* tlm = (TelemetryPacket*)packet;
                    tlm->setFilter(filter);
                }
            }
        }
    }
    return 0;
}

/*----------------------------------------------------------------------------
 * setDesignationsCmd  -
 *
 *   Notes: This function can cause a memory leak if called multiple times
 *            the static function that sets the designations does not know if
 *            the string has been allocated or is a global constant... it would be
 *            better if the static class allocated and freed the memory inside (this
 *            is a TODO)
 *----------------------------------------------------------------------------*/
int ItosRecordParser::setDesignationsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    if(argc != 3) return -1;

    const char* cmd_apid_str    = StringLib::checkNullStr(argv[0]);
    const char* cmd_fc_str      = StringLib::checkNullStr(argv[1]);
    const char* tlm_apid_str    = StringLib::checkNullStr(argv[2]);

    CommandPacket::setDesignations(cmd_apid_str, cmd_fc_str);
    TelemetryPacket::setDesignations(tlm_apid_str);

    return 0;
}

/*----------------------------------------------------------------------------
 * buildDatabaseCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::buildDatabaseCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    /* Create Records from Tokens */
    mlog(CRITICAL, "Creating records... from %d tokens\n", tokens.length());
    createRecords();

    /* Create Packets from Records */
    mlog(CRITICAL, "Creating packets... from %d records and %d declarations\n", dictionary.length(), declarations.length());
    createPackets();

    /* Populate Commands and Telemetry */
    mlog(CRITICAL, "Creating list of commands and telemetry... from %d packets\n", packets.length());
    createCmdTlmLists();

    /* Populate Mnemonics */
    mlog(CRITICAL, "Populating list of mnemonics... from %d records\n", mnemonics.length());
    createMnemonics();

    return 0;
}

/*----------------------------------------------------------------------------
 * buildDatabaseCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::buildRecordsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    char namebuf[Itos::Record::MAX_TOKEN_SIZE];

    /* Get Packets */
    MgList<Packet*>* pkts = getPackets();
    for(int p = 0; p < pkts->length(); p++)
    {
        Packet* pkt = (Packet*)pkts->get(p);
        if(pkt->isPrototype() == true) continue;

        /* Get Packet Characteristics */
        int apid = pkt->getApid();
        int numfields = pkt->getNumFields();

        /* Create Ccsds Record */
        if(pkt->isType(Packet::COMMAND))
        {
            const char* fc_str = pkt->getProperty(CommandPacket::fcDesignation, "value", 0);
            if(fc_str)
            {
                long tmpfc = 0;
                StringLib::str2long(fc_str, &tmpfc);
                delete [] fc_str;
                uint8_t fc = (uint8_t)tmpfc;
                CcsdsRecord::defineCommand(pkt->getName(), CommandPacket::apidDesignation, apid, fc, pkt->getNumBytes(), NULL, 0, numfields);
                mlog(INFO, "Creating command record %s with apid: %04X and function code: %d\n", pkt->getName(), apid, fc);
            }
        }
        else if(pkt->isType(Packet::TELEMETRY))
        {
            CcsdsRecord::defineTelemetry(pkt->getName(), TelemetryPacket::apidDesignation, apid, pkt->getNumBytes(), NULL, 0, numfields);
            mlog(INFO, "Creating telemetry record %s with apid: %04X\n", pkt->getName(), apid);
        }
        else
        {
            continue;
        }

        /* Populate Fields */
        for(int f = 0; f < numfields; f++)
        {
            Field* field = (Field*)pkt->getField(f);
            int bit_length = field->getLengthInBits();
            int bit_base = field->getBaseSizeInBits();
            int num_elem = field->getNumElements();
            RecordObject::fieldType_t fieldtype = RecordObject::INVALID_FIELD;
            if(bit_length % 8 != 0)
            {
                fieldtype = RecordObject::BITFIELD;
            }
            else if(field->isType(Field::UNSIGNED))
            {
                if(bit_base == 8)  fieldtype = RecordObject::UINT8;
                if(bit_base == 16) fieldtype = RecordObject::UINT16;
                if(bit_base == 32) fieldtype = RecordObject::UINT32;
                if(bit_base == 64) fieldtype = RecordObject::UINT64;
            }
            else if(field->isType(Field::INTEGER))
            {
                if(bit_base == 8)  fieldtype = RecordObject::INT8;
                if(bit_base == 16) fieldtype = RecordObject::INT16;
                if(bit_base == 32) fieldtype = RecordObject::INT32;
                if(bit_base == 64) fieldtype = RecordObject::INT64;
            }
            else if(field->isType(Field::FLOAT))
            {
                if(bit_base == 32) fieldtype = RecordObject::FLOAT;
                if(bit_base == 64) fieldtype = RecordObject::DOUBLE;
            }
            else if(field->isType(Field::STRING))
            {
                fieldtype = RecordObject::STRING;
            }

            if(fieldtype == RecordObject::BITFIELD)
            {
                CcsdsRecord::defineField(pkt->getName(), field->getDisplayName(namebuf), fieldtype, field->getOffsetInBits(), field->getLengthInBits(), field->getBigEndian());
            }
            else
            {
                if(num_elem <= 1)
                {
                    CcsdsRecord::defineField(pkt->getName(), field->getDisplayName(namebuf), fieldtype, field->getOffsetInBits() / 8, field->getLengthInBits() / 8, field->getBigEndian());
                }
                else
                {
                    for(int e = 0; e < num_elem; e++)
                    {
                        char* bracket_ptr = StringLib::find(field->getDisplayName(namebuf), '[', false);
                        if(bracket_ptr) *bracket_ptr = '\0'; // operates on namebuf
                        char fnamebuf[Itos::Record::MAX_TOKEN_SIZE]; // to hold final field name
                        StringLib::format(fnamebuf, Itos::Record::MAX_TOKEN_SIZE, "%s[%d]", namebuf, e);
                        CcsdsRecord::defineField(pkt->getName(), fnamebuf, fieldtype, (field->getOffsetInBits() + (e * bit_base)) / 8, field->getLengthInBits() / 8, field->getBigEndian());
                    }
                }
            }
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * datasrvExportCmd  -
 *
 *   Notes: Command Processor Command
 *
 * Data Entry:
 * ----------
 * DB_version, Data_Key, Short_Description, APID, Byte_offset, Bit_offset, Size_In_Bits, Data_type, Units, Calibration_ID, Num_Of_Dimensions, Dim_0, Dim_1, Dim_2
 *
 * Calibration Entry:
 * -----------------
 * DB_version, Calibration_Key, Short_Description, Calibration_Type, Value, Converted_Value
 *
 *
 * DB_Version:        a decimal integer that identifies the version of the database this record is for
 * Data_Key:          a decimal integer that uniquely identifies this data item
 * Short_Description: a text string 128 chars or less describing the data
 * APID:              a decimal integer representing the CCSDS APID of the packet this data item is in
 * Byte_Offset:       a decimal integer representing the byte offset into the packet the most significant bit is located
 * Size_In_Bits:      a decimal integer representing the number of bits this item occupies in the packet
 * Data_Type:         a decimal integer representing the type of data:
 *                          0 = unsigned integer
 *                          1 = signed integer
 *                          2 = IEEE floating point
 *                          3 = boolean
 *                          4 = text string
 * Units:             a text string 32 chars or less with the name of the engineering units (e.g. volts)
 * Calibration_Key:   a decimal integer representing the key to the calibration record used to apply the calibration
 * Num_Of_Dimensions: a decimal integer representing the number of dimensions this data item has (if not an array then 0)
 * Dim_0:             a decimal integer representing the size of dimension 0
 * Dim_1:             a decimal integer representing the size of dimension 1
 * Dim_2:             a decimal integer representing the size of dimension 2
 *
 * Calibration_Type:  a decimal integer representing the type of calibration used
 *                          0 = polynomial
 *                          1 = table lookup
 *                          2 = ? (add these as necessary)
 * Value:             a decimal integer representing the descrete value that gets converted
 * Converted_Value    a text string that replaces the idescrete value
 *----------------------------------------------------------------------------*/
int ItosRecordParser::datasrvExportCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* dbver = StringLib::checkNullStr(argv[0]);
    const char* dfilename = StringLib::checkNullStr(argv[1]);
    const char* cfilename = StringLib::checkNullStr(argv[2]);

    if(dbver == NULL)
    {
        mlog(CRITICAL, "Must supply database version!\n");
        return -1;
    }

    if(dfilename == NULL)
    {
        mlog(CRITICAL, "Must supply filename!\n");
        return -1;
    }

    if(cfilename == NULL)
    {
        mlog(CRITICAL, "Must supply filename!\n");
        return -1;
    }

    /* Open Calibration File */
    FILE* cfp = fopen(cfilename, "w");  // calibration file
    if(cfp == NULL)
    {
        mlog(CRITICAL, "Unable to open calibration output file: %s\n", cfilename);
        return -1;
    }
    else
    {
        fprintf(cfp, "DB_version, Calibration_Key, Short_Description, Calibration_Type, Value, Converted_Value\n");
    }

    /* Open Data File */
    FILE* dfp = fopen(dfilename, "w");  // data file
    if(dfp == NULL)
    {
        mlog(CRITICAL, "Unable to open data output file: %s\n", dfilename);
        fclose(cfp);
        return -1;
    }
    else
    {
        fprintf(dfp, "DB_version, Data_Key, Short_Description, APID, Byte_offset, Bit_offset, Size_In_Bits, Data_type, Units, Calibration_ID, Num_Of_Dimensions, Dim_0, Dim_1, Dim_2\n");
    }

    /* Get Conversions */
    for(int c = 0; c < conversions.length(); c++)
    {
        TypeConversion* conv = conversions[c];
        char** names = NULL;
        int numnames = conv->getNames(&names);
        for(int n = 0; n < numnames; n++)
        {
            fprintf(cfp, "%s, %d, %s, 1, %s, %s\n", dbver, c + 1, conv->getName(), names[n], conv->getEnumValue(names[n]));
            delete names[n];
        }
        delete names;
    }

    /* Get Packets */
    long data_key = 0;
    MgList<Packet*>* pkts = getPackets();
    for(int p = 0; p < pkts->length(); p++)
    {
        Packet* pkt = (Packet*)pkts->get(p);
        if(pkt->isPrototype() == true) continue;

        /* Get Packet Characteristics */
        int apid        = pkt->getApid();
        int numfields   = pkt->getNumFields();
        int valtype     = 0; // set below per field

        /* Loop Through Fields */
        for(int f = 0; f < numfields; f++)
        {
            Field* field = (Field*)pkt->getField(f);

            /* Get Data Type */
                 if(field->getLengthInBits() % 8 != 0)  valtype = 0; // UNSIGNED INTEGER
            else if(field->isType(Field::UNSIGNED))     valtype = 0; // UNSIGNED INTEGER
            else if(field->isType(Field::INTEGER))      valtype = 1; // SIGNED INTEGER
            else if(field->isType(Field::FLOAT))        valtype = 2; // FLOAT
            else if(field->isType(Field::STRING))       valtype = 4; // TEXT STRING

            /* Get Units */
            int cal_id = 0;
            const char* fconv = field->getConversion();
            if(fconv != NULL)
            {
                TypeConversion* conv = findConversion(fconv);
                if(conv)
                {
                    for(int c = 0; c < conversions.length(); c++)
                    {
                        TypeConversion* lconv = conversions[c];
                        if(lconv)
                        {
                            if(strcmp(conv->getName(), lconv->getName()) == 0)
                            {
                                cal_id = c;
                                break;
                            }
                        }
                    }
                }
            }

            /* Get Num Dimensions */
            int valdim = 0;
            int valdim1 = 0;
            int numelem = field->getNumElements();
            if(numelem > 1)
            {
                valdim = 1;
                valdim1 = numelem;
            }

            /* Write Record */
            char tmp[Itos::Record::MAX_TOKEN_SIZE];
            fprintf(dfp, "%s, %ld, %s.%s, 0x%04X, %d, %d, %d, %d, %s, %d, %d, %d, %d, %d\n", dbver, ++data_key, pkt->getName(), field->getDisplayName(tmp), apid, field->getByteOffset(), field->getOffsetInBits(), field->getLengthInBits(), valtype, field->getType(), cal_id, valdim, valdim1, 0, 0);
        }
    }

    /* Close Files */
    fclose(cfp);
    fclose(dfp);

    return 0;
}

/*----------------------------------------------------------------------------
 * printTokensCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::printTokensCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    for(int i = 0; i < tokens.length(); i++)
    {
        mlog(RAW, "%s\n", tokens[i].getString());
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * printKeysCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::printKeysCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    char** keys = NULL;
    int numkeys = dictionary.getKeys(&keys);

    for(int i = 0; i < numkeys; i++)
    {
        mlog(RAW, "%s\n", keys[i]);
        delete [] keys[i];
    }

    delete [] keys;

    return 0;
}

/*----------------------------------------------------------------------------
 * printPacketsCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::printPacketsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    for(int p = 0; p < packets.length(); p++)
    {
        Packet* packet = packets[p];
        if(packet->isPrototype() == false)
        {
            if(packet->isType(Packet::COMMAND))
            {
                mlog(RAW,"[COMMAND] %s(%d)\n", packet->getName(), packet->getNumBytes());
            }
            else if(packet->isType(Packet::TELEMETRY))
            {
                mlog(RAW,"[TELEMETRY] %s(%d)\n", packet->getName(), packet->getNumBytes());
            }
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * printFiltersCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::printFiltersCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    for(int p = 0; p < filters.length(); p++)
    {
        Filter* entry = filters[p];
        mlog(RAW, "%s\t%s: [%s,%s]\n", entry->getProperty("sid"), entry->getProperty("fsw_define"), entry->getProperty("q"), entry->getProperty("spw"));
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * generateReportCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::generateReportCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    char*   reporttemplate  = argv[0];
    char*   summarytemplate = argv[1];
    char*   outputpath      = argv[2];

    generateReport(reporttemplate, summarytemplate, outputpath);

    return 0;
}

/*----------------------------------------------------------------------------
 * generateDocsCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::generateDocsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    char*   documenttemplate = argv[0];
    char*   outputpath       = argv[1];

    generateDocuments(documenttemplate, outputpath);

    return 0;
}

/*----------------------------------------------------------------------------
 * reportFullCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::reportFullCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    optFullPktDetails = enable;

    return 0;
}

/*----------------------------------------------------------------------------
 * makeEditableCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::makeEditableCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    optUserEditable = enable;

    return 0;
}

/*----------------------------------------------------------------------------
 * useRemoteContentCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::useRemoteContentCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    optRemoteContent = enable;

    return 0;
}

/*----------------------------------------------------------------------------
 * listCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
int ItosRecordParser::listCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    char* display_name = NULL;
    const int max_result_size = 65536;
    SafeString result(max_result_size);
    bool packet_found = false;

    const char* pktname = argv[0];
    Packet* packet = NULL;

    for(int p = 0; p < packets.length(); p++)
    {
        packet = packets[p];
        if(packet->isPrototype() == false)
        {
            if(packet->isType(Packet::TELEMETRY))
            {
                if(strcmp(pktname, packet->getName()) == 0)
                {
                    packet_found = true;
                    break;
                }
                else if(strstr(packet->getName(), pktname) != NULL)
                {
                    result += packet->getName();
                    result += "\n";
                }
            }
        }
    }

    if(packet_found)
    {
        int numfields = packet->getNumFields();
        for(int f = 0; f < numfields; f++)
        {
            Field* field = (Field*)packet->getField(f);
            if(field->getLengthInBits() % 8 != 0)
            {
                mlog(RAW, "%-32s BITFIELD[%lX]\n", field->getDisplayName(display_name), field->getBitMask());
            }
            else if(field->getNumElements() > 1)
            {
                mlog(RAW, "%-32s %s[%d] (%d %d %d %d)\n", field->getDisplayName(display_name), field->getType(), field->getNumElements(), field->getLengthInBits(), field->getBaseSizeInBits(), field->getByteOffset(), field->getByteSize());
            }
            else
            {
                mlog(RAW, "%-32s %s (%d %d %d %d)\n", field->getDisplayName(display_name), field->getType(), field->getLengthInBits(), field->getBaseSizeInBits(), field->getByteOffset(), field->getByteSize());
            }
            delete [] display_name;
        }
    }
    else
    {
        mlog(RAW, "%s", result.getString(false));
    }

    return 0;
}
