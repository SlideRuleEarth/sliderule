## I. Installation and Setup

* LTTng
    - See [core.md](../../packages/core/core.md) for how to install and configure SlideRule for lttng tracing.

* Trace-Compass
    - Go to `https://www.eclipse.org/tracecompass/` to download the latest release
    - `sudo apt install default-jre`
    - `tar -xvzf trace-compase-{version info}.tar.gz`
    - `cd trace-compass; ./tracecompass`

* Babeltrace2
    - Go to `https://babeltrace.org/` for the latest version and instructions on getting babletrace2.
    - `sudo apt install python3-bt2` to install the python bindings

## II. Start and Stop a Trace

1. To run a fully instrumented version of sliderule, run the `instrument_and_run.sh` script and pass it the same parameters you would `sliderule`.  This script preloads wrappers for memory allocation and pthread mutex functions so that they can be correlated with other userspace traces.  If you don't want the wrappers, and just want the sliderule provided traces, then just run `sliderule` as you normally would.

2. To start a trace, run the `sudo create_trace.sh [kernel]` script.  The "kernel" option will enable kernel level traces and is usefull when analyzing the trace with tracecompass.

3. To stop a trace, run the `kill_trace.sh` script.  This can be run at anytime, before or after the application is stopped.  Note that this script is not run as sudo.  Sudo is supplied inside the script so that the whoami command picks up the correct user.


## III. Analyze a Trace with Babeltrace2

To analyze a captured trace, run the targets/sliderule-lttng/trace.py python script and pass it the path to the trace folder (default: /tmp/sliderule-session): `/usr/bin/python3 trace.py /tmp/sliderule-session`.  

__Note 1__: The system installation of python is used since the steps above install babeltrace2 into the system python.  If a separate python distribution is being used (e.g. Anaconda) then the installation steps above must be modified to install the babeltrace2 python bindings into that distribution's environment.

__Note 2__: The trace must be killed before it can be analyzed.

After running the `trace.py` as indicated above, subsequent runs can supply `sta <trace_id1> <trace_id2> ... <trace_idn>` where the trace ids listed are the ones interested in.  Running this command will produce a `pytrace.txt` trace file and a `pytrace.PerfIDSetup` file which can be injesting into the __Software Timing Analyzer__ tool.

## IV. Analyze a Trace with Trace-Compass

Trace-Compass is an eclipse plugin that can be used to graphically view the results of the trace.  To use Trace-Compass:

1. Run `tracecompass` from the terminal.
2. Create a new tracing project (the first time only). Select "New Project" --> "Tracing" --> "Tracing Project".
3. Expand the project to expose the Tracing component; right click the Tracing component and select "Import".
4. Set the source root directory to "/tmp/sliderule-session", and select "sliderule-session" in the selection box.
5. Click "Finish" to import the session.

Once imported, navigate the sliderule project content by clicking through the following tree: "Traces" --> "ust" --> "uid" --> "1000" --> "64-bit".

Usefull views include the "UST Memory Usage" view correlated against a filtered "sliderule:*" event type.


