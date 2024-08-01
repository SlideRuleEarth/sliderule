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

#define __STDC_FORMAT_MACROS

#include "AtlasFileWriter.h"
#include "TimeTagProcessorModule.h"
#include "TimeTagHistogram.h"
#include "TimeProcessorModule.h"
#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
AtlasFileWriter::AtlasFileWriter(CommandProcessor* cmd_proc, const char* obj_name, atlas_fmt_t _fmt, const char* _prefix, const char* inq_name, unsigned int _max_file_size):
    CcsdsFileWriter(cmd_proc, obj_name, CcsdsFileWriter::USER_DEFINED, _prefix, inq_name, _max_file_size)
{
    atlas_fmt = _fmt;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
AtlasFileWriter::~AtlasFileWriter(void)
{
}

/*----------------------------------------------------------------------------
 * createCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* AtlasFileWriter::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    atlas_fmt_t format = str2fmt(argv[0]);
    const char* prefix = argv[1];
    const char* stream = StringLib::checkNullStr(argv[2]);

    if(format == INVALID)
    {
        mlog(CRITICAL, "Error: invalid format specified for atlas file writer %s", name);
        return NULL;
    }

    unsigned int filesize = CcsdsFileWriter::FILE_MAX_SIZE;
    if(argc == 4)
    {
        mlog(WARNING, "Truncating file size to maximum allowed: %d", CcsdsFileWriter::FILE_MAX_SIZE);
        filesize = (unsigned int)strtol(argv[3], NULL, 0);
    }

    return new AtlasFileWriter(cmd_proc, name, format, prefix, stream, filesize);
}

/*----------------------------------------------------------------------------
 * str2fmt  -
 *----------------------------------------------------------------------------*/
AtlasFileWriter::atlas_fmt_t AtlasFileWriter::str2fmt(const char* str)
{
         if(strcmp(str, "SCI_PKT") == 0)    return SCI_PKT;
    else if(strcmp(str, "SCI_CH") == 0)     return SCI_CH;
    else if(strcmp(str, "SCI_TX") == 0)     return SCI_TX;
    else if(strcmp(str, "HISTO") == 0)      return HISTO;
    else if(strcmp(str, "CCSDS_STAT") == 0) return CCSDS_STAT;
    else if(strcmp(str, "CCSDS_INFO") == 0) return CCSDS_INFO;
    else if(strcmp(str, "META") == 0)       return META;
    else if(strcmp(str, "CHANNEL") == 0)    return CHANNEL;
    else if(strcmp(str, "AVCPT") == 0)      return AVCPT;
    else if(strcmp(str, "TIMEDIAG") == 0)   return TIMEDIAG;
    else if(strcmp(str, "TIMESTAT") == 0)   return TIMESTAT;
    else                                    return INVALID;
}

/*----------------------------------------------------------------------------
 * str2fmt  -
 *----------------------------------------------------------------------------*/
const char* AtlasFileWriter::fmt2str(AtlasFileWriter::atlas_fmt_t _fmt)
{
         if(_fmt == SCI_PKT)    return "SCI_PKT";
    else if(_fmt == SCI_CH)     return "SCI_CH";
    else if(_fmt == SCI_TX)     return "SCI_TX";
    else if(_fmt == HISTO)      return "HISTO";
    else if(_fmt == CCSDS_STAT) return "CCSDS_STAT";
    else if(_fmt == CCSDS_INFO) return "CCSDS_INFO";
    else if(_fmt == META)       return "META";
    else if(_fmt == CHANNEL)    return "CHANNEL";
    else if(_fmt == AVCPT)      return "AVCPT";
    else if(_fmt == TIMEDIAG)   return "TIMEDIAG";
    else if(_fmt == TIMESTAT)   return "TIMESTAT";
    else                        return "INVALID";
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * writeMsg -
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeMsg(void* msg, int size, bool with_header)
{
    switch(atlas_fmt)
    {
        case SCI_PKT:       return writeSciPkt(msg, size, with_header);
        case SCI_CH:        return writeSciCh(msg, size, with_header);
        case SCI_TX:        return writeSciTx(msg, size, with_header);
        case HISTO:         return writeHisto(msg, size, with_header);
        case CCSDS_STAT:    return writeCcsdsStat(msg, size, with_header);
        case CCSDS_INFO:    return writeCcsdsInfo(msg, size, with_header);
        case META:          return writeHistoMeta(msg, size, with_header);
        case CHANNEL:       return writeHistoChannel(msg, size, with_header);
        case AVCPT:         return writeAVCPT(msg, size, with_header);
        case TIMEDIAG:      return writeTimeDiag(msg, size, with_header);
        case TIMESTAT:      return writeTimeStat(msg, size, with_header);
        default:            return -1; // should never happen

    }
}

/*----------------------------------------------------------------------------
 * isBinary -
 *----------------------------------------------------------------------------*/
bool AtlasFileWriter::isBinary(void)
{
    switch(atlas_fmt)
    {
        case SCI_PKT:       return false;
        case SCI_CH:        return false;
        case SCI_TX:        return false;
        case HISTO:         return false;
        case CCSDS_STAT:    return false;
        case CCSDS_INFO:    return false;
        case META:          return false;
        case CHANNEL:       return false;
        case AVCPT:         return false;
        case TIMEDIAG:      return false;
        case TIMESTAT:      return false;
        default:            return false; // should never happen
    }
}

/*----------------------------------------------------------------------------
 * writeSciPkt
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeSciPkt (void* msg, int size, bool with_header)
{
    (void)size;

    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "PCE,    SEGCNT,   PKTCNT,   SEQ,   LEN,   ODD,   MFC,   SEG,   HDR,   FMT,   DLB,   TAG,   PKT,   WARN,   MINTAGS,   MAXTAGS,   AVGTAGS");
    }

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    pktStat_t* stat = (pktStat_t*)data;
    cnt += fprintf(outfp, "%6u,   ",  stat->pce);
    cnt += fprintf(outfp, "%6u,   ",  stat->segcnt);
    cnt += fprintf(outfp, "%6u,   ",  stat->pktcnt);
    cnt += fprintf(outfp, "%3u,   ",  stat->mfc_errors);
    cnt += fprintf(outfp, "%3u,   ",  stat->hdr_errors);
    cnt += fprintf(outfp, "%3u,   ",  stat->fmt_errors);
    cnt += fprintf(outfp, "%3u,   ",  stat->dlb_errors);
    cnt += fprintf(outfp, "%3u,   ",  stat->tag_errors);
    cnt += fprintf(outfp, "%3u,   ",  stat->pkt_errors);
    cnt += fprintf(outfp, "%4u,   ",  stat->warnings);
    cnt += fprintf(outfp, "%7u,   ",  stat->min_tags);
    cnt += fprintf(outfp, "%7u,   ",  stat->max_tags);
    cnt += fprintf(outfp, "%.1lf,   ", stat->avg_tags);
    cnt += fprintf(outfp, "\n");

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeSciCh
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeSciCh (void* msg, int size, bool with_header)
{
    (void)size;

    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "PCE,  CH,   STATCNT,   NUMTAGS,   NUMDUPR,   TDCCALR,   MINCALR,   MAXCALR,   AVGCALR,   NUMDUPF,   TDCCALF,   MINCALF,   MAXCALF,   AVGCALF\n");
    }

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    chStat_t* chstat = (chStat_t*)data;
    for(int channel = 0; channel < NUM_CHANNELS; channel++)
    {
        cnt += fprintf(outfp, "%2u,   ",     chstat->pce + 1);
        cnt += fprintf(outfp, "%2d,   ",     channel + 1);
        cnt += fprintf(outfp, "%7u,   ",     chstat->statcnt);
        cnt += fprintf(outfp, "%7u,   ",     chstat->rx_cnt[channel]);
        cnt += fprintf(outfp, "%7u,   ",     chstat->num_dupr[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->tdc_calr[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->min_calr[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->max_calr[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->avg_calr[channel]);
        cnt += fprintf(outfp, "%7u,   ",     chstat->num_dupf[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->tdc_calf[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->min_calf[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->max_calf[channel]);
        cnt += fprintf(outfp, "%7.1lf,   ",  chstat->avg_calf[channel]);
        cnt += fprintf(outfp, "\n");
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeSciTx
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeSciTx (void* msg, int size, bool with_header)
{
    (void)size;

    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "PCE,    STATCNT,   TXCNT,  sMINTAGS,  sMAXTAGS,  sAVGTAGS,  sSTDTAGS,  wMINTAGS,  wMAXTAGS,  wAVGTAGS,  wSTDTAGS,   MINDELTA,   MAXDELTA,   AVGDELTA\n");
    }

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    txStat_t* stat = (txStat_t*)data;
    cnt += fprintf(outfp, "%7u,   ",     stat->pce + 1);
    cnt += fprintf(outfp, "%7u,   ",     stat->statcnt);
    cnt += fprintf(outfp, "%5u,   ",     stat->txcnt);
    cnt += fprintf(outfp, "%6u,   ",     stat->min_tags[STRONG_SPOT]);
    cnt += fprintf(outfp, "%7u,   ",     stat->max_tags[STRONG_SPOT]);
    cnt += fprintf(outfp, "%7.1lf,   ",  stat->avg_tags[STRONG_SPOT]);
    cnt += fprintf(outfp, "%7.1lf,   ",  stat->std_tags[STRONG_SPOT]);
    cnt += fprintf(outfp, "%6u,   ",     stat->min_tags[WEAK_SPOT]);
    cnt += fprintf(outfp, "%7u,   ",     stat->max_tags[WEAK_SPOT]);
    cnt += fprintf(outfp, "%7.1lf,   ",  stat->avg_tags[WEAK_SPOT]);
    cnt += fprintf(outfp, "%7.1lf,   ",  stat->std_tags[WEAK_SPOT]);
    cnt += fprintf(outfp, "%8.5lf,   ",  stat->min_delta);
    cnt += fprintf(outfp, "%8.5lf,   ",  stat->max_delta);
    cnt += fprintf(outfp, "%8.5lf,   ",  stat->avg_delta);
    cnt += fprintf(outfp, "\n");

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeHisto
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeHisto (void* msg, int size, bool with_header)
{
    (void)with_header; // NULL msg used as header indication
    (void)size;

    int cnt = 0;

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    AtlasHistogram::hist_t* hist = (AtlasHistogram::hist_t*)data;
    for(int i = 0; i < hist->size; i++)
    {
        cnt += fprintf(outfp, "%5d,", hist->bins[i]);
    }
    cnt += fprintf(outfp, "\n");

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeCcsdsStat
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeCcsdsStat (void* msg, int size, bool with_header)
{
    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "TPKTS,    TBYTE,    TDROP,    PKTS,  BYTES, ERRS,  MAXBPS, MINBPS, AVGBPS\n");
    }

    const unsigned char* data = NULL;
    int typelen = RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    CcsdsPacketParser::pktStats_t* stat = (CcsdsPacketParser::pktStats_t*)data;
    if((size - typelen) == sizeof(CcsdsPacketParser::pktStats_t))
    {
        cnt += fprintf(outfp, "%6u,   ",  stat->total_pkts);
        cnt += fprintf(outfp, "%6u,   ",  stat->total_bytes);
        cnt += fprintf(outfp, "%6u,   ",  stat->pkts_dropped);
        cnt += fprintf(outfp, "%3u,   ",  stat->curr_pkts);
        cnt += fprintf(outfp, "%3u,   ",  stat->curr_bytes);
        cnt += fprintf(outfp, "%.1lf,   ",  stat->max_bps);
        cnt += fprintf(outfp, "%.1lf,   ",  stat->min_bps);
        cnt += fprintf(outfp, "%.1lf,   ",  stat->avg_bps);
        cnt += fprintf(outfp, "\n");
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeCcsdsInfo
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeCcsdsInfo (void* msg, int size, bool with_header)
{
    (void)with_header; // NULL msg used as header indication
    (void)size;

    unsigned char* pkt_buffer = (unsigned char*)msg;
    CcsdsSpacePacket ccsdspkt(pkt_buffer, size);
    TimeLib::gmt_time_t gmt_time = ccsdspkt.getCdsTimeAsGmt();

    /* Print Info */
    int bytes = 0;
    bytes += fprintf(outfp, "[%02d:%03d:%02d:%02d:%02d] ", gmt_time.year, gmt_time.doy, gmt_time.hour, gmt_time.minute, gmt_time.second);
    bytes += fprintf(outfp, "APID: %04X, SEG: %s, SEQ: %d, LEN: %d >> ", ccsdspkt.getAPID(), CcsdsSpacePacket::seg2str(ccsdspkt.getSEQFLG()), ccsdspkt.getSEQ(), ccsdspkt.getLEN());
    for(int i = 0; i < size; i++) bytes += fprintf(outfp, "%02X", pkt_buffer[i]);
    bytes += fprintf(outfp, "\n");

    return bytes;
}

/*----------------------------------------------------------------------------
 * writeHistoMeta
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeHistoMeta (void* msg, int size, bool with_header)
{
    (void)size;

    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "GPS,MFC,PCE,TYPE,RWS,RWW,DLBW1,DLBW2,DLBW3,DLBW4,SIGRNG,BKGND,SIGPES,SIGWID,HISTSUM,TXCNT,MBPS,TXERR,WRERR,STTDC,WKTDC,RWDERR,SDRMERR,MFCERR,HDRERR,FMTERR,DLBERR,TAGERR,PKTERR,DLBS1,DLBS2,DLBS3,DLBS4\n");
    }

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    if( !RecordObject::isType((unsigned char*)msg, size, "TagHist") ) return 0;

    TimeTagHistogram::ttHist_t* hist = (TimeTagHistogram::ttHist_t*)data;
    mfdata_t* mfdata = &hist->hist.majorFrameData;
    const TimeTagHistogram::band_t* dlb = hist->downlinkBands;
    TimeTagHistogram::stat_t* stat = &hist->pktStats;

    char gps_str[MAX_STR_SIZE];
    long gps_ms = (long)(hist->hist.gpsAtMajorFrame * 1000.0);
    TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(gps_ms);
    StringLib::format(gps_str, MAX_STR_SIZE, "%d:%d:%d:%d:%d:%d", gmt.year, gmt.doy, gmt.hour, gmt.minute, gmt.second, gmt.millisecond);

    cnt += fprintf(outfp, "%s,",   gps_str);
    cnt += fprintf(outfp, "%ld,",  hist->hist.majorFrameCounter);
    cnt += fprintf(outfp, "%d,",   hist->hist.pceNum + 1);
    cnt += fprintf(outfp, "%d,",   hist->hist.type);
    cnt += fprintf(outfp, "%.1lf,",hist->hist.rangeWindowStart);
    cnt += fprintf(outfp, "%.1lf,",hist->hist.rangeWindowWidth);
    cnt += fprintf(outfp, "%d,",   dlb[0].width);
    cnt += fprintf(outfp, "%d,",   dlb[1].width);
    cnt += fprintf(outfp, "%d,",   dlb[2].width);
    cnt += fprintf(outfp, "%d,",   dlb[3].width);
    cnt += fprintf(outfp, "%.1lf,",hist->hist.signalRange);
    cnt += fprintf(outfp, "%.1lf,",hist->hist.noiseFloor);
    cnt += fprintf(outfp, "%.1lf,",hist->hist.signalEnergy);
    cnt += fprintf(outfp, "%.1lf,",hist->hist.signalWidth);
    cnt += fprintf(outfp, "%d,",   hist->hist.sum);
    cnt += fprintf(outfp, "%d,",   hist->hist.transmitCount);
    cnt += fprintf(outfp, "%d,",   (hist->hist.pktBytes * 8) * 50);
    cnt += fprintf(outfp, "%d,",   mfdata->DidNotFinishTransfer_Err);
    cnt += fprintf(outfp, "%d,",   mfdata->DidNotFinishWritingData_Err);
    cnt += fprintf(outfp, "%d,",   mfdata->TDC_StrongPath_Err);
    cnt += fprintf(outfp, "%d,",   mfdata->TDC_WeakPath_Err);
    cnt += fprintf(outfp, "%d,",   mfdata->RangeWindowDropout_Err);
    cnt += fprintf(outfp, "%d,",   mfdata->SDRAMMismatch_Err);
    cnt += fprintf(outfp, "%d,",   stat->mfc_errors);
    cnt += fprintf(outfp, "%d,",   stat->hdr_errors);
    cnt += fprintf(outfp, "%d,",   stat->fmt_errors);
    cnt += fprintf(outfp, "%d,",   stat->dlb_errors);
    cnt += fprintf(outfp, "%d,",   stat->tag_errors);
    cnt += fprintf(outfp, "%d,",   stat->pkt_errors);
    cnt += fprintf(outfp, "%d,",   dlb[0].start);
    cnt += fprintf(outfp, "%d,",   dlb[1].start);
    cnt += fprintf(outfp, "%d,",   dlb[2].start);
    cnt += fprintf(outfp, "%d,",   dlb[3].start);

    cnt += fprintf(outfp, "\n");

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeHistoChannel
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeHistoChannel (void* msg, int size, bool with_header)
{
    (void)size;

    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "MFC,    PCE,    TYPE,   RWS,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20\n");
    }

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    TimeTagHistogram::ttHist_t* hist = (TimeTagHistogram::ttHist_t*)data;
    if((hist->hist.type == AtlasHistogram::STT) || (hist->hist.type == AtlasHistogram::WTT))
    {
        cnt += fprintf(outfp, "%-7ld,",  hist->hist.majorFrameCounter);
        cnt += fprintf(outfp, "%-7d,",   hist->hist.pceNum + 1);
        if(hist->hist.type == AtlasHistogram::STT) cnt += fprintf(outfp, "STT,");
        if(hist->hist.type == AtlasHistogram::WTT) cnt += fprintf(outfp, "WTT,");
        cnt += fprintf(outfp, "%-7.0lf,",hist->hist.rangeWindowStart);
        for(int i = 0; i < NUM_CHANNELS; i++) cnt += fprintf(outfp, "%-3d,", hist->channelCounts[i]);
        cnt += fprintf(outfp, "\n");
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeAVCPT
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeAVCPT (void* msg, int size, bool with_header)
{
    (void)size;

    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s\n", "MFC", "PCE", "TYPE", "RWS", "RWW", "TOF", "BKGND", "SIGPES", "TXCNT");
    }

    const unsigned char* data = NULL;
    RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    AtlasHistogram::hist_t* hist = (AtlasHistogram::hist_t*)data;
    if(hist->type == AtlasHistogram::STT || hist->type == AtlasHistogram::WTT)
    {
        cnt += fprintf(outfp, "%12ld,",  hist->majorFrameCounter);
        cnt += fprintf(outfp, "%12d,",   hist->pceNum + 1);
        cnt += fprintf(outfp, "%12d,",   hist->type);
        cnt += fprintf(outfp, "%12.0lf,",hist->rangeWindowStart);
        cnt += fprintf(outfp, "%12.0lf,",hist->rangeWindowWidth);
        cnt += fprintf(outfp, "%12.1lf,",hist->signalRange);
        cnt += fprintf(outfp, "%12.3lf,",hist->noiseFloor);
        cnt += fprintf(outfp, "%12.3lf,",hist->signalEnergy);
        cnt += fprintf(outfp, "%12d,",   hist->transmitCount);
        cnt += fprintf(outfp, "\n");
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeTimeDiag
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeTimeDiag (void* msg, int size, bool with_header)
{
    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s\n", "REF", "TIME_REF", "SC_1PPS", "SC_TAT_RX", "SC_ATT_RX", "SC_POS_RX", "SC_ATT_SOL", "SC_POS_SOL", "SXP_PCE1_TIME_RX", "SXP_PCE2_TIME_RX", "SXP_PCE3_TIME_RX", "SXP_1ST_MF1_EXTRAP", "SXP_1ST_MF2_EXTRAP", "SXP_1ST_MF3_EXTRAP", "PCE1_1ST_MF_AFTER_1PPS", "PCE2_1ST_MF_AFTER_1PPS", "PCE3_1ST_MF_AFTER_1PPS", "SXP_STATUS");
    }

    const unsigned char* data = NULL;
    int typelen = RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    timeDiagStat_t* timediag = (timeDiagStat_t*)data;
    if((size - typelen) == sizeof(timeDiagStat_t))
    {
        cnt += fprintf(outfp, "%12s,",   timediag->ref == TIME_REF_ASC_1PPS_GPS ? "GPS" : "AMET");
        cnt += fprintf(outfp, "%12lf,",  timediag->asc_1pps_gps_ref);
        cnt += fprintf(outfp, "%12lf,",  timediag->sc_1pps_delta);
        cnt += fprintf(outfp, "%12lf,",  timediag->sc_tat_rx_delta);
        cnt += fprintf(outfp, "%12lf,",  timediag->sc_att_rx_delta);
        cnt += fprintf(outfp, "%12lf,",  timediag->sc_pos_rx_delta);
        cnt += fprintf(outfp, "%12lf,",  timediag->sc_att_sol_delta);
        cnt += fprintf(outfp, "%12lf,",  timediag->sc_pos_sol_delta);
        cnt += fprintf(outfp, "%12lf,",  timediag->sxp_pce_time_rx_delta[0]);
        cnt += fprintf(outfp, "%12lf,",  timediag->sxp_pce_time_rx_delta[1]);
        cnt += fprintf(outfp, "%12lf,",  timediag->sxp_pce_time_rx_delta[2]);
        cnt += fprintf(outfp, "%12lf,",  timediag->sxp_1st_mf_extrap_delta[0]);
        cnt += fprintf(outfp, "%12lf,",  timediag->sxp_1st_mf_extrap_delta[1]);
        cnt += fprintf(outfp, "%12lf,",  timediag->sxp_1st_mf_extrap_delta[2]);
        cnt += fprintf(outfp, "%12lf,",  timediag->pce_1st_mf_1pps_delta[0]);
        cnt += fprintf(outfp, "%12lf,",  timediag->pce_1st_mf_1pps_delta[1]);
        cnt += fprintf(outfp, "%12lf,",   timediag->pce_1st_mf_1pps_delta[2]);
        const int num_sxp_status = 11;
        if(timediag->sxp_status[0] >= 0 && timediag->sxp_status[0] < num_sxp_status)
        {
            const char* sxp_status[num_sxp_status] = { "Unknown", "Good", "Not_Enabled", "Could_Not_Run", "Spot_At_TQ_Failed", "Spot_Velocity_Failed", "Range_Velocity_Failed", "Off_Nadir_Velocity_Failed", "Params_Failed", "Failed", "Timeout" };
            cnt += fprintf(outfp, "%12s\n",  sxp_status[timediag->sxp_status[0]]);
        }
        else
        {
            cnt += fprintf(outfp, "%12s\n", "OUT_OF_BOUNDS");
        }
    }

    return cnt;
}

/*----------------------------------------------------------------------------
 * writeTimeStat
 *----------------------------------------------------------------------------*/
int AtlasFileWriter::writeTimeStat (void* msg, int size, bool with_header)
{
    int cnt = 0;

    if(with_header)
    {
        cnt += fprintf(outfp, "%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s,%12s\n", "SC_1PPS_FREQ", "ASC_1PPS_FREQ", "TQ_FREQ", "SC_1PPS_TIME", "ASC_1PPS_TIME", "TQ_TIME", "SC_1PPS_AMET", "ASC_1PPS_AMET", "SC2ASC_AMET_DELTA");
    }

    const unsigned char* data = NULL;
    int typelen = RecordObject::parseSerial((unsigned char*)msg, size, NULL, &data);
    if(!data) return 0;

    timeStat_t* timestat = (timeStat_t*)data;
    if((size - typelen) == sizeof(timeStat_t))
    {
        cnt += fprintf(outfp, "%12lf,",  timestat->sc_1pps_freq);
        cnt += fprintf(outfp, "%12lf,",  timestat->asc_1pps_freq);
        cnt += fprintf(outfp, "%12lf,",  timestat->tq_freq);
        cnt += fprintf(outfp, "%12lf,",  timestat->sc_1pps_time);
        cnt += fprintf(outfp, "%12lf,",  timestat->asc_1pps_time);
        cnt += fprintf(outfp, "%12lf,",  timestat->tq_time);
        cnt += fprintf(outfp, "%12lld,", (long long)timestat->sc_1pps_amet);
        cnt += fprintf(outfp, "%12lld,", (long long)timestat->asc_1pps_amet);
        cnt += fprintf(outfp, "%12lld\n",(long long)timestat->sc_to_asc_1pps_amet_delta);
    }

    return cnt;
}
