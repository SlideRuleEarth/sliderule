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

#ifndef __atlas_histogram__
#define __atlas_histogram__

#include "atlasdefines.h"
#include "MajorFrameProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class AtlasHistogram: public RecordObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int NUM_MAX_BINS  = 3;
        static const int MAX_HIST_SIZE = 10000;
        static const int GPS_STR_SIZE = 32;

        static const double HISTOGRAM_DEFAULT_FILTER_WIDTH;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            NAS     = -1,   // Not Applicable as Science
            SAL     = 0,    // Strong Altimetric Histogram Telemetry
            WAL     = 1,    // Weak Altimetric Histogram Telemetry
            SAM     = 2,    // Strong Atmospheric Histogram Telemetry
            WAM     = 3,    // Weak Atmospheric Histogram Telemetry
            STT     = 4,    // Strong Time Tag Science Data
            WTT     = 5,    // Weak Time Tag Science Data
            SHS     = 6,    // Strong HSTVS Simulated Waveforms
            WHS     = 7,    // Weak HSTVS Simulator Waveforms
            NUM_TYPES = 8   // Total number of scidata types
        } type_t;

        typedef struct {
            type_t      type;
            int         integrationPeriod;
            double      binSize;

            int         pceNum;
            long        majorFrameCounter;
            bool        majorFramePresent;
            mfdata_t    majorFrameData;

            char        gpsString[GPS_STR_SIZE];
            double      gpsAtMajorFrame;
            double      rangeWindowStart;
            double      rangeWindowWidth;

            int         transmitCount;
            double      noiseFloor;
            double      noiseBin;
            double      signalRange;
            double      signalWidth;
            double      signalEnergy;
            double      tepEnergy;

            int         pktBytes;
            int         pktErrors;

            int         ignoreStartBin; // >= (i.e. inclusive)
            int         ignoreStopBin;  // <  (i.e. exclusive)

            int         maxVal[NUM_MAX_BINS];
            int         maxBin[NUM_MAX_BINS];

            int         beginSigBin;
            int         endSigBin;

            int         size;
            int         sum;
            int         bins[MAX_HIST_SIZE];
        } hist_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const double             histogramBias[NUM_TYPES];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            AtlasHistogram          (const char* _rec_type, type_t _type,
                                                     int _intperiod, double _binsize,
                                                     int _pcenum, long _mfc, mfdata_t* _mfdata,
                                                     double _gps, double _rws, double _rww);
        virtual             ~AtlasHistogram         (void);

        bool                setBin                  (int bin, int val);
        bool                addBin                  (int bin, int val);
        bool                incBin                  (int bin);

        int                 getSum                  (void);
        double              getMean                 (void);
        double              getStdev                (void);
        int                 getMin                  (int start_bin=0, int stop_bin=-1);
        int                 getMax                  (int start_bin=0, int stop_bin=-1);
        int                 getSumRange             (int start_bin, int stop_bin);

        void                scale                   (double scale);
        void                addScalar               (int scalar);

        int                 getSize                 (void);
        int                 operator[]              (int index);

        void                setIgnore               (int start, int stop);
        void                setPktBytes             (int bytes);
        int                 addPktBytes             (int bytes);
        void                setPktErrors            (int errors);
        int                 addPktErrors            (int errors);
        void                setTransmitCount        (int count);
        int                 addTransmitCount        (int count);
        void                setTepEnergy            (double energy);

        type_t              getType                 (void);
        int                 getIntegrationPeriod    (void);
        double              getBinSize              (void);
        int                 getPceNum               (void);
        long                getMajorFrameCounter    (void);
        bool                isMajorFramePresent     (void);
        const mfdata_t*     getMajorFrameData       (void);
        double              getGpsAtMajorFrame      (void);
        double              getRangeWindowStart     (void);
        double              getRangeWindowWidth     (void);
        int                 getTransmitCount        (void);
        double              getNoiseFloor           (void);
        double              getNoiseBin             (void);
        double              getSignalRange          (void);
        double              getSignalWidth          (void);
        double              getSignalEnergy         (void);
        double              getTepEnergy            (void);
        int                 getPktBytes             (void);
        int                 getPktErrors            (void);

        static type_t       str2type                (const char* str);
        static const char*  type2str                (type_t type);

        virtual bool        calcAttributes          (double sigwid, double bincal); // returns if signal is found

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        hist_t* hist;

        /*--------------------------------------------------------------------
         * Method
         *--------------------------------------------------------------------*/

        static recordDefErr_t   defineHistogram     (const char* rec_type, int data_size, fieldDef_t* fields, int num_fields);
};

#endif  /* __atlas_histogram__ */
