import bt2
import sys
import datetime

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
    start_time      = trace["start"].default_clock_snapshot.ns_from_origin
    stop_time       = trace["stop"].default_clock_snapshot.ns_from_origin
    sec_from_origin = start_time / 1e9
    sec_duration    = (stop_time - start_time) / 1e9
    dt              = datetime.datetime.fromtimestamp(sec_from_origin)
    name            = trace["start"].event.payload_field['name']
    attributes      = trace["start"].event.payload_field['attributes']
    # Print trace
    print('{} ({:12.6f} sec):{:{indent}}{:{width}} {}'.format(dt, sec_duration, "", str(name), attributes, indent=depth, width=30-depth))
    # Recurse on children
    for child in trace["children"]:
        display_trace(child, depth + 2)

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Get the trace path from the first command-line argument.
    path = sys.argv[1]

    # Create a trace collection message iterator with this path.
    msg_it = bt2.TraceCollectionMessageIterator(path)

    # Dictionary of traces indexed by trace id, and list of origin traces
    traces = {}
    origins = []

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
                        traces[trace_id] = {"start": msg, "stop": None, "children": []}
                        # Link to parent
                        parent_trace_id = msg.event.payload_field['parent']
                        if parent_trace_id != TRACE_ORIGIN:
                            if parent_trace_id in traces.keys():
                                traces[parent_trace_id]["children"].append(traces[trace_id])
                            else:
                                raise Exception("parent not found")
                        else:
                            origins.append(traces[trace_id])
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

    # Print traces
    for trace in origins:
        display_trace(trace, 1)


