ATLAS (SlideRule Plugin)
========================

SigView is a signal processing plugin for SlideRule to analyze the ATLAS PCE science and housekeeping data.


Running SigView
---------------

$ `sliderule ipy/sigview.lua <json file>`


JSON Input File Format
-----------------------

````
--    {
--         "gui":       true | false,
--         "threads":   <number_threads>,
--         "block":     true | false,
--         "timeout":   <timeout_in_seconds>
--         "database":  <glob path to rec files, default: ../../itos/rec/atlas/fsw/*.rec>
--
--         "source":    {
--                          "type": "DATASRV",
--                          "location": "REMOTE" | "LOCAL",
--                          "arch": "ITOS_TLM_ARCH" | "SPW_SSR_ARCH",
--                          "start": "<start_time>",
--                          "stop": "<stop_time>",
--                      }, |
--                      {
--                          "type": "FILE",
--                          "files": "<file paths to data input>"
--                          "format": "CCSDS" | "SPW" | "ITOS" | "MOC" | "ADAS" | "SIS" | "SSR" | "CDH"
--                      },
--         "metric":    {
--                          "key":       "RECEIPT_KEY" | "FIELD_KEY <field name>" | "CALCULATED_KEY <function>"
--                          "base":      {
--                                          "<record 1>": ["<field 1>", "<field 2>", "<field 3>", ... ],
--                                          "<record 2>": ["<field 1>", "<field 2>", "<field 3>", ... ],
--                                          .
--                                          .
--                                          "<record n>": ["<field 1>", "<field 2>", "<field 3>", ... ]
--                                       },
--                          "ccsds":     {
--                                          "<record 1>": ["<field 1>", "<field 2>", "<field 3>", ... ],
--                                          "<record 2>": ["<field 1>", "<field 2>", "<field 3>", ... ],
--                                          .
--                                          .
--                                          "<record n>": ["<field 1>", "<field 2>", "<field 3>", ... ]
--                                      }
--                      }
--         "limit":     {
--                          "key":       "RECEIPT_KEY" | "FIELD_KEY <field name>" | "CALCULATED_KEY <function>"
--                          "base":     [
--                                          {
--                                              "record": <record name>,
--                                              "field": <field name>,
--                                              "id": <id field that filters on only these ids>,
--                                              "min": <minimum allowed value>,
--                                              "max": <maximum allowed value>
--                                          },
--                                          .
--                                          .
--                                      ],
--                          "ccsds":    [
--                                          {
--                                              "record": <record name>,
--                                              "field": <field name>,
--                                              "id": <id field that filters on only these ids>,
--                                              "min": <minimum allowed value>,
--                                              "max": <maximum allowed value>
--                                          },
--                                          .
--                                          .
--                                      ]
--                      }
--    }
--
````

1. Command line parameters can be substituted for any input field in the json file that is
   a string.  To specified which command line parameter should be used, specify ${argnum}.
   For example, if you want the "files" and "format" fields to be filled in by command line
   parameters, then the json file would include
````
  "source":    {
                   "type": "FILE",
                   "files": "$1"
                   "format": "$2"
               },
````
Then on the command line you would type
````
   sliderule sigview.lua {jsonfile} {data file} {format}
````
An example could be
````
   slierule sigview.lua ametlatch.json /mnt/THUMB/atlas_data.bin CCSDS
````

2. The location parameter specifies which IP address to connect to datasrv on
   * REMOTE - use outside the lab when connecting on cne
   * LOCAL - use inside the lab when connecting on simnet or firenet

3. {start time} and {stop time} format: YYYYMMDDHHMMSS
   Data server times are recorded and provided in local wall-clock time.
   The flight software records all times as GPS times and converts them for
   displaying to GMT time; which during daylight savings (3/11 to 11/4) is
   4 hours later than local east-coast time (e.g. 1:00 p.m. local time is
   5:00 p.m. GMT) and outside of daylight savings is 5 hours later.

4. The gui parameter specifies whether or not to start the gtk viewer and shell component of sigview.  The sigview
   application always initializes the gui component which means that a display always needs to be available
   to the application (e.g. you cannot start sigview via a straight ssh connection even with the gui
   parameter set to false, you must use ssh -X).

5. The {number_threads} parameter specifies how many threads to allocate to process the packets.  The number
   of threads should not exceed the number of cores.  Setting the number of threads to 1 makes the processing
   deterministic, though in many cases sigview will order the data it reports using a sliding window, so using
   additional threads is often fine even when the results need to be ordered.

6. The key is used to create the index for each record.  A typical index is the creation time
   provided in the secondary header of the CCSDS telemetry packet.  The following options are availble
   for creating the index:
   * RECEIPT_KEY - increments by one for every record in the dispatched stream
   * INCREMENTING_KEY - increments by one for every metric created (no gaps in index value)
   * FIELD_KEY - uses the value of the provided field as the index (e.g. major frame count)
   * CALCULATED_KEY - uses a supplied callback function to calculate the key based on the record (e.g. secondary header timestamp)

7. The block parameter specifies whether or not the program should attempt to automatically wait for the
   playback to complete and exit.  This is useful when running sigview from a script or other automated process.

8. The {timeout_in_seconds} is used when blocking is set to specify how many seconds to wait for data from the datasrv
   before exiting sigview. In low bandwidth connectionslike the VPN, it may be necessary to increase the timeout to
   30 seconds.  In high bandwidth connections like inside the lab, the timeout can be left to the default value
   (by omitting it from the json).


