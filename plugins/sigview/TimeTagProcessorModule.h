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

#ifndef __time_tag_processor_module__
#define __time_tag_processor_module__

#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************/
/* RECORD CLASSES */
/******************/

/* --- PktStat --- */

typedef struct {
    uint32_t  pce;        // set in post function
    uint32_t  segcnt;
    uint32_t  pktcnt;
    uint32_t  mfc_errors;
    uint32_t  hdr_errors;
    uint32_t  fmt_errors;
    uint32_t  dlb_errors;
    uint32_t  tag_errors;
    uint32_t  pkt_errors;
    uint32_t  warnings;
    uint32_t  sum_tags;   // excludes transmit tags
    uint32_t  min_tags;   // excludes transmit tags
    uint32_t  max_tags;   // excludes transmit tags
    double    avg_tags;   // excludes transmit tags; this is a snapshot average updated every major frame
} pktStat_t;

class PktStat: public StatisticRecord<pktStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        PktStat (CommandProcessor* cmd_proc, const char* stat_name);

};

/* --- ChStat --- */

typedef struct {
    uint32_t  pce;
    uint32_t  statcnt;
    uint32_t  rx_cnt[NUM_CHANNELS];
    uint32_t  num_dupr[NUM_CHANNELS];
    uint32_t  num_dupf[NUM_CHANNELS];
    uint32_t  cell_cnts[NUM_CHANNELS][MAX_FINE_COUNT];
    double    tdc_calr[NUM_CHANNELS];   // ps
    double    min_calr[NUM_CHANNELS];   // ps
    double    max_calr[NUM_CHANNELS];   // ps
    double    avg_calr[NUM_CHANNELS];   // ps
    double    tdc_calf[NUM_CHANNELS];   // ps
    double    min_calf[NUM_CHANNELS];   // ps
    double    max_calf[NUM_CHANNELS];   // ps
    double    avg_calf[NUM_CHANNELS];   // ps
    double    bias[NUM_CHANNELS];       // ps
    double    dead_time[NUM_CHANNELS];  // ps
} chStat_t;

class ChStat: public StatisticRecord<chStat_t>
{
    public:

        static const int MAX_FIELD_NAME_SIZE = 64;

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        ChStat  (CommandProcessor* cmd_proc, const char* stat_name);

        static void defineRecord (void);
};

/* --- TxStat --- */

typedef struct {
    uint32_t  pce;
    uint32_t  statcnt;
    uint32_t  txcnt;
    uint32_t  min_tags[NUM_SPOTS];
    uint32_t  max_tags[NUM_SPOTS];
    double  avg_tags[NUM_SPOTS];
    double  std_tags[NUM_SPOTS];
    double  min_delta;
    double  max_delta;
    double  avg_delta;
} txStat_t;

class TxStat: public StatisticRecord<txStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        TxStat (CommandProcessor* cmd_proc, const char* stat_name);
};

/* --- SigStat --- */

typedef struct {
    uint32_t  pce;
    uint32_t  statcnt;
    double  rws[NUM_SPOTS];
    double  rww[NUM_SPOTS];
    double  sigrng[NUM_SPOTS];
    double  bkgnd[NUM_SPOTS];
    double  sigpes[NUM_SPOTS];
    double  teppe[NUM_SPOTS];
} sigStat_t;

class SigStat: public StatisticRecord<sigStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        SigStat (CommandProcessor* cmd_proc, const char* stat_name);
};

/*******************/
/* PROCESSOR CLASS */
/*******************/

class TimeTagProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int    INVALID_MFC_OFFSET              = -1;
        static const int    INVALID_INDEX                   = -1;
        static const int    NUM_ALT_BINS_PER_PKT            = 500;
        static const int    NUM_ATM_BINS_PER_PKT            = 467;
        static const int    NUM_ALT_SEGS_PER_PKT            = 4;
        static const int    NUM_MF_TO_BUFF                  = 256;
        static const int    MAX_RX_PER_SHOT                 = 1000;
        static const int    MAX_STAT_NAME_SIZE              = 128;
        static const int    GRANULE_HIST_SIZE               = 2000;
        static const double DEFAULT_10NS_PERIOD;
        static const double DEFAULT_SIGNAL_WIDTH;
        static const double DEFAULT_GPS_TOLERANCE;
        static const double DEFAULT_TEP_LOCATION;
        static const double DEFAULT_TEP_WIDTH;

        static const int    TransmitPulseCoarseCorrection   = -1;     // 100MHz clocks (this is a correction to T0 and does not take into account start pulse delay onto the board)
        static const int    ReturnPulseCoarseCorrection     = -1;     // 100MHz clocks
        static const double DetectorDeadTime;
        static const double MaxFineTimeCal;
        static const double MinFineTimeCal;
        static const double DefaultTimeTagBinSize;

        static const char*  fullColumnIntegrationKey;
        static const char*  autoSetTrueRulerClkPeriodKey;
        static const char*  blockTepKey;
        static const char*  lastGpsKey;
        static const char*  lastGpsMfcKey;
        static const char*  signalWidthKey;
        static const char*  tepLocationKey;
        static const char*  tepWidthKey;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            FALLING_EDGE = 0,
            RISING_EDGE = 1,
            NUM_LVPECL_EDGE_TYPES = 2
        } lvpecl_t;

        /* Time Tag Structures */

        typedef struct {
            uint32_t    tag;
            uint8_t     toggle;
            uint8_t     band;
            int16_t     coarse;
            uint8_t     fine;
            uint8_t     channel;
            bool        duplicate;
            double      calval;
            double      range;
        } rxPulse_t;

        typedef struct {
            uint32_t    tag;
            uint8_t     width;
            uint8_t     trailing_fine;
            int16_t     leading_coarse;
            uint8_t     leading_fine;
            uint16_t    return_count[NUM_SPOTS];
            double      time;
        } txPulse_t;

        typedef struct {
            uint32_t  mask;
            uint16_t  start;
            uint16_t  width;
        } dlb_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

	                    TimeTagProcessorModule  (CommandProcessor* cmd_proc, const char* obj_name, int pcenum, const char* histq_name, const char* txtimeq_name);
                        ~TimeTagProcessorModule (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            txPulse_t           tx;
            rxPulse_t           rx[MAX_RX_PER_SHOT];
            int                 rx_index;
            List<rxPulse_t*>    rx_list[NUM_LVPECL_EDGE_TYPES][NUM_CHANNELS];
            bool                truncated;
        } shot_data_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            RemoveDuplicates;
        double          TrueRulerClkPeriod;
        double          SignalWidth;
        bool            FullColumnIntegration;
        bool            AutoSetTrueRulerClkPeriod;
        double          GpsAccuracyTolerance;
        double          TepLocation;
        double          TepWidth;
        bool            BlockTep;
        double          TimeTagBinSize;         // in meters
        double          LastGps;
        long            LastGpsMfc;
        bool            BuildUpMfc;
        long            BuildUpMfcCount;

        int             pce;

        PktStat*        pktStat;
        ChStat*         chStat;
        TxStat*         txStat;
        SigStat*        sigStat;

        bool            channelDisable[NUM_CHANNELS]; // when the mask is set, it disables the channel (default is false, which has all channels enabled)

        const char*     majorFrameProcName;
        const char*     timeProcName;
        const char*     timeStatName;

        FILE*           resultFile;

        Publisher*      histQ;    // output histograms
        Publisher*      txTimeQ;  //output absolute Tx times

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments         (List<CcsdsSpacePacket*>& segments, int numpkts);

        int     removeDuplicatesCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int     setClkPeriodCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int     setSignalWidthCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     setCorrectionCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int     fullColumnModeCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     ttBinsizeCmd            (int argc, char argv[][MAX_CMD_SIZE]);
        int     chDisableCmd            (int argc, char argv[][MAX_CMD_SIZE]);
        int     autoSetRulerClkCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int     setTepLocationCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     blockTepCmd             (int argc, char argv[][MAX_CMD_SIZE]);
        int     buildUpMfcCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachMFProcCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachTimeProcCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     startResultFileCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int     stopResultFileCmd       (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __time_tag_processor_module__ */
