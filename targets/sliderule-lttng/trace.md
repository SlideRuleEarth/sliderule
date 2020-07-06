## Installation and Setup

* LTTng
    - See [core.md](../../packages/core/core.md) for how to install and configure SlideRule for lttng tracing.

* Trace-Compass
    - `sudo apt install default-jre`
    - Go to `https://www.eclipse.org/tracecompass/` to download the latest release
    - `tar -xvzf trace-compase-{version info}.tar.gz`
    - `cd trace-compass; ./tracecompass`


## Start and Stop Trace

To start a trace, run the sliderule application, and then kick off the `create_trace.sh` script.

To stop a trace, run the `kill_trace.sh` script.  This can be run at anytime, before or after the application is stopped.
