import bt2
import sys
import datetime
import pandas

###############################################################################
# GLOBALS
###############################################################################

TRACE_ORIGIN = 0

###############################################################################
# FUNCTIONS
###############################################################################

def display_trace(trace, depth):
    # Correct missing stops
    if trace["stop"] == None:
        trace["stop"] = trace["start"]
    # Get values of trace
    trace_id        = trace["start"].event.payload_field['id']
    start_time      = trace["start"].default_clock_snapshot.ns_from_origin
    stop_time       = trace["stop"].default_clock_snapshot.ns_from_origin
    sec_from_origin = start_time / 1e9
    sec_duration    = (stop_time - start_time) / 1e9
    dt              = datetime.datetime.fromtimestamp(sec_from_origin)
    name            = trace["start"].event.payload_field['name']
    attributes      = trace["start"].event.payload_field['attributes']
    # Print trace
    print('{} ({:12.6f} sec):{:{indent}}{:{width}} {} [{}]'.format(dt, sec_duration, "", str(name), attributes, trace_id, indent=depth, width=30-depth))
    # Recurse on children
    for child in trace["children"]:
        display_trace(child, depth + 2)

def write_sta_events(filename, df):
    f = open(filename, "w")
    for index,row in df.iterrows():
        f.write("%08d %08X %.3f us\n" % (index, int(row["id"] + (int(row["edge"]) << 14)), row["delta"]))
    f.close()

def write_sta_setup(filename, perf_names):
    f = open(filename, "w")
    f.write("[PerfID Table]\n")
    f.write("Version=1\n")
    f.write("Auto Hide=True\n")
    f.write("Auto Set Bit For Exit=True\n")
    f.write("Bit To Set For Exit=14\n")
    f.write("Number of PerfIDs=%d\n" % (len(perf_names)))
    for name in perf_names:
        perfid = int(perf_names[name])
        f.write("[PerfID %d]\n" % (perfid))
        f.write("Name=%s\n" % (name))
        f.write("Entry ID=%08X\n" % perfid)
        f.write("Exit ID=%08X\n" % (int(perfid + (int(1) << 14))))
        f.write("Calc CPU=True\n")
        f.write("Color=65280\n")
        f.write("Hide=False\n")
        f.write("DuplicateEdgeWarningsDisabled=False\n")
    f.close()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Get the trace path from the first command-line argument.
    path = sys.argv[1]

    # Get output format option.
    outfmt = "console"
    idlist = []
    if len(sys.argv) > 2:
        outfmt = sys.argv[2]
        for i in range(len(sys.argv) - 3):
            idlist.append(int(sys.argv[i + 3]))

    # Create a trace collection message iterator with this path.
    msg_it = bt2.TraceCollectionMessageIterator(path)

    # Dictionary of traces indexed by trace id, and list of origin traces
    initial_time = 0.0 # time of first trace (used as an offset)
    names = {} # dictionary of unique trace names
    traces = {} # dictionary of unique traces
    origins = [] # list of highest level "root" traces

    # Iterate over each trace point to create traces
    for msg in msg_it:

        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:

            # Populate traces dictionary
            try:
                if initial_time == 0:
                    initial_time = msg.default_clock_snapshot.ns_from_origin
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

    # Select which operation to run
    if outfmt == "console":
        # Output traces to console
        for trace in origins:
            display_trace(trace, 1)
    elif outfmt == "sta":
        # Flatten names to get indexes
        names_list = list(names)
        perf_names = {}
        # Build list of events and names
        events = []
        for trace_id in idlist:
            for child in traces[trace_id]["children"]:
                # Start trace
                trace = child["start"]
                name = child["name"]
                perf_names[name] = names_list.index(name)
                delta_time = (trace.default_clock_snapshot.ns_from_origin - initial_time) / 1e3 # convert to microseconds
                events.append({"id": perf_names[name], "delta": delta_time, "edge": 0})
                # Stop trace
                trace = child["stop"]
                delta_time = (trace.default_clock_snapshot.ns_from_origin - initial_time) / 1e3 # convert to microseconds
                events.append({"id": perf_names[name], "delta": delta_time, "edge": 1})
        # Build and sort data frame
        df = pandas.DataFrame(events)
        df.sort_values("delta")
        # Write out data frame as sta events
        write_sta_events("pytrace.txt", df)
        write_sta_setup("pytrace.PerfIDSetup", perf_names)
