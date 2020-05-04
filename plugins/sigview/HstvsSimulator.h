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

#ifndef __hstvs_simulator__
#define __hstvs_simulator__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <vector>

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define NUM_STRONG_RX_CHANNELS      16
#define NUM_WEAK_RX_CHANNELS        4
#define NUM_TX_CHANNELS             2
#define SHOTS_PER_MAJOR_FRAME       200
#define SHOTS_PER_SECOND            10000

#define CLOCK_OUT_PERIOD            0.000000005
#define PROB_BIN_PERIOD             0.000000010
#define HISTO_BIN_PERIOD            0.000000020
#define NUM_PROB_BINS_IN_15KM       ((int)((15000 / (PROB_BIN_PERIOD * 3.0 / 0.000000020)) + 0.5))
#define NUM_PROB_BINS_IN_1SEC       (NUM_PROB_BINS_IN_15KM * SHOTS_PER_SECOND)
#define PROB_BUFFER_SIZE            (NUM_PROB_BINS_IN_1SEC * 2)
#define NUM_TICKS_PER_PROB_BIN      ((uint8_t)((PROB_BIN_PERIOD / CLOCK_OUT_PERIOD) + 0.5))

#define TX_OFFSET                   5           // Always put transmit pulse at beginning of 15KM column
#define TX_FLAGS                    3           // Always transmit both the leading and trailing transmit pulse

#define TEP_DELAY_DEFAULT           0.000000100 // seconds
#define TEP_STRENGTH_DEFAULT        0.1         // pe

#define DEFAULT_RVGS_SEED           0x9E3A31F1  // 5F12E0BB
#define PRNG_MODULUS                2147483647  // DON'T CHANGE THIS VALUE
#define PRNG_MULTIPLIER             48271       // DON'T CHANGE THIS VALUE
#define NUM_LFSRS                   20
#define LFSR_CYCLE_CNT              1

#define NUMBER_14BIT_MODES          4
#define NUM_RX_PER_TESTINPUT        3

#define NUMBER_BINS_PER_SHOT        10000
#define NUM_RX_CHANNELS             20

#define DEFAULT_TX_OFFSET           5           // The value we need to put TX offset to make the HSTVS work properly
#define DEFAULT_TX_FLAGS            3           // Always transmit both the leading and trailing transmit pulse

#define NUM_PED_BITS                14
#define INVALID_SPOT                (-1)

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef struct {
    uint32_t        range;                      // Signal range  : (from laser fire to the center of the return surface echo) - in ns.
    double          energy_pe;                  // Signal energy : (a) photoelectrons
    double          energy_fj;                  // Signal energy : (b) femtoJoules
    double          energy_fjm2;                // Signal energy : (c) femtoJoules/sq.meter
    uint32_t        width;                      // Signal width  : in nanoseconds.
} sig_ret_t;

typedef struct {
    double          met;                                    // Time offset from the start time - in seconds and fractions of seconds.  LSB = 0.0001s.
    sig_ret_t       signal_return[NUM_RX_PER_TESTINPUT];    // Signal Returns - ground, canopy, cloud
    uint32_t        noise_offset;                           // Noise start: Delay from fire to start of noise – in nanoseconds. (This was previously behind noise data).
    double          noise_rate_pes;                         // Background noise: (a) pes/sec (instr+optical)
    double          noise_rate_w;                           // Background noise: (b) Watts (optical)
    double          noise_rate_wm2;                         // Background noise: (c) Watts/sq.m (optical)
    int8_t          spot;                                   // strong or weak spot: used in the code, is NOT supplied in the test data files
} test_input_t;

/******************************************************************************
 * PED PROBABILITY ENCODER
 ******************************************************************************/

class PedProbabilityEncoder
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        PedProbabilityEncoder();
        ~PedProbabilityEncoder();

        uint8_t         encodeProbability       ( int mode, double probability );
        void            generateTables          ( int InternalPedValueBitSize, const int *ExponentBitTable, int NumberModes, const uint16_t *pModeCommandBits );
        int             determineModeToUse      ( double *Probabilities, int NumberProbabilities );
        int             getModeFromCommandBits  ( uint16_t CommandBits );
        unsigned int    decodeProbabiltyValue   ( int mode, uint8_t EncodedProbabilityValue );

        uint8_t         tableValue              ( int mode, int index ) { return ModeEncodeProbabilityTable[mode][index]; }
        uint16_t        getModeCommandBits      ( int mode ) { return ModeCommandBits[mode]; }

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        std::vector<int> HighestBitSet;

        // these values are given to the GenerateTables() routine
        int iNumberInternalPedBits;
        int iNumberModes;
        std::vector<int> ModeNumberExponentBits;
        std::vector<uint16_t> ModeCommandBits;

        // these values are generated by the GenerateTables() routine
        int NumberValues;
        unsigned int MaxValue;

        std::vector<int> ModeNumberMantissaBits;
        std::vector<int> ModeMaxMantissa;
        std::vector<int> ModeMaxExponent;
        std::vector<int> ModeMaxShiftValue;
        std::vector<unsigned int> ModeMaxValue;
        std::vector<double> ModeHighestRepresentableProbability;

        // index by [mode][scaledProbability]
        std::vector< std::vector<uint8_t> > ModeEncodeProbabilityTable;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        uint8_t   encodeTargetValue       (unsigned int targetValue, int mode );
        void    generateHighestBitSet   (void);
};

/******************************************************************************
 * TEST INPUT LIST
 ******************************************************************************/

class TestInputList: public MgList<test_input_t*>
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                TestInputList   (void);
        bool    loadInputs      (const char* strong_input_filename, const char* weak_input_filename);


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool isValid;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const int FILENAME_MAX_CHARS =   64;
        static const int TEST_DATA_ATOMS    =   20;
        static const int MAX_TOKEN_SIZE     =   1024;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        int             readTextLine        (FILE* fd, char* linebuf, int maxsize);
        int             tokenizeTextLine    (char* str, int str_size, char separator, int numtokens, char tokens[][MAX_TOKEN_SIZE]);
        test_input_t*   getNextInputEntry   (FILE* fp, int spot);
};

/******************************************************************************
 * HSTVS SIMULATOR
 ******************************************************************************/

class HstvsSimulator: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Probability Curve Format */
        typedef struct
        {
            uint8_t   prob[NUM_SPOTS];
        } bin_prob_t;

        /* HS-TVS Output Command Format
         * The rx_prob.prob value is the
         * probability of 1 photon on
         * 1 channel in 5 ns */
        typedef struct
        {
                    uint32_t      seed[NUM_RX_CHANNELS];              // start seed for each psuedo-random number generator dedicated to each of the 20 channels
                    uint16_t      tmet[3];                            // tvs-met of the start of the rx probabilities - 10ns
                    uint16_t      tx_offset;                          // offset in tvs-met ticks from the tvs-met above of the tx pulse
                    uint16_t      tx_flags;                           // Bit[9..8] is decode probability mode, Bit[0]: leading pulse; Bit[1]: trailing pulse
                    uint32_t      ch_enables;                         // Bit[23..16] --> Ch[7..0], Bit[15..8] --> Ch[23..16], Bit[7..0] --> Ch[15..8]
                    bin_prob_t  rx_prob[NUMBER_BINS_PER_SHOT];      // Bits[15..8]: strong spot probability; Bits[7..0]: weak spot probability
        }
#ifdef _GNU_
        __attribute__ ((packed))
#endif
        ped_command_output_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const uint8_t  MaxChannelsPerSpot[NUM_SPOTS];
        static const int    Mode14bit_NumberExponentBits[NUMBER_14BIT_MODES];
        static const uint16_t Mode14bit_PedModeCommandBits[NUMBER_14BIT_MODES];

        static const unsigned long StrongChannelOutMask[NUM_STRONG_RX_CHANNELS + 1];
        static const unsigned long WeakChannelOutMask[NUM_WEAK_RX_CHANNELS + 1];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        TestInputList       testInput;
        Publisher*          histQ;
        long                rgvsCurrentValue;
        bool                useLehmer;
        bool                enableSimulation;

        uint8_t             channelsPerSpot[NUM_SPOTS];
        bool                dynamicChannelsPerSpot[NUM_SPOTS];
        uint32_t            channelEnableOverride;
        bool                overrideChannelEnable;

        double              tepDelay;
        double              tepStrength;

        PedProbabilityEncoder PedEncoder;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                HstvsSimulator              (CommandProcessor* cmd_proc, const char* cmdq_name, const char* histq_name);
                ~HstvsSimulator             (void);

        double  rgvsRandom                  (void);
        void    rgvsPutSeed                 (long x);
        uint32_t  LFSR32                      (uint32_t cval);
        uint32_t  LEHMER32                    (void);

        void    generateSimulatedOutput14   (ped_command_output_t* cmdout, int64_t gpsMet);
        void    writeCommandOutput          (int64_t met, double prob_curve[NUM_SPOTS][NUM_PROB_BINS_IN_15KM],  int32_t start_bin, int32_t num_bins);
        void    populateProbCurve           (test_input_t* input, double prob_curve[NUM_SPOTS][NUM_PROB_BINS_IN_15KM], int32_t start_bin, int32_t num_bins);
        void    generateCommands            (void);

        int     generateCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int     loadCmd                     (int argc, char argv[][MAX_CMD_SIZE]);
        int     clearInputCmd               (int argc, char argv[][MAX_CMD_SIZE]);
        int     simulateCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int     setNumberChannelsCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int     overrideChannelMaskCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int     configureTepCmd             (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __hstvs_simulator__ */
