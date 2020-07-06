import bt2
import sys
import datetime

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Get the trace path from the first command-line argument.
    path = sys.argv[1]

    # Create a trace collection message iterator with this path.
    msg_it = bt2.TraceCollectionMessageIterator(path)

    # Dictionary of traces indexed by id #
    traces = {}

    # Iterate the trace messages.
    for msg in msg_it:

        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:

            try:
                # Populate trace dictionary
                trace_id = msg.event.payload_field['id']
                if msg.event.name == "sliderule:start":
                    if trace_id not in traces.keys():                        
                        traces[trace_id] = [msg, None]
                    else:
                        raise Exception("double start")
                elif msg.event.name == "sliderule:stop":
                    if trace_id in traces.keys():
                        traces[trace_id][1] = msg
                    else:
                        raise Exception("stop without start")
            except Exception as inst:
                print(inst)

    # Print traces
    for t in traces.keys():
        trace = traces[t]

        # Get time, duration, and date of trace
        sec_from_origin = trace[0].default_clock_snapshot.ns_from_origin / 1e9
        sec_duration = (trace[1].default_clock_snapshot.ns_from_origin - trace[0].default_clock_snapshot.ns_from_origin) / 1e9
        dt = datetime.datetime.fromtimestamp(sec_from_origin)

        # Print trace
        print('{} ({:.6f} s): {} {}'.format(dt, sec_duration, trace[0].event.payload_field['name'], trace[0].event.payload_field['attributes']))

