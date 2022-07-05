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

#ifndef __itos_record_parser__
#define __itos_record_parser__

#include "ItosRecord.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class ItosRecordParser: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject*       createObject    (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        MgDictionary<Itos::Record*>*    getDictionary   (void);
        MgList<Itos::Packet*>*          getPackets      (void);
        const char*                     pkt2str         (unsigned char* packet);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<SafeString> tokens;    // all tokens in rec files

        MgList<Itos::Filter*> filters; // all filter entries in filter table

        MgDictionary<Itos::Record*> dictionary; // key'ed database of all records

        Dictionary<List<Itos::Record*>*> instantiations; // given a system prototype name, gives list of instantiated system records

        MgList<Itos::Record*> declarations;         // zero-depth records in rec files
        MgList<Itos::Packet*> packets;              // list of all the packet definitions: commands, telemetry
        List<Itos::Record*> mnemonics;              // zero-depth list of all the mnemonics definitions
        MgList<Itos::TypeConversion*> conversions;  // zero-depth list of all discrete conversions
        List<Itos::Record*> aliases;                // zero-depth list of all aliases

        bool optFullPktDetails;     // option to show all fields in a packet when generating a report
        bool optUserEditable;       // option to provide edit buttons interactively via mod_python
        bool optRemoteContent;      // option to use local content vs remote iframe content for packet display

        List<Itos::Packet*> cmdPackets[CCSDS_NUM_APIDS];
        List<Itos::Packet*> tlmPackets[CCSDS_NUM_APIDS];
        List<Itos::Mnemonic*> mneDefinitions;


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                ItosRecordParser        (CommandProcessor* cmd_proc, const char* obj_name);
                                ~ItosRecordParser       (void);

        SafeString*             readFile                (const char* fname);
        bool                    parseFilterTbl          (SafeString* fcontents);
        bool                    parseRecTokens          (SafeString* fcontents);
        bool                    isStr                   (int i, const char* str);
        bool                    startStr                (int i, const char* str);
        bool                    checkComment            (int* c_index, Itos::Record* c_record, int* index);
        Itos::Record*           createRecord            (Itos::Record* container, int* index);
        void                    createRecords           (void);
        void                    populatePacket          (Itos::Record* subrec, Itos::Packet* pkt, Itos::Record* conrec, int conindex);
        Itos::Packet*           createPacket            (Itos::Record* declaration, Itos::Packet* pkt, Itos::Record** system_declaration, Itos::Record** struct_declaration, int struct_index);
        void                    createPackets           (void);
        void                    createMnemonics         (void);
        void                    createCmdTlmLists       (void);
        Itos::TypeConversion*   createConversion        (Itos::TypeConversion::type_conv_t _type, Itos::Record* declaration);
        const char*             createPacketDetails     (Itos::Packet* packet);
        const char*             createCTSummary         (const char* pkttype, bool local=true);
        const char*             createCTDetails         (void);
        const char*             createMNSummary         (bool local);
        Itos::Packet*           findPacket              (const char* name);
        Itos::TypeConversion*   findConversion          (const char* name);
        bool                    writeFile               (const char* fname, const char* fcontents, long fsize);
        void                    generateReport          (const char* reporttemplate, const char* summarytemplate, const char* outputpath);
        void                    generateDocuments       (const char* documenttemplate, const char* outputpath);

        int                     loadRecFilesCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int                     loadFilterTblCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int                     applyFilterTblCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int                     setDesignationsCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int                     buildDatabaseCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int                     buildRecordsCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int                     datasrvExportCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int                     printTokensCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int                     printKeysCmd            (int argc, char argv[][MAX_CMD_SIZE]);
        int                     printPacketsCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int                     printFiltersCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int                     generateReportCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int                     generateDocsCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int                     reportFullCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int                     makeEditableCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int                     useRemoteContentCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int                     listCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __itos_record_parser__ */
