# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
#  Uses the "event" endpoint to capture a set of traces and produce human 
#  readable results; also produces an output compatible with the GSFC
#  Software Timing Analyzer developed by Steve Slegel.
#

###############################################################################
# IMPORTS
###############################################################################

import argparse
import pandas
import sliderule

###############################################################################
# COMMAND LINE ARGUMENTS
###############################################################################

parser = argparse.ArgumentParser(description="""monitor""")
parser.add_argument('--domain',             type=str,               default="localhost")
parser.add_argument('--organization',       type=str,               default=None)
parser.add_argument('--verbose',            action='store_true',    default=False)
parser.add_argument('--depth',              type=int,               default=0)
parser.add_argument('--filter',             type=str, nargs='+',    default=[])
args,_ = parser.parse_known_args()

###############################################################################
# GLOBALS
###############################################################################

TRACE_ORIGIN = 0
TRACE_START = 1
TRACE_STOP = 2
LOG = 1
TRACE = 2
TELEMETRY = 4
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
    sec_from_origin = start_time / 1e6
    sec_duration    = (stop_time - start_time) / 1e6
    dt              = sec_from_origin
    name            = trace["start"]['name']
    attributes      = trace["start"]['attr']
    # Print trace
    print('{:.3f} ({:7.3f} sec):{:{indent}}{:{width}} <{}> {} [{}]'.format(dt, sec_duration, "", str(name), thread_id, attributes, trace_id, indent=depth, width=30-depth))
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

def build_event_list(trace, depth, max_depth, names, events, perf_ids, filter_list):
    name = trace["name"]
    # Filter
    if name.split(".")[0] in filter_list:
        return
    # Get Perf ID
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
            build_event_list(child, depth + 1, max_depth, names, events, perf_ids, filter_list)

def console_output(origins):
    # Output traces to console
    for trace in origins:
        display_trace(trace, 1)

def sta_output(filter_list, depth, names, traces):
    global origins
    # Build list of events and names
    events = []
    perf_ids = {}
    for trace in origins:
        build_event_list(trace, 1, depth, names, events, perf_ids, filter_list)
    # Build and sort data frame
    df = pandas.DataFrame(events)
    df = df.sort_values("time")
    # Build delta times
    df["delta"] = df["time"].diff()
    df.at[0, "delta"] = 0.0
    # Write out data frame as sta events
    write_sta_events("pytrace.txt", df)
    write_sta_setup("pytrace.PerfIDSetup", perf_ids)

def process_event(rec, session):
    global names, traces, origins
    # Populate traces dictionary
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

    # Initialize Client
    sliderule.init(args.domain, organization=args.organization, verbose=args.verbose)

    # Default Request
    rqst = {
        "type": LOG | TRACE,
        "duration": 30
    }

    # Connect to SlideRule
    rsps = sliderule.source("event", rqst, stream=True, callbacks={'tracerec': process_event})

    # Flatten Names to Get Indexes
    names = list(names)

    # Output Traces
    console_output(origins)
    sta_output(args.filter, args.depth, names, traces)
