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
 * TODO: the provided range is from tx, but the current code only modulos the
 *       range into the current event buffer
 * TODO: currently there is a constraint that the two spots cannot have inputs
 *       that are separated by more than 1 second... this should give a warning
 *       when this is detected
 * TODO: noise offset
 * TODO: the pdf is calculated at discrete points for signal, this should be
 *       changed to be a cdf so that there are no rounding errors
 * TODO: Consider using the PedProbabilityEncoder for the 12-bit values also
 */

/******************************************************************************
 *INCLUDES
 ******************************************************************************/

#define _USE_MATH_DEFINES
#include <math.h>

#include "AltimetryHistogram.h"
#include "HstvsSimulator.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * PED PROBABILITY ENCODER PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
PedProbabilityEncoder::PedProbabilityEncoder() {}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
PedProbabilityEncoder::~PedProbabilityEncoder() {}

/*----------------------------------------------------------------------------
 * generateTables  -
 *  Called to generate all of the tables used to make a quick probability to 8-bit PED command value.
 *
 * InternalPedValueBitSize:
 *  The number of bits that the internal HSTVS generates for its comparision values.
 *
 * ExponentBitTables:
 *  Each of the 8-bit PED command values is 8-bits. They have exponent bits and mantissa bits.
 *  Theses are the number of exponent bits in each mode.
 *  NOTE: The values should be fed in with the smallest number of bits to the largest number of bits.
 *  Example : {1,2,3,4}
 *
 * NumberModes:
 *  The number of shift modes that the PED supports in the 8-bit PED command.
 *----------------------------------------------------------------------------*/
void PedProbabilityEncoder::generateTables( int InternalPedValueBitSize, const int *ExponentBitTable, int NumberModes, const uint16_t *pModeCommandBits)
{
    int mode, i;

    // set values base on the parameters
    iNumberInternalPedBits = InternalPedValueBitSize;
    iNumberModes = NumberModes;

    ModeNumberExponentBits.resize(NumberModes);
    ModeCommandBits.resize(NumberModes);
    for( i=0; i<NumberModes; i++ )
    {
        ModeNumberExponentBits[i] = ExponentBitTable[i];
        ModeCommandBits[i] = pModeCommandBits[i];
    }

    // based on bit size calculate number of internal values
    NumberValues = (1<<iNumberInternalPedBits);
    MaxValue = NumberValues - 1;

    generateHighestBitSet();

    // resize the vectors we will use
    ModeNumberMantissaBits.resize(NumberModes);
    ModeMaxMantissa.resize(NumberModes);
    ModeMaxExponent.resize(NumberModes);
    ModeMaxShiftValue.resize(NumberModes);
    ModeMaxValue.resize(NumberModes);
    ModeHighestRepresentableProbability.resize(NumberModes);

    ModeEncodeProbabilityTable.resize(NumberModes);
    for( i=0; i<NumberModes; i++ )
    {
        ModeEncodeProbabilityTable[i].resize(NumberValues);
    }

    for( mode=0; mode<NumberModes; mode++ )
    {
        // derive some basic values based on the number of exponent bits for this mode
        ModeNumberMantissaBits[mode] = 8 - ModeNumberExponentBits[mode];
        ModeMaxMantissa[mode] = (1 << ModeNumberMantissaBits[mode]) - 1;
        ModeMaxExponent[mode] = (1 << ModeNumberExponentBits[mode]) - 1;

        ModeMaxShiftValue[mode] = (1 << ModeNumberExponentBits[mode]) - 1;

        if( ModeNumberMantissaBits[mode] + ModeMaxShiftValue[mode] > iNumberInternalPedBits )
        {
            ModeMaxValue[mode] = MaxValue;
        }
        else
        {
            ModeMaxValue[mode] = (ModeMaxMantissa[mode] << ModeMaxShiftValue[mode]);
            if( ModeMaxValue[mode] > MaxValue ) ModeMaxValue[mode] = MaxValue;
        }

        ModeHighestRepresentableProbability[mode] = double(ModeMaxValue[mode]) / NumberValues;

        // generate the encode table
        for( i=0; i<NumberValues; i++ )
        {
            ModeEncodeProbabilityTable[mode][i] = encodeTargetValue( i, mode );
        }
    }

}

/*----------------------------------------------------------------------------
 * encodeTargetValue  -
 *----------------------------------------------------------------------------*/
uint8_t PedProbabilityEncoder::encodeTargetValue(unsigned int targetValue, int mode )
{
    if( targetValue == 0 ) return 0;
    if( targetValue > ModeMaxValue[mode] ) return 0xFF;

    uint8_t  encodedValue;

    int highBitNumber = HighestBitSet[targetValue];

    // determine the exponent and mantissa
    int numberShifts = highBitNumber - ModeNumberMantissaBits[mode] + 1;
    if( numberShifts < 0 )
        numberShifts = 0;
    else if( numberShifts > ModeMaxExponent[mode] )
        numberShifts = ModeMaxExponent[mode];

    // check if we need to round up the mantissa. which may make a need for a new exponent
    if( numberShifts > 0 )
    {
        unsigned int halfBit = (targetValue>>(numberShifts-1)) & 1;
        // round up by adding the 0.5 before we shift
        // this sounds dumb but makes it easier to generate a value that changes a n-bit number to a n+1-bit number.
        // example: if we need to round 7 (binary 111) we get 8 (binary 1000)
        if( halfBit )
        {
            // need to round up the mantissa
            targetValue += (1<<numberShifts);
        }
    }

    // redetermine mantissa/expononent on the rounded number
    highBitNumber = HighestBitSet[targetValue];

    // determine the exponent and mantissa
    numberShifts = highBitNumber - ModeNumberMantissaBits[mode] + 1;
    if( numberShifts < 0 )
        numberShifts = 0;
    else if( numberShifts > ModeMaxExponent[mode] )
        numberShifts = ModeMaxExponent[mode];

    encodedValue = (numberShifts<<ModeNumberMantissaBits[mode]) | ((targetValue>>numberShifts) & ModeMaxMantissa[mode]);

    return encodedValue;
}

/*----------------------------------------------------------------------------
 * generateHighestBitSet  -
 *----------------------------------------------------------------------------*/
void PedProbabilityEncoder::generateHighestBitSet()
{
    if( int(HighestBitSet.size()) != NumberValues )
        HighestBitSet.resize( NumberValues );

    int index;
    int bitNumber;
    int nextBitMask;

    index = 0;
    for( bitNumber = -1; bitNumber<iNumberInternalPedBits; bitNumber++ )
    {
        nextBitMask = 1 << (bitNumber + 1);
        while( index < nextBitMask )
        {
            HighestBitSet[index] = bitNumber;
            index++;
        }
    }
}

/*----------------------------------------------------------------------------
 * determineModeToUse  -
 *----------------------------------------------------------------------------*/
int PedProbabilityEncoder::determineModeToUse( double *Probabilities, int NumberProbabilities )
{
    int i, mode;

    if( iNumberModes <= 1 )
        return 0;

    // find the largest probability
    double maxProbability = Probabilities[0];

    for( i=1; i<NumberProbabilities; i++ )
    {
        if( Probabilities[i] > maxProbability )
            maxProbability = Probabilities[i];
    }

    // now see what mode we should use based on the max probability we need to encode.
    // look for a mode whose highest probablity is more than the maxProbability.
    // Keep in mind that ModeHighestRepresentableProbability[] is ordered so that we go from
    // smallest to largest probability values
    for( mode=0; mode<iNumberModes; mode++ )
    {
        if( ModeHighestRepresentableProbability[mode] > maxProbability )
            return mode;
    }

    // return mode with largest representable probability
    return iNumberModes - 1;
}

/*----------------------------------------------------------------------------
 * encodeProbability  -
 *----------------------------------------------------------------------------*/
uint8_t PedProbabilityEncoder::encodeProbability( int mode, double probability )
{
    int scaledProbability = (int)(probability * NumberValues);
    // make sure that any non-zero probability is represented by at least the smallest integer probability
    if( scaledProbability == 0 )
    {
        if( probability > 0.0 )
            scaledProbability = 1;
    }
    return ModeEncodeProbabilityTable[mode][scaledProbability];
}

/*----------------------------------------------------------------------------
 * getModeFromCommandBits  -
 *----------------------------------------------------------------------------*/
int PedProbabilityEncoder::getModeFromCommandBits( uint16_t CommandBits )
{
    unsigned int mode;
    for( mode=0; mode<ModeCommandBits.size(); mode++ )
    {
        if( (CommandBits & 0x300) == ModeCommandBits[mode] )
            return mode;
    }

    // return something even though we didn't see a mode we recognized
    return 0;
}

/*----------------------------------------------------------------------------
 * decodeProbabiltyValue  -
 *----------------------------------------------------------------------------*/
unsigned int PedProbabilityEncoder::decodeProbabiltyValue( int mode, uint8_t EncodedProbabilityValue )
{
    int MantissaMask = (1<<ModeNumberMantissaBits[mode]) - 1;
    unsigned int Mantissa = EncodedProbabilityValue & MantissaMask;
    unsigned int Exponent = EncodedProbabilityValue >> ModeNumberMantissaBits[mode];
    return Mantissa << Exponent;
}

/******************************************************************************
 * TEST INPUT LIST PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
TestInputList::TestInputList(void) {}

/*----------------------------------------------------------------------------
 * loadInputs  -
 *----------------------------------------------------------------------------*/
bool TestInputList::loadInputs(const char* strong_input_filename, const char* weak_input_filename)
{
    FILE*           strong_fp = NULL;
    FILE*           weak_fp = NULL;
    char            line[MAX_TOKEN_SIZE];
    char            tokens[TEST_DATA_ATOMS][MAX_TOKEN_SIZE];

    int i;

    // Open Strong File //
    if(strong_input_filename != NULL)
    {
        strong_fp = fopen(strong_input_filename, "r");
        if(strong_fp == NULL)
        {
            mlog(CRITICAL, "unable to open HSTVS strong spot test data input file: %s\n", strong_input_filename);
            return false;
        }
        else
        {
            mlog(INFO, "loading strong input: %s\n", strong_input_filename);
        }
    }

    // Open Weak File //
    if(weak_input_filename != NULL)
    {
        weak_fp = fopen(weak_input_filename, "r");
        if(weak_fp == NULL)
        {
            mlog(CRITICAL, "unable to open HSTVS weak spot test data input file: %s\n", weak_input_filename);
            return false;
        }
        else
        {
            mlog(INFO, "loading weak input: %s\n", weak_input_filename);
        }
    }

    // First line of the file:
    //      FSW/BCE Embedded Sim Data for ATLAS Spot # 1
    if( strong_fp != NULL )
    {
        readTextLine(strong_fp, NULL, MAX_TOKEN_SIZE); //toss line
        readTextLine(strong_fp, line, MAX_TOKEN_SIZE);
        // get the MJD out of it
        char *pColon = strchr( line, ':' );
        if( pColon == NULL )
        {
            mlog(CRITICAL, "No system time found on 2nd line of file: %s\n", strong_input_filename);
            return false;
        }
        i = tokenizeTextLine(pColon, MAX_TOKEN_SIZE, ' ', TEST_DATA_ATOMS, tokens);
        if( i != 4)
        {
            mlog(CRITICAL, "Error parsing system time. Saw %d : %s", i, pColon);
            return false;
        }
    }
    if( weak_fp != NULL )
    {
        readTextLine(weak_fp, NULL, MAX_TOKEN_SIZE);  //toss line
        readTextLine(weak_fp, NULL, MAX_TOKEN_SIZE);  //toss line
    }

    // Read and Parse Input Data //
    test_input_t* strong_input = getNextInputEntry(strong_fp, STRONG_SPOT);
    test_input_t* weak_input = getNextInputEntry(weak_fp, WEAK_SPOT);

    while(strong_input || weak_input)
    {
        int8_t spot = INVALID_SPOT;

        // Determine Which Spot is Next //
        if(!weak_input && strong_input)                 spot = STRONG_SPOT;
        else if(!strong_input && weak_input)            spot = WEAK_SPOT;
        else if(strong_input->met <= weak_input->met)   spot = STRONG_SPOT;
        else if(weak_input->met < strong_input->met)    spot = WEAK_SPOT;

        // Insert Spot's Test Data //
        if(spot == INVALID_SPOT)
        {
            mlog(CRITICAL, "Unable to determine spot for current input\n");
            if(weak_input) delete weak_input;
            if(strong_input) delete strong_input;
            break;
        }
        else if(spot == STRONG_SPOT)
        {
            add(strong_input);
            strong_input = getNextInputEntry(strong_fp, STRONG_SPOT);
        }
        else if(spot == WEAK_SPOT)
        {
            add(weak_input);
            weak_input = getNextInputEntry(weak_fp, WEAK_SPOT);
        }
    }

    // Close Files //
    if(strong_fp != NULL)   fclose(strong_fp);
    if(weak_fp != NULL)     fclose(weak_fp);

    return true;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * readTextLine
 *----------------------------------------------------------------------------*/
int TestInputList::readTextLine(FILE* fd, char* linebuf, int maxsize)
{
    size_t bytes;
    char c;
    int i = 0;

    /* Validate Parameters */
    if(fd == NULL) return -1;

    if(linebuf == NULL)
    {
        /* Check for NULL Line - Just Read It & Drop It */
        do bytes = fread(&c, 1, 1, fd);
        while(bytes == 1 && c != '\n');
    }
    else
    {
        /* Read Line */
        do bytes = fread(&linebuf[i], 1, 1, fd);
        while(bytes == 1 && i < (maxsize-1) && linebuf[i++] != '\n');
        linebuf[i] = '\0'; // null terminate
    }

    /* Return Bytes Read */
    return i;
}

/*----------------------------------------------------------------------------
 * tokenizeTextLine
 *----------------------------------------------------------------------------*/
int TestInputList::tokenizeTextLine(char* str, int str_size, char separator, int numtokens, char tokens[][MAX_TOKEN_SIZE])
{
    int t = 0, i = 0, j = 0;

    if(str == NULL || tokens == NULL) return 0;

    while(t < numtokens)
    {
        while( ((i < str_size) && (str[i] != '\0')) && ((str[i] == separator) || !isgraph(str[i])) ) i++; // find first character
        while( (i < str_size) && (str[i] != '\0') && (str[i] != separator) && isgraph(str[i])) tokens[t][j++] = str[i++]; // copy characters in
        tokens[t][j] = '\0';
        t++;
        if(j == 0) break;
        j = 0;
    }

    return t;
}

/*----------------------------------------------------------------------------
 *  getNextInputEntry -
 *----------------------------------------------------------------------------*/
test_input_t* TestInputList::getNextInputEntry (FILE* fp, int spot)
{
    if(fp == NULL) return NULL;

    char    line[MAX_TOKEN_SIZE];
    char    tokens[TEST_DATA_ATOMS][MAX_TOKEN_SIZE];

    memset(line, 0, MAX_TOKEN_SIZE);
    readTextLine(fp, line, MAX_TOKEN_SIZE);
    if(tokenizeTextLine(line, MAX_TOKEN_SIZE, ' ', TEST_DATA_ATOMS, tokens) != TEST_DATA_ATOMS)
    {
        return NULL;
    }

    test_input_t* entry = new test_input_t;
    entry->spot = spot;

    entry->met                          = strtod (tokens[0], NULL);
    entry->signal_return[0].range       = strtoul(tokens[1], NULL, 0);
    entry->signal_return[0].energy_pe   = strtod (tokens[2], NULL);
    entry->signal_return[0].energy_fj   = strtod (tokens[3], NULL);
    entry->signal_return[0].energy_fjm2 = strtod (tokens[4], NULL);
    entry->signal_return[0].width       = strtoul(tokens[5], NULL, 0);
    entry->signal_return[1].range       = strtoul(tokens[6], NULL, 0);
    entry->signal_return[1].energy_pe   = strtod (tokens[7], NULL);
    entry->signal_return[1].energy_fj   = strtod (tokens[8], NULL);
    entry->signal_return[1].energy_fjm2 = strtod (tokens[9], NULL);
    entry->signal_return[1].width       = strtoul(tokens[10], NULL, 0);
    entry->signal_return[2].range       = strtoul(tokens[11], NULL, 0);
    entry->signal_return[2].energy_pe   = strtod (tokens[12], NULL);
    entry->signal_return[2].energy_fj   = strtod (tokens[13], NULL);
    entry->signal_return[2].energy_fjm2 = strtod (tokens[14], NULL);
    entry->signal_return[2].width       = strtoul(tokens[15], NULL, 0);
    entry->noise_offset                 = strtoul(tokens[16], NULL, 0);
    entry->noise_rate_pes               = strtod (tokens[17], NULL);
    entry->noise_rate_w                 = strtod (tokens[18], NULL);
    entry->noise_rate_wm2               = strtod (tokens[19], NULL);

    return entry;
}

/******************************************************************************
 * HSTVS SIMULATOR CONSTANTS
 ******************************************************************************/

const char*     HstvsSimulator::TYPE = "HstvsSimulator";

const uint8_t     HstvsSimulator::MaxChannelsPerSpot[NUM_SPOTS] = {NUM_STRONG_RX_CHANNELS, NUM_WEAK_RX_CHANNELS};
const int       HstvsSimulator::Mode14bit_NumberExponentBits[NUMBER_14BIT_MODES] = { 1,2,3,4 };
const uint16_t    HstvsSimulator::Mode14bit_PedModeCommandBits[NUMBER_14BIT_MODES] = { 0,0x100,0x200,0x300 };

// Format for the channel mask going out to the HSTVS:
// ssss wwxx   xxxx xxxx   ssss wwss   ssxx ssss   where s - strong channel, w - weak channel
// ------------------------------
// BIT     CH        BIT     CH
//   0      9         16    *
//   1     10         17    *
//   2     11         18    *
//   3     12         19    *
//   4    *         20    *
//   5    *         21    *
//   6      3         22    *
//   7      4         23    *
//   8      1         24    *
//   9      2         25    *
//  10     19         26     17
//  11     20         27     18
//  12      7         28     13
//  13      8         29     14
//  14     15         30      5
//  15     16         31      6
// ------------------------------
const unsigned long HstvsSimulator::StrongChannelOutMask[NUM_STRONG_RX_CHANNELS + 1] =
//  STRONG = 0  STRONG = 1  STRONG = 2  STRONG = 3  STRONG = 4  STRONG = 5  STRONG = 6  STRONG = 7
{ 0x00000000, 0x80000000, 0xC0000000, 0xE0000000, 0xF0000000, 0xF0008000, 0xF000C000, 0xF0000E00,
//  STRONG = 8  STRONG = 9  STRONG = 10 STRONG = 11 STRONG = 12 STRONG = 13 STRONG = 14 STRONG = 15 STRONG = 16
  0xF000F000, 0xF000F200, 0xF000F300, 0xF000F380, 0xF000F3C0, 0xF000F3C8, 0xF000F3CC, 0xF000F3CE, 0xF000F3CF };

const unsigned long HstvsSimulator::WeakChannelOutMask[NUM_WEAK_RX_CHANNELS + 1] =
//   WEAK = 0    WEAK = 1    WEAK = 2    WEAK = 3    WEAK = 4
{ 0x00000000, 0x08000000, 0x0C000000, 0x0C000800, 0x0C000C00 };

/******************************************************************************
 * HSTVS SIMULATOR PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
HstvsSimulator::HstvsSimulator(CommandProcessor* cmd_proc, const char* obj_name, const char* histq_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Initialize Streams */
    histQ = NULL;
    if(histq_name != NULL)
    {
        histQ = new Publisher(histq_name);
    }

    /* Initialize Parameters */
    rgvsCurrentValue                    = DEFAULT_RVGS_SEED;
    useLehmer                           = false;
    overrideChannelEnable               = false;
    dynamicChannelsPerSpot[STRONG_SPOT] = true;
    dynamicChannelsPerSpot[WEAK_SPOT]   = true;
    for(int i = 0; i < NUM_SPOTS; i++)
    {
        channelsPerSpot[i] = MaxChannelsPerSpot[i];
    }

    /* TEP Initialization */
    tepDelay = TEP_DELAY_DEFAULT;
    tepStrength = 0.0; // set by command

    /* Initialize PED Encoder */
    PedEncoder.generateTables( 14, Mode14bit_NumberExponentBits, NUMBER_14BIT_MODES, Mode14bit_PedModeCommandBits );

    /* Initialize TVS Histogram Record */
    AltimetryHistogram::defineHistogram();

    /* Register Commands */
    registerCommand("GENERATE_COMMANDS", (cmdFunc_t)&HstvsSimulator::generateCmd,             0, "");
    registerCommand("LOAD",              (cmdFunc_t)&HstvsSimulator::loadCmd,                -1, "[<strong input filename> <weak input filename>] | [<met> <rng1> <pe1> <w1> <rng2> <pe2> <w2> <rng3> <pe3> <w3> <nr> <spot>]");
    registerCommand("CLEAR_INPUTS",      (cmdFunc_t)&HstvsSimulator::clearInputCmd,           0, "");
    registerCommand("NUMBER_CHANNELS",   (cmdFunc_t)&HstvsSimulator::setNumberChannelsCmd,    2, "<number of strong channels 1 - 16 | 0: dynamic> <number of weak channels 1 - 4 | 0: dynamic>");
    registerCommand("OVERRIDE_CH_MASK",  (cmdFunc_t)&HstvsSimulator::overrideChannelMaskCmd, -1, "<ENABLE <mask> | DISABLE>");
    registerCommand("CONFIGURE_TEP",     (cmdFunc_t)&HstvsSimulator::configureTepCmd,         1, "<ENABLE | DISABLE>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
HstvsSimulator::~HstvsSimulator(void)
{
    delete histQ;
}

/*----------------------------------------------------------------------------
 *  createCmd -
 *----------------------------------------------------------------------------*/
CommandableObject* HstvsSimulator::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    const char* histq = StringLib::checkNullStr(argv[0]);
    return new HstvsSimulator(cmd_proc, name, histq);
}

/******************************************************************************
 * HSTVS SIMULATOR PRIVATE METHODS
 ******************************************************************************/

/* ---------------------------------------------------------------
 *  rgvsRandom -
 *
 *   returns a pseudo-random real number uniformly distributed
 *   between 0.0 and 1.0.
 *----------------------------------------------------------- * */
double HstvsSimulator::rgvsRandom(void)
{
    const long Q = PRNG_MODULUS / PRNG_MULTIPLIER;
    const long R = PRNG_MODULUS % PRNG_MULTIPLIER;

    long t = PRNG_MULTIPLIER * (rgvsCurrentValue % Q) - R * (rgvsCurrentValue / Q);

    if (t > 0)
    {
        rgvsCurrentValue = t;
    }
    else
    {
        rgvsCurrentValue = t + PRNG_MODULUS;
    }

    return ((double) rgvsCurrentValue / PRNG_MODULUS);
}

/* ---------------------------------------------------------------
 *  rgvsPutSeed -
 *
 *   Use this function to set the state of the current random number
 *   generator stream according to the following conventions:
 *        if x > 0 then x is the state (unless too large)
 *        if x < 0 then the state is obtained from the system clock
 *        if x = 0 then the state is to be supplied interactively
 *----------------------------------------------------------- * */
void HstvsSimulator::rgvsPutSeed(long x)
{
    if (x > 0)
    {
        x = x % PRNG_MODULUS; /* correct if x is too large  */
        rgvsCurrentValue = x;
    }
    else
    {
        mlog(CRITICAL, "ERROR: Invalid seed provided %ld\n", x);
    }

    rgvsRandom();	/* fix bug where first call is always the same */
}

/*----------------------------------------------------------------------------
 * LFSR32 -32(0), 22(10), 2(30), 1(31)
 *----------------------------------------------------------------------------*/
uint32_t HstvsSimulator::LFSR32(uint32_t cval)
{
    uint32_t newbit;

    newbit = ~((cval >> 0) ^ (cval >> 10)) & 0x1;
    newbit = ~((cval >> 30) ^ newbit) & 0x1;
    newbit = ~((cval >> 31) ^ newbit) & 0x1;
    newbit = (newbit << 31) & 0x80000000;

    uint32_t nval = ((cval >> 1) & ~0x80000000) | newbit;

    return nval;
}

/*----------------------------------------------------------------------------
 * LEHMER32
 *----------------------------------------------------------------------------*/
uint32_t HstvsSimulator::LEHMER32(void)
{
    return (uint32_t)(rgvsRandom() * 0xFFFFFFFF);
}

/*----------------------------------------------------------------------------
 *  generateSimulatedOutput14 -
 *
 *   Notes: This is the simulation for the newer HSTVS loads that have internal 14-bit
       probability values.
 *----------------------------------------------------------------------------*/
void HstvsSimulator::generateSimulatedOutput14(ped_command_output_t* cmdout, int64_t gpsMet )
{
    static long mfc = 0;

    // Check Parameters //
    assert(cmdout != NULL);

    // Create Histograms:        type         integration,            binsize                        pce     mfc  mfdata              gsp               rws   rww  //
    AltimetryHistogram sthist(AtlasHistogram::SHS, 1, round(HISTO_BIN_PERIOD / 0.000000020 * 3.0), NOT_PCE, mfc++, NULL, double( gpsMet ) / 10000000.0, 0.0, 100000.0);
    AltimetryHistogram wkhist(AtlasHistogram::WHS, 1, round(HISTO_BIN_PERIOD / 0.000000020 * 3.0), NOT_PCE, mfc++, NULL, double( gpsMet ) / 10000000.0, 0.0, 100000.0);

    // Store Seeds For Use In Clocking //
    uint32_t lfsr_cval[NUM_LFSRS];
    for(int i = 0; i < NUM_LFSRS; i++)
    {
        lfsr_cval[i] = cmdout->seed[i];
    }

    // Generate Simulated Histogram //
    for(int shot = 0; shot < SHOTS_PER_MAJOR_FRAME; shot++)
    {
        // Get the encoding mode we use this command
        uint16_t encodeMode = PedEncoder.getModeFromCommandBits( cmdout->tx_flags );

        // Output Clock Signals //
        for(int bin = 0; bin < NUM_PROB_BINS_IN_15KM; bin++)
        {
            uint16_t strong_probability_value = PedEncoder.decodeProbabiltyValue( encodeMode, cmdout->rx_prob[bin].prob[STRONG_SPOT] );
            uint16_t weak_probability_value   = PedEncoder.decodeProbabiltyValue( encodeMode, cmdout->rx_prob[bin].prob[WEAK_SPOT] );

			// Generate rgvsRandom Numbers from LFSRs //
			uint32_t cmp_val[NUM_TICKS_PER_PROB_BIN][NUM_RX_CHANNELS];
			memset(cmp_val, 0, sizeof(cmp_val));

			uint32_t lfsr_index = 0;
			uint32_t bit_offset = 0;
			uint32_t bit_index = 0;
			for(int ch = 0; ch < NUM_RX_CHANNELS; ch++)
			{
				for(int tick = 0; tick < NUM_TICKS_PER_PROB_BIN; tick++)
				{
					for(int bit = 0; bit < NUM_PED_BITS; bit++)
					{
						uint32_t lfsr_val = lfsr_cval[lfsr_index++];
						uint32_t bit_val  = (lfsr_val >> (bit_index++ % 32)) & 1;
						if(lfsr_index == NUM_LFSRS)
						{
							lfsr_index = 0;
							bit_offset++;
							bit_index = bit_offset;
						}
						cmp_val[tick][ch] |= (bit_val << bit);
					}
				}
			}

			// Cycle LFSRs //
			for(int i = 0; i < NUM_LFSRS; i++)
			{
				if(useLehmer == true)	lfsr_cval[i] = LEHMER32();
				else					for(int c = 0; c < LFSR_CYCLE_CNT; c++) lfsr_cval[i] = LFSR32(lfsr_cval[i]);
			}

            // Clock 5ns Ticks //
            for(int tick = 0; tick < NUM_TICKS_PER_PROB_BIN; tick++)
            {
                // Output Strong Channels - 1..16 //
                for(int strong_channel = 0; strong_channel < NUM_STRONG_RX_CHANNELS; strong_channel++)
                {
                    if((StrongChannelOutMask[strong_channel + 1] & cmdout->ch_enables) == StrongChannelOutMask[strong_channel + 1])
                    {
                        if(strong_probability_value > cmp_val[tick][strong_channel])
                        {
                            sthist.incBin(bin / 2);
                        }
                    }
                }

                // Output Strong Channels - 17..20 //
                for(int weak_channel = 0; weak_channel < NUM_WEAK_RX_CHANNELS; weak_channel++)
                {
                    if((WeakChannelOutMask[weak_channel + 1] & cmdout->ch_enables) == WeakChannelOutMask[weak_channel + 1])
                    {
                        if(weak_probability_value >  cmp_val[tick][NUM_STRONG_RX_CHANNELS + weak_channel])
                        {
                            wkhist.incBin(bin / 2);
                        }
                    }
                }
            }
        }
    }

    // Calculate Signal Attributes //
    sthist.calcAttributes(80.0, 10.0);
    wkhist.calcAttributes(80., 10.0);

    // Post Histograms //
    if(histQ != NULL)
    {
        unsigned char* buffer; // reference to serial buffer
        int size;

        size = sthist.serialize(&buffer, RecordObject::REFERENCE);
        histQ->postCopy(buffer, size);

        size = wkhist.serialize(&buffer, RecordObject::REFERENCE);
        histQ->postCopy(buffer, size);
    }
}

/*----------------------------------------------------------------------------
 *  writeCommandOutput -
 *----------------------------------------------------------------------------*/
void HstvsSimulator::writeCommandOutput(int64_t met, double prob_curve[NUM_SPOTS][NUM_PROB_BINS_IN_15KM], int32_t start_bin, int32_t num_bins)
{
    ped_command_output_t cmdout;
    uint16_t decodeProbabiltyModeBits;

    mlog(INFO, "Writing Command Output at met %lld for %d bins\n", (unsigned long long)met, num_bins);

    int32_t end_bin = start_bin + num_bins;
    int32_t bin = start_bin;
    while(bin < end_bin)
    {
        // Write Seeds //
        for(int channel = 0; channel < NUM_RX_CHANNELS; channel++)
        {
            cmdout.seed[channel]   = (uint32_t)(rgvsRandom() * 0xFFFF);
            cmdout.seed[channel] <<= 16;
            cmdout.seed[channel]  |= (uint32_t)(rgvsRandom() * 0xFFFF);
        }

        cmdout.tmet[0] = (met >> 32) & 0xFFFF;
        cmdout.tmet[1] = (met >> 16) & 0xFFFF;
        cmdout.tmet[2] = (met >> 0) & 0xFFFF;

        cmdout.tx_offset = TX_OFFSET;

        // Write Rx Probabilities //
        int decodeProbabiltyMode = PedEncoder.determineModeToUse( &prob_curve[0][0], NUM_PROB_BINS_IN_15KM * NUM_SPOTS );
        decodeProbabiltyModeBits = PedEncoder.getModeCommandBits( decodeProbabiltyMode );

        for(int32_t i = 0; i < NUM_PROB_BINS_IN_15KM; i++)
        {
            cmdout.rx_prob[i].prob[STRONG_SPOT] = PedEncoder.encodeProbability( decodeProbabiltyMode, prob_curve[STRONG_SPOT][bin] );
            cmdout.rx_prob[i].prob[WEAK_SPOT] = PedEncoder.encodeProbability( decodeProbabiltyMode, prob_curve[WEAK_SPOT][bin] );
            bin++;
        }

        // Set Decode Mode //
        cmdout.tx_flags = TX_FLAGS | decodeProbabiltyModeBits;

        // Set Channel Enables //
        cmdout.ch_enables = StrongChannelOutMask[channelsPerSpot[STRONG_SPOT]] | WeakChannelOutMask[channelsPerSpot[WEAK_SPOT]];

        // Produce Simulated Histograms //
        generateSimulatedOutput14(&cmdout, met);    break;

#if 0
        // Product Text File Output Commands //
        {
            static int file_counter = 0;
            char filename[40];
            sprintf(filename, "hstvs_cmd_%03d.txt", file_counter++);
            FILE* fp = fopen(filename, "w");

            if(fp != NULL)
            {
                mlog(INFO, "Writing HS-TVS Command File: %s\n", filename);

                for(int k = 0; k < NUM_RX_CHANNELS; k++)
                {
                    fprintf(fp, "%04X\n", cmdout.seed[k] >> 16);
                    fprintf(fp, "%04X\n", cmdout.seed[k] & 0xFFFF);
                }

                for(int k = 0; k < 3; k++)
                {
                    fprintf(fp, "%04X\n", cmdout.tmet[k]);
                }

                fprintf(fp, "%04X\n", cmdout.tx_offset);
                fprintf(fp, "%04X\n", cmdout.tx_flags);
                fprintf(fp, "%04X\n", cmdout.ch_enables);

                for(int k = 0; k < NUM_PROB_BINS_IN_15KM; k++)
                {
                    fprintf(fp, "%02X%02X\n", cmdout.rx_prob[k].prob[STRONG_SPOT], cmdout.rx_prob[k].prob[WEAK_SPOT]);
                }

                fclose(fp);
            }
            else
            {
                mlog(CRITICAL, "Unable to open HS-TVS Command Output File for writing: %s\n", filename);
            }
        }
#endif
        // Increment Bin to Next 500Hz Location //
        bin += (NUM_PROB_BINS_IN_1SEC / 500);
    }
}

/*----------------------------------------------------------------------------
 *  populateProbCurve -
 *----------------------------------------------------------------------------*/
void HstvsSimulator::populateProbCurve(test_input_t* input, double prob_curve[NUM_SPOTS][NUM_PROB_BINS_IN_15KM], int32_t start_bin, int32_t num_bins)
{
    assert(input != NULL);

    static double event_buffer[NUM_PROB_BINS_IN_15KM];
    static const int max_prob_bits = 14;
    static const int channels_for_bits[NUM_SPOTS][max_prob_bits + 1] =
    {
        // 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
        { 16, 16, 16, 16, 16, 16,  8,  8,  4,  4,  2,  2,  2,  1,  1 },
        {  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  2,  2,  2,  1,  1 }
    };

    #define MAX_EVENT_DELTA 10.0
    #define MIN_EVENT_DELTA 0.0000001

    // Determine Maximum Request Signal Strengths //
    #define MAX_EVENTS_TO_INSPECT (NUM_RX_PER_TESTINPUT + 2) // returns(2) + cloud(1) + noise(1) + zero(1)
    double max_events_per_bin[MAX_EVENTS_TO_INSPECT] = {MAX_EVENT_DELTA, MAX_EVENT_DELTA, MAX_EVENT_DELTA, MAX_EVENT_DELTA, 0.0};
    for(int r = 0; r < NUM_RX_PER_TESTINPUT; r++)
    {
        if(input->signal_return[r].energy_pe != 0)
        {
            if(r != 2)  /* return */ max_events_per_bin[r] = (1.0 / (((((input->signal_return[r].width * 0.5887) / 1000000000.0) / PROB_BIN_PERIOD) / 2.3548200) * sqrt(2.0 * M_PI))) * input->signal_return[r].energy_pe;
            else        /* cloud */  max_events_per_bin[r] = input->signal_return[r].energy_pe / ((input->signal_return[r].width / 1000000000.0) / PROB_BIN_PERIOD);
        }
    }
    max_events_per_bin[MAX_EVENTS_TO_INSPECT - 2] = /* noise */ input->noise_rate_pes * PROB_BIN_PERIOD;
    max_events_per_bin[MAX_EVENTS_TO_INSPECT - 1] = /* zero */ 0.0;

    // Find minimal delta signal strength //
    double min_event_delta = MAX_EVENT_DELTA;
    for(int i = 0; i < MAX_EVENTS_TO_INSPECT; i++)
    {
        for(int j = i + 1; j < MAX_EVENTS_TO_INSPECT; j++)
        {
            double event_delta = fabs(max_events_per_bin[i] - max_events_per_bin[j]);
            if(event_delta > MIN_EVENT_DELTA && event_delta < min_event_delta) min_event_delta = event_delta;
        }
    }

    // Find Bits Needed to Represent Minimal Delta //
    double min_prob_delta = 1.0 - (1.0 / exp(min_event_delta));
    int bits_of_prob_delta = ((int)(log10(1.0 / min_prob_delta) * 3.321928094887362)) + 1; // hardcoded value is 1.0 / log10(2)

    if(bits_of_prob_delta < 0) bits_of_prob_delta = 0;
    else if(bits_of_prob_delta > max_prob_bits) bits_of_prob_delta = max_prob_bits;

    // Set Number of Channels to Use //
    if(dynamicChannelsPerSpot[(int)input->spot])
    {
        channelsPerSpot[(int)input->spot] = channels_for_bits[(int)input->spot][bits_of_prob_delta];
        mlog(RAW, "## %d: %d (%lf %lf %d) (%lf %lf %lf %lf) (%.10lf %.10lf %.10lf %.10lf) ##\n",
                    input->spot, channelsPerSpot[(int)input->spot],
                    min_event_delta, min_prob_delta, bits_of_prob_delta,
                    input->signal_return[0].energy_pe, input->signal_return[1].energy_pe, input->signal_return[2].energy_pe, input->noise_rate_pes,
                    max_events_per_bin[0], max_events_per_bin[1], max_events_per_bin[2], max_events_per_bin[3]);
    }

    // Populate Probability Curve //
    int8_t  spot                           = input->spot;
    int32_t remaining_bins_to_populate     = num_bins;
    int32_t populate_start_bin             = start_bin;
    while(remaining_bins_to_populate > 0)
    {
        // Initialize Event Buffer with Noise //
        double expected_noise_pe = input->noise_rate_pes * PROB_BIN_PERIOD;
        for(int32_t bin = 0; bin < NUM_PROB_BINS_IN_15KM; bin++)
        {
            event_buffer[bin] = expected_noise_pe;
        }

        // Loop Through Returns and Populate Event Buffer //
        for(int32_t return_index = 0; return_index < NUM_RX_PER_TESTINPUT; return_index++)
        {
            if(input->signal_return[return_index].energy_pe == 0.0)
            {
                continue;
            }

            double          range_bins  = fmod(((input->signal_return[return_index].range / 1000000000.0) / PROB_BIN_PERIOD), NUM_PROB_BINS_IN_15KM) + TX_OFFSET;
            const double    step_bin    = 0.1;

            if(return_index == (NUM_RX_PER_TESTINPUT - 1)) // cloud return goes into last slot and is square
            {
                double          width_bins  = ((input->signal_return[return_index].width / 1000000000.0) / PROB_BIN_PERIOD);
                double          cloud_start_bin = range_bins - (0.5 * width_bins);
                double          cloud_stop_bin = range_bins + (0.5 * width_bins);
                double          step_energy = step_bin * (input->signal_return[return_index].energy_pe / width_bins);
                for(double sigbin = cloud_start_bin; sigbin <= cloud_stop_bin; sigbin+=step_bin)
                {
                    event_buffer[((int)sigbin) % NUM_PROB_BINS_IN_15KM] += step_energy;
                }
            }
            else // gaussian return for ground and canopy
            {
                double          std_bins    = (((input->signal_return[return_index].width * 0.5887) / 1000000000.0) / PROB_BIN_PERIOD) / 2.3548200;
                double          gnd_start_bin   = range_bins - (4 * std_bins);
                double          gnd_stop_bin    = range_bins + (4 * std_bins);
                for(double sigbin = gnd_start_bin; sigbin <= gnd_stop_bin; sigbin+=step_bin)
                {
                    double scalar = 1.0 / (std_bins * sqrt(2.0 * M_PI));
                    double exponent = pow(sigbin - range_bins, 2.0) / (2.0 * pow(std_bins, 2.0));
                    double pdf = scalar * exp(-exponent);
                    event_buffer[((int)sigbin) % NUM_PROB_BINS_IN_15KM] += pdf * input->signal_return[return_index].energy_pe * step_bin;
                }
            }
        }

        // Add TEP - single probability bin //
        double tep_offset = tepDelay / PROB_BIN_PERIOD;
        event_buffer[TX_OFFSET + lround(tep_offset)] += tepStrength;

        // Populate Curve //
        int32_t bins_to_populate = MIN(NUM_PROB_BINS_IN_15KM, remaining_bins_to_populate);
        for(int32_t bin = 0; bin < bins_to_populate; bin++)
        {
            // probability of at least one event in 5ns period on a given channel
            double  pe = event_buffer[bin] / (double)NUM_TICKS_PER_PROB_BIN / (double)channelsPerSpot[(int)spot];
            double  p = 1.0 - (1.0 / exp(pe));

            // set probability curve
            prob_curve[(int)spot][(bin + populate_start_bin) % NUM_PROB_BINS_IN_15KM] = p;
        }

        // Update Indices //
        populate_start_bin += bins_to_populate;
        remaining_bins_to_populate -= bins_to_populate;
    }
}

/*----------------------------------------------------------------------------
 *  generateCommands -
 *        The queue we use is 'raw'. So the entire packet (CCSDS header+Secondary header+data)
 *        is generated here. Only the function code is filled in at this end.
 *----------------------------------------------------------------------------*/
void HstvsSimulator::generateCommands(void)
{
    static double prob_curve[NUM_SPOTS][NUM_PROB_BINS_IN_15KM];

    // Loop Through All Inputs //
    int32_t curr_input = 0;
    while(curr_input < testInput.length())
    {
        int8_t    spot    = testInput[curr_input]->spot;
        double  met     = testInput[curr_input]->met;

        mlog(INFO, "Processing Input Number: %d, on Spot: %d, at MET: %lf\n", curr_input, spot, met);

        // Populate Curve with Current Input //
        populateProbCurve(testInput[curr_input], prob_curve, 0, NUM_PROB_BINS_IN_15KM);
        curr_input++;

        // Check for Matching Other Spot //
        if(curr_input < testInput.length() && testInput[curr_input]->spot != spot && testInput[curr_input]->met == met)
        {
            populateProbCurve(testInput[curr_input], prob_curve, 0, NUM_PROB_BINS_IN_15KM);
            curr_input++;
        }

        // Write Output //
        writeCommandOutput((int64_t)(met * (double)100000000), prob_curve, 0, NUM_PROB_BINS_IN_15KM);
    }
}

/*----------------------------------------------------------------------------
 *  generateCmd -
 *----------------------------------------------------------------------------*/
int HstvsSimulator::generateCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    if(testInput.length() <= 0)
    {
        mlog(CRITICAL, "No test inputs loaded!  Cannot generate HS-TVS commands.\n");
        return -1;
    }

    generateCommands();

    return 0;
}

/*----------------------------------------------------------------------------
 *  loadCmd -
 *----------------------------------------------------------------------------*/
int HstvsSimulator::loadCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    bool status = true;

    /* Parameter Files */
    if(argc == 2)
    {
        const char* strong  = StringLib::checkNullStr(argv[0]);
        const char* weak    = StringLib::checkNullStr(argv[1]);

        status = testInput.loadInputs(strong, weak);
    }
    else if(argc == 12)
    {
        test_input_t* test_input = new test_input_t;
        memset(test_input, 0, sizeof(test_input_t));

        test_input->met                         = strtod(argv[0], NULL);
        test_input->signal_return[0].range      = strtol(argv[1], NULL, 0);
        test_input->signal_return[0].energy_pe  = strtod(argv[2], NULL);
        test_input->signal_return[0].width      = strtol(argv[3], NULL, 0);
        test_input->signal_return[1].range      = strtol(argv[4], NULL, 0);
        test_input->signal_return[1].energy_pe  = strtod(argv[5], NULL);
        test_input->signal_return[1].width      = strtol(argv[6], NULL, 0);
        test_input->signal_return[2].range      = strtol(argv[7], NULL, 0);
        test_input->signal_return[2].energy_pe  = strtod(argv[8], NULL);
        test_input->signal_return[2].width      = strtol(argv[9], NULL, 0);
        test_input->noise_rate_pes              = strtod(argv[10], NULL);
        test_input->spot                        = INVALID_SPOT;

        if(strcmp(argv[11], "STRONG") == 0)     test_input->spot = STRONG_SPOT;
        else if(strcmp(argv[11], "WEAK") == 0)  test_input->spot = WEAK_SPOT;

        testInput.add(test_input);
    }
    else
    {
        mlog(CRITICAL, "Unable to perform HSTVS load: the wrong number of parameters supplied!\n");
        status = false;
    }

    if(status)  return 0;
    else        return -1;
}

/*----------------------------------------------------------------------------
 *  clearInputCmd -
 *----------------------------------------------------------------------------*/
int HstvsSimulator::clearInputCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    testInput.clear();

    return 0;
}

/*----------------------------------------------------------------------------
 *  setNumberChannelsCmd -
 *   sets the number of active channels (0 is dynamic)
 *----------------------------------------------------------------------------*/
int HstvsSimulator::setNumberChannelsCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    int nStrongChannels  = strtol(argv[0], NULL, 0);
    int nWeakChannels  = strtol(argv[1], NULL, 0);

    if( (nStrongChannels < 0) || (nStrongChannels > MaxChannelsPerSpot[STRONG_SPOT]) )
    {
        mlog(CRITICAL, "Number strong channels must be in range [0,16]");
        return -1;
    }
    else if (nStrongChannels == 0)
    {
        dynamicChannelsPerSpot[STRONG_SPOT] = true;
    }
    else
    {
        dynamicChannelsPerSpot[STRONG_SPOT] = false;
        channelsPerSpot[STRONG_SPOT] = nStrongChannels;
    }

    if( (nWeakChannels < 0) || (nWeakChannels > MaxChannelsPerSpot[WEAK_SPOT]) )
    {
        mlog(CRITICAL, "Number weak channels must be in range [0,4]");
        return -1;
    }
    else if (nWeakChannels == 0)
    {
        dynamicChannelsPerSpot[WEAK_SPOT] = true;
    }
    else
    {
        dynamicChannelsPerSpot[WEAK_SPOT] = false;
        channelsPerSpot[WEAK_SPOT] = nWeakChannels;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 *  overrideChannelMaskCmd -
 *----------------------------------------------------------------------------*/
int HstvsSimulator::overrideChannelMaskCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    uint32_t mask = 0;
    if(enable)
    {
        if(argc > 1)
        {
            mask = (uint32_t)strtod(argv[1], NULL);
        }
        else
        {
            mlog(CRITICAL, "mask not specified!\n");
            return -1;
        }
    }

    overrideChannelEnable = enable;
    channelEnableOverride = mask;

    return 0;
}

/*----------------------------------------------------------------------------
 *  configure TepCmd -
 *----------------------------------------------------------------------------*/
int HstvsSimulator::configureTepCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    if(enable)  tepStrength = TEP_STRENGTH_DEFAULT;
    else        tepStrength = 0.0;

    return 0;
}
