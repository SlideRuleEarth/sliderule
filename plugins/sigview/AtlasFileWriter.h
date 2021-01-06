#ifndef __atlas_file_writer__
#define __atlas_file_writer__

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class AtlasFileWriter: public CcsdsFileWriter
{
    public:

        typedef enum {
            SCI_PKT,
            SCI_CH,
            SCI_TX,
            HISTO,
            CCSDS_STAT,
            CCSDS_INFO,
            META,
            CHANNEL,
            AVCPT,
            TIMEDIAG,
            TIMESTAT,
            INVALID
        } fmt_t;

                                    AtlasFileWriter     (CommandProcessor* cmd_proc, const char* name, fmt_t _fmt, const char* _prefix, const char* inq_name, unsigned int _max_file_size=FILE_MAX_SIZE);
        virtual                     ~AtlasFileWriter    (void);

        static CommandableObject*   createObject        (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        static fmt_t                str2fmt             (const char* str);
        static const char*          fmt2str             (fmt_t _fmt);

    protected:

        int     writeMsg        (void* msg, int size, bool with_header=false);
        bool    isBinary        (void);

    private:

        fmt_t   fmt;

        int writeSciPkt         (void* record, int size, bool with_header=false);
        int writeSciCh          (void* record, int size, bool with_header=false);
        int writeSciTx          (void* record, int size, bool with_header=false);
        int writeHisto          (void* record, int size, bool with_header=false);
        int writeCcsdsStat      (void* record, int size, bool with_header=false);
        int writeCcsdsInfo      (void* record, int size, bool with_header=false);
        int writeHistoMeta      (void* record, int size, bool with_header=false);
        int writeHistoChannel   (void* record, int size, bool with_header=false);
        int writeAVCPT          (void* record, int size, bool with_header=false);
        int writeTimeDiag       (void* record, int size, bool with_header=false);
        int writeTimeStat       (void* record, int size, bool with_header=false);
};

#endif  /* __atlas_file_writer__ */
