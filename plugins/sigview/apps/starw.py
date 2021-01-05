#
#   Purpose: process dfc range gate and tx settings to produce software timing analyzer plots
#
#   Example creation of Software Timing Analyzer files:
#       python starw.py range_gates.json
#
#   where range_gates.json is:
#
#       {
#            'TxCoarse':             [4096],
#            'T0Offset':             [3096],
#            'Alt_RWS_Strong':       [323488],
#            'Alt_RWS_Weak':         [324734],
#            'Alt_RWW_Strong':       [2110],
#            'Alt_RWW_Weak':         [854],
#            'Atm_RWS_Strong':       [316260],
#            'Atm_RWS_Weak':         [316260],
#            'Atm_RWW_Strong':       [9340],
#            'Atm_RWW_Weak':         [9340]
#       }
# 
#

import sys
import json
import pandas as pd

###############################################################################
# GLOBALS
###############################################################################

COLOR_MAP = [65535, 12615935, 8454143, 16744703, 65280, 32768, 16777215, 8421376]
PERFIDS = {"sam": 1, "wam": 2, "sal": 3, "wal": 4, "tx": 5, "txoff": 6, "t0": 7, "mf": 8}
EDGES = {True: 0, False: 1 << 14}

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':
    
    if sys.argv[1] == "hardcode":
        
        # Hard Coded Parameters #
        config = {
            "num_major_frames": 1,
            "correct_clocks": False        
        }
        settings = {
            'TxCoarse':             [((0xA5935 & 0x001FFF80) / 128) + 1],
            'T0Offset':             [((0xCC8B5 & 0x001FFF80) / 128) + 1],
            'Alt_RWS_Strong':       [323488],
            'Alt_RWS_Weak':         [324734],
            'Alt_RWW_Strong':       [2110],
            'Alt_RWW_Weak':         [854],
            'Atm_RWS_Strong':       [316260],
            'Atm_RWS_Weak':         [316260],
            'Atm_RWW_Strong':       [9340],
            'Atm_RWW_Weak':         [9340]
        }

    else:
        
        # From JSON File #
        with open(sys.argv[1]) as inputfile:
            inputdict = json.load(inputfile)
        config = inputdict["config"]
        settings = inputdict["settings"]        
            
    # Correct Clocks #
    if config["correct_clocks"]:
        for setting in ["TxCoarse", "T0Offset", "Alt_RWW_Strong", "Alt_RWW_Weak", "Atm_RWW_Strong", "Atm_RWW_Weak"]:
            for index in range(len(settings[setting])):
                settings[setting][index] += 1
        for setting in ["Alt_RWS_Strong", "Alt_RWS_Weak", "Atm_RWS_Strong", "Atm_RWS_Weak"]:
            for index in range(len(settings[setting])):
                settings[setting][index] += 13
    
    # Build Data Frame #
    hc = {}
    for setting in settings:
        hc[setting] = [value for value in settings[setting] for major_frame in range(config["num_major_frames"])]        
    df = pd.DataFrame(hc)    

    # Build Dictionary of all Events #
    t0time = 0
    events = {"time": [], "signal": [], "edge": []}
    for _,row in df.iterrows():        

        # MF Rising #
        events["time"].append(t0time)
        events["signal"].append("mf")
        events["edge"].append(True)

        # MF Falling #
        events["time"].append(t0time + 1)
        events["signal"].append("mf")
        events["edge"].append(False)

        # Loop Through Each Shot in Major Frame #
        for shot in range(200):

            # Calculate Tx #
            txtime = t0time + row["TxCoarse"]

            # T0 Rising #
            events["time"].append(t0time)
            events["signal"].append("t0")
            events["edge"].append(True)

            # T0 Falling #
            events["time"].append(t0time + 1)
            events["signal"].append("t0")
            events["edge"].append(False)

            # Tx Rising #
            events["time"].append(txtime)
            events["signal"].append("tx")
            events["edge"].append(True)

            # Tx Falling #
            events["time"].append(txtime + 1)
            events["signal"].append("tx")
            events["edge"].append(False)

            # Tx Offset Rising #
            events["time"].append(t0time + row["T0Offset"])
            events["signal"].append("txoff")
            events["edge"].append(True)

            # Tx Offset Falling #
            events["time"].append(t0time + row["T0Offset"] + 1)
            events["signal"].append("txoff")
            events["edge"].append(False)

            # SAL Rising #
            events["time"].append(txtime + row['Alt_RWS_Strong'])
            events["signal"].append("sal")
            events["edge"].append(True)

            # SAL Falling #
            events["time"].append(txtime + row['Alt_RWS_Strong'] + row['Alt_RWW_Strong'])
            events["signal"].append("sal")
            events["edge"].append(False)

            # WAL Rising #
            events["time"].append(txtime + row['Alt_RWS_Weak'])
            events["signal"].append("wal")
            events["edge"].append(True)

            # WAL Falling #
            events["time"].append(txtime + row['Alt_RWS_Weak'] + row['Alt_RWW_Weak'])
            events["signal"].append("wal")
            events["edge"].append(False)

            # SAM Rising #
            events["time"].append(txtime + row['Atm_RWS_Strong'])
            events["signal"].append("sam")
            events["edge"].append(True)

            # SAM Falling #
            events["time"].append(txtime + row['Atm_RWS_Strong'] + row['Atm_RWW_Strong'])
            events["signal"].append("sam")
            events["edge"].append(False)

            # WAM Rising #
            events["time"].append(txtime + row['Atm_RWS_Weak'])
            events["signal"].append("wam")
            events["edge"].append(True)

            # WAM Falling #
            events["time"].append(txtime + row['Atm_RWS_Weak'] + row['Atm_RWW_Weak'])
            events["signal"].append("wam")
            events["edge"].append(False)

            # Increment T0 #
            t0time += 10000

    # Create Sorted DataFrame of Events #
    trace = pd.DataFrame(events)
    trace = trace.sort_values("time")
    trace["delta"] = trace["time"].diff()
    trace.at[0, "delta"] = 0.0

    # Find First Range Window #
    first_mf_index = 0
    index = 0
    for _,row in trace.iterrows():
        if row["signal"] == "mf":
            first_mf_index = index
        if row["signal"] != "t0" and row["signal"] != "tx" and row["signal"] != "mf":
            break
        index += 1

    # Write Trace File #
    f = open("rwtrace.txt", "w")
    index = 0
    for _,row in trace[first_mf_index:].iterrows():
        f.write("%08d %08X %.3f ns\n" % (index, PERFIDS[row["signal"]] + EDGES[row["edge"]], row["delta"] * 10))
        index += 1
    f.close()

    # Write Setup File #
    f = open("rwtrace.PerfIDSetup", "w")
    f.write("[PerfID Table]\n")
    f.write("Version=1\n")
    f.write("Auto Hide=True\n")
    f.write("Auto Set Bit For Exit=True\n")
    f.write("Bit To Set For Exit=14\n")
    f.write("Number of PerfIDs=%d\n" % (len(PERFIDS)))
    index = 0
    for signal in PERFIDS:
        perfid = PERFIDS[signal]
        f.write("[PerfID %d]\n" % (index))
        f.write("Name=%s\n" % (signal))
        f.write("Entry ID=%08X\n" % perfid)
        f.write("Exit ID=%08X\n" % (int(perfid + EDGES[False])))
        f.write("Calc CPU=True\n")
        f.write("Color=%d\n" % (COLOR_MAP[index]))
        f.write("Hide=False\n")
        f.write("DuplicateEdgeWarningsDisabled=False\n")
        index += 1
    f.close()
