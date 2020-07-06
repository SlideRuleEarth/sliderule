## Installation and Setup

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

## Start and Stop a Trace

To start a trace, run the sliderule application, and then kick off the `create_trace.sh` script.

To stop a trace, run the `kill_trace.sh` script.  This can be run at anytime, before or after the application is stopped.

## Analyze a Trace

To analyze a captured trace, run the targets/sliderule-lttng/trace.py python script and pass it the path to the trace folder (default: /tmp/sliderule-session): `/usr/bin/python3 trace.py /tmp/sliderule-session`.  

Note 1: The system installation of python is used since the steps above install babeltrace2 into the system python.  If a separate python distribution is being used (e.g. Anaconda) then the installation steps above must be modified to install the babeltrace2 python bindings into that distribution's environment.

Note 2: The trace must be killed before it can be analyzed.
