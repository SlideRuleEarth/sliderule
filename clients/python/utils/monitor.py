#
#   Uses the "event" endpoint to capture a set of traces
#   and produce human readable results
#

import sys
import pandas
import sliderule
from utils import parse_command_line

###############################################################################
# GLOBALS
###############################################################################

TRACE_ORIGIN = 0
TRACE_START = 1
TRACE_STOP = 2
LOG = 1
TRACE = 2
METRIC = 4
COLOR_MAP = [8421631, 8454143, 8454016, 16777088, 16744703, 16777215]

names = {} # dictionary of unique trace names
traces = {} # dictionary of unique traces
origins = [] # list of highest level "root" traces

###############################################################################
# FUNCTIONS
###############################################################################

def display_trace(trace, depth):
    # Correct missing stops
    if trace["stop"] == None:
        trace["stop"] = trace["start"]
    # Get values of trace
    trace_id        = trace["start"]['id']
    thread_id       = trace["start"]['tid']
    start_time      = trace["start"]['time']
    stop_time       = trace["stop"]['time']
    sec_from_origin = start_time / 1e3
    sec_duration    = (stop_time - start_time) / 1e3
    dt              = sliderule.gps2utc(sec_from_origin)
    name            = trace["start"]['name']
    attributes      = trace["start"]['attr']
    # Print trace
    print('{} ({:7.3f} sec):{:{indent}}{:{width}} <{}> {} [{}]'.format(dt, sec_duration, "", str(name), thread_id, attributes, trace_id, indent=depth, width=30-depth))
    # Recurse on children
    for child in trace["children"]:
        display_trace(child, depth + 2)

def write_sta_events(filename, df):
    f = open(filename, "w")
    for index,row in df.iterrows():
        f.write("%08d %08X %.3f ms\n" % (index, int(row["id"] + (int(row["edge"]) << 14)), row["delta"]))
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
        events.append({"id": perf_id, "time": trace["start"]["time"], "edge": 0})
        events.append({"id": perf_id, "time": trace["stop"]["time"], "edge": 1})
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
    global origins
    # Build list of events and names
    events = []
    perf_ids = {}
    if len(idlist) > 0:
        for trace_id in idlist:
            build_event_list(traces[trace_id], 1, depth, names, events, perf_ids)
    else:
        for trace in origins:
            build_event_list(trace, 1, depth, names, events, perf_ids)
    # Build and sort data frame
    df = pandas.DataFrame(events)
    df = df.sort_values("time")
    # Build delta times
    df["delta"] = df["time"].diff()
    df.at[0, "delta"] = 0.0
    # Write out data frame as sta events
    write_sta_events("pytrace.txt", df)
    write_sta_setup("pytrace.PerfIDSetup", perf_ids)

def process_event(rec):
    global names, traces, origins
    # Populate traces dictionary
    if rec["type"] == LOG:
        print('%s:%s:%s' % (rec["ipv4"], rec["name"], rec["attr"]))
    elif rec["type"] == TRACE:
        trace_id = rec['id']
        if rec["flags"] & TRACE_START:
            if trace_id not in traces.keys():
                # Populate start of span
                name = str(rec['name']) + "." + str(rec['tid'])
                traces[trace_id] = {"id": trace_id, "name": name, "start": rec, "stop": None, "children": []}
                # Link to parent
                parent_trace_id = rec['parent']
                if parent_trace_id in traces.keys():
                    traces[parent_trace_id]["children"].append(traces[trace_id])
                else:
                    origins.append(traces[trace_id])
                # Populate name
                names[name] = True
            else:
                print('warning: double start for %s' % (rec['name']))
        elif rec["flags"] & TRACE_STOP:
            if trace_id in traces.keys():
                # Populate stop of span
                traces[trace_id]["stop"] = rec
            else:
                print('warning: stop without start for %s' % (rec['name']))

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Default Parameters
    parms = {
        "url": "localhost",
        "organization": None,
        "fmt": "console",
        "depth": 0,
        "ids": []
    }

    # Override Parameters
    parms = parse_command_line(sys.argv, parms)

    # Default Request
    rqst = {
        "type": LOG | TRACE,
        "level" : "INFO",
        "duration": 30
    }

    # Override Request
    rqst = parse_command_line(sys.argv, rqst)

    # Set URL and Organization
    sliderule.set_url(parms["url"])
    sliderule.authenticate(parms["organization"])

    # Connect to SlideRule
    rsps = sliderule.source("event", rqst, stream=True, callbacks={'eventrec': process_event})

    # Flatten names to get indexes
    names = list(names)

    # Run commanded operation
    if parms["fmt"] == "console":
        console_output(origins)
    elif parms["fmt"] == "sta":
        sta_output(parms["ids"], parms["depth"], names, traces)
