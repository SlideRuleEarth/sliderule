#
#   Purpose: process trace outputs from lttng to produce human readable results
#
#   Example creation of Software Timing Analyzer files:
#       /usr/bin/python3 trace.py --fmt sta --ids 22 23 24 25 /tmp/sliderule-session
#
#   Example simple console output:
#       /usr/bin/python3 trace.py /tmp/sliderule-session
#

import bt2
import sys
import datetime
import pandas

###############################################################################
# GLOBALS
###############################################################################

TRACE_ORIGIN = 0
COLOR_MAP = [8421631, 8454143, 8454016, 16777088, 16744703, 16777215]

###############################################################################
# FUNCTIONS
###############################################################################

def display_trace(trace, depth):
    # Correct missing stops
    if trace["stop"] == None:
        trace["stop"] = trace["start"]
    # Get values of trace
    trace_id        = trace["start"].event.payload_field['id']
    thread_id       = trace["start"].event.payload_field['tid']
    start_time      = trace["start"].default_clock_snapshot.ns_from_origin
    stop_time       = trace["stop"].default_clock_snapshot.ns_from_origin
    sec_from_origin = start_time / 1e9
    sec_duration    = (stop_time - start_time) / 1e9
    dt              = datetime.datetime.fromtimestamp(sec_from_origin)
    name            = trace["start"].event.payload_field['name']
    attributes      = trace["start"].event.payload_field['attributes']
    # Print trace
    print('{} ({:12.6f} sec):{:{indent}}{:{width}} <{}> {} [{}]'.format(dt, sec_duration, "", str(name), thread_id, attributes, trace_id, indent=depth, width=30-depth))
    # Recurse on children
    for child in trace["children"]:
        display_trace(child, depth + 2)

def write_sta_events(filename, df):
    f = open(filename, "w")
    for index,row in df.iterrows():
        f.write("%08d %08X %.3f us\n" % (index, int(row["id"] + (int(row["edge"]) << 14)), row["delta"]))
    f.close()

def write_sta_setup(filename, perf_ids):
    f = open(filename, "w")
    f.write("[PerfID Table]\n")
    f.write("Version=1\n")
    f.write("Auto Hide=True\n")
    f.write("Auto Set Bit For Exit=True\n")
    f.write("Bit To Set For Exit=14\n")
    f.write("Number of PerfIDs=%d\n" % (len(perf_ids)))
    index = 0
    for name in perf_ids:
        perf_id = int(perf_ids[name]["id"])
        depth = int(perf_ids[name]["depth"])
        if(depth > len(COLOR_MAP)):
            depth = len(COLOR_MAP)
        f.write("[PerfID %d]\n" % (index))
        f.write("Name=%s\n" % (name))
        f.write("Entry ID=%08X\n" % perf_id)
        f.write("Exit ID=%08X\n" % (int(perf_id + (int(1) << 14))))
        f.write("Calc CPU=True\n")
        f.write("Color=%d\n" % (COLOR_MAP[depth - 1]))
        f.write("Hide=False\n")
        f.write("DuplicateEdgeWarningsDisabled=False\n")
        index += 1
    f.close()

def build_event_list(trace, depth, max_depth, names, events, perf_ids):
    # Get Perf ID
    name = trace["name"]
    perf_id = names.index(name)
    perf_ids[name] = {"id": perf_id, "depth": depth}
    # Append Events
    try:
        events.append({"id": perf_id, "time": trace["start"].default_clock_snapshot.ns_from_origin / 1000.0, "edge": 0})
        events.append({"id": perf_id, "time": trace["stop"].default_clock_snapshot.ns_from_origin / 1000.0, "edge": 1})
    except:
        pass
    # Recurse on Children
    if (depth < max_depth) or (max_depth == 0):
        for child in trace["children"]:
            build_event_list(child, depth + 1, max_depth, names, events, perf_ids)

def console_output(origins):
    # Output traces to console
    for trace in origins:
        display_trace(trace, 1)

def sta_output(idlist, depth, names, traces):
    # Build list of events and names
    events = []
    perf_ids = {}
    for trace_id in idlist:
        build_event_list(traces[trace_id], 1, depth, names, events, perf_ids)
    # Build and sort data frame
    df = pandas.DataFrame(events)
    df = df.sort_values("time")
    # Build delta times
    df["delta"] = df["time"].diff()
    df.at[0, "delta"] = 0.0
    # Write out data frame as sta events
    write_sta_events("pytrace.txt", df)
    write_sta_setup("pytrace.PerfIDSetup", perf_ids)

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Initial Configuration
    outfmt = "console"
    depth = 1
    idlist = []

    # Command Line Parameters
    num_parms = len(sys.argv) - 1
    path = sys.argv[num_parms] # path to system trace
    parm = 0
    while parm < num_parms:
        if sys.argv[parm] == "--fmt":
            parm += 1
            outfmt = sys.argv[parm]
        elif sys.argv[parm] == "--depth":
            parm += 1
            depth = int(sys.argv[parm])
        elif sys.argv[parm] == "--ids":
            while (parm + 1) < num_parms:
                if sys.argv[parm + 1].isnumeric():
                    parm += 1
                    idlist.append(int(sys.argv[parm]))
                else:
                    break
        parm += 1

    # Create a trace collection message iterator with this path.
    msg_it = bt2.TraceCollectionMessageIterator(path)

    # Dictionary of traces indexed by trace id, and list of origin traces
    names = {} # dictionary of unique trace names
    traces = {} # dictionary of unique traces
    origins = [] # list of highest level "root" traces

    # Iterate over each trace point to create traces
    for msg in msg_it:

        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:

            # Populate traces dictionary
            try:
                trace_id = msg.event.payload_field['id']
                if msg.event.name == "sliderule:start":
                    if trace_id not in traces.keys():                        
                        # Populate start of span
                        name = str(msg.event.payload_field['name']) + "." + str(msg.event.payload_field['tid'])
                        traces[trace_id] = {"id": trace_id, "name": name, "start": msg, "stop": None, "children": []}
                        # Link to parent
                        parent_trace_id = msg.event.payload_field['parent']
                        if parent_trace_id in traces.keys():
                            traces[parent_trace_id]["children"].append(traces[trace_id])
                        else:
                            origins.append(traces[trace_id])
                        # Populate name
                        names[name] = True
                    else:
                        raise Exception("double start")
                elif msg.event.name == "sliderule:stop":
                    if trace_id in traces.keys():
                        # Populate stop of span
                        traces[trace_id]["stop"] = msg
                    else:
                        raise Exception("stop without start")
            except Exception as inst:
                print(inst)

    # Flatten names to get indexes
    names = list(names)

    # Run commanded operation
    if outfmt == "console":
        console_output(origins)
    elif outfmt == "sta":
        sta_output(idlist, depth, names, traces)
