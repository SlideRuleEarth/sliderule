#
#   Purpose: process dfc housekeeping telemetry to produce software timing analyzer plots
#
#   Example creation of Software Timing Analyzer files:
#       python starw.py txrx_dfc2.csv
#

import sys
import pandas as pd

###############################################################################
# GLOBALS
###############################################################################

COLOR_MAP = [8421631, 8454143, 8454016, 16777088, 16744703, 16777215, 8421376, 255]
PERFIDS = {"sal": 1, "wal": 2, "sam": 3, "wam": 4, "tx": 5, "t0": 6, "mf": 7, "tri": 8}
EDGES = {True: 0, False: 1 << 14}

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':
    
    # Read Data Frame #
    df = pd.read_csv(sys.argv[1], index_col=False)

    # Build Range Windows #    
    df["SAL.rws"] = (df["A_DFC2.HK.StrongAltRWS[0]"] * 65536) + (df["A_DFC2.HK.StrongAltRWS[1]"] * 256) + df["A_DFC2.HK.StrongAltRWS[2]"]
    df["SAL.rww"] = df["A_DFC2.HK.StrongAltRWW"]
    df["SAM.rws"] = (df["A_DFC2.HK.StrongAtmRWS[0]"] * 65536) + (df["A_DFC2.HK.StrongAtmRWS[1]"] * 256) + df["A_DFC2.HK.StrongAtmRWS[2]"]
    df["SAM.rww"] = df["A_DFC2.HK.StrongAtmRWW"]
    df["WAL.rws"] = (df["A_DFC2.HK.WeakAltRWS[0]"] * 65536) + (df["A_DFC2.HK.WeakAltRWS[1]"] * 256) + df["A_DFC2.HK.WeakAltRWS[2]"]
    df["WAL.rww"] = df["A_DFC2.HK.WeakAltRWW"]
    df["WAM.rws"] = (df["A_DFC2.HK.WeakAtmRWS[0]"] * 65536) + (df["A_DFC2.HK.WeakAtmRWS[1]"] * 256) + df["A_DFC2.HK.WeakAtmRWS[2]"]
    df["WAM.rww"] = df["A_DFC2.HK.WeakAtmRWW"]

    # Build Transmits #
    df["TxTag"] = (df["A_DFC2.HK.LeadingStartTimeTag[0]"] * 65536) + (df["A_DFC2.HK.LeadingStartTimeTag[1]"] * 256) + df["A_DFC2.HK.LeadingStartTimeTag[2]"]    
    df["TxCoarse"] = ((df["TxTag"] & 0x001FFF80) / 128) + 1
    
    # Filter Major Frames and Get Trigger #
    trigger = -1
    if len(sys.argv) > 3:
        df = df[df['A_DFC2.HK.MajorFrameCount'] >= int(sys.argv[2])]
        df = df[df['A_DFC2.HK.MajorFrameCount'] <= int(sys.argv[3])]    
        if len(sys.argv) > 4:
            trigger = int(sys.argv[4])
    elif len(sys.argv) == 3:
        trigger = int(sys.argv[2])

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

        # Check Trigger #
        if row['A_DFC2.HK.MajorFrameCount'] == trigger:

            # Find First Range Window Edge #
            first_rw = min([row["SAL.rws"], row["WAL.rws"], row["SAM.rws"], row["WAM.rws"]])

            # Trigger Rising #
            events["time"].append(t0time + first_rw)
            events["signal"].append("tri")
            events["edge"].append(True)

            # Trigger Falling #
            events["time"].append(t0time + first_rw + 1)
            events["signal"].append("tri")
            events["edge"].append(False)

        # Loop Through Each Shot in Major Frame #
        for shot in range(200):

            # T0 Rising #
            events["time"].append(t0time)
            events["signal"].append("t0")
            events["edge"].append(True)

            # T0 Falling #
            events["time"].append(t0time + 1)
            events["signal"].append("t0")
            events["edge"].append(False)

            # SAL Rising #
            events["time"].append(t0time + row["SAL.rws"])
            events["signal"].append("sal")
            events["edge"].append(True)

            # SAL Falling #
            events["time"].append(t0time + row["SAL.rws"] + row["SAL.rww"])
            events["signal"].append("sal")
            events["edge"].append(False)

            # WAL Rising #
            events["time"].append(t0time + row["WAL.rws"])
            events["signal"].append("wal")
            events["edge"].append(True)

            # WAL Falling #
            events["time"].append(t0time + row["WAL.rws"] + row["WAL.rww"])
            events["signal"].append("wal")
            events["edge"].append(False)

            # SAM Rising #
            events["time"].append(t0time + row["SAM.rws"])
            events["signal"].append("sam")
            events["edge"].append(True)

            # SAM Falling #
            events["time"].append(t0time + row["SAM.rws"] + row["SAM.rww"])
            events["signal"].append("sam")
            events["edge"].append(False)

            # WAM Rising #
            events["time"].append(t0time + row["WAM.rws"])
            events["signal"].append("wam")
            events["edge"].append(True)

            # WAM Falling #
            events["time"].append(t0time + row["WAM.rws"] + row["WAM.rww"])
            events["signal"].append("wam")
            events["edge"].append(False)

            # Tx Rising #
            events["time"].append(t0time + row["TxCoarse"])
            events["signal"].append("tx")
            events["edge"].append(True)

            # Tx Falling #
            events["time"].append(t0time + row["TxCoarse"] + 1)
            events["signal"].append("tx")
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
