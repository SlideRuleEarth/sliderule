--
-- ENDPOINT:    /source/time
--
-- INPUT:       arg[1]
--              {
--                  "time":     <time value>
--                  "input":    "<format of time value>"
--                  "output":   "<format of output>"
--              }
--
-- OUTPUT:      {
--                  "time":     <time value>
--                  "format":   "<format of time value>"
--              }
--
-- NOTES:       1. Both the input and the output are json objects
--              2. The format options are as follows
--                  NOW -   if supplied for either input or time then grab the current time
--                  CDS -   CCSDS 6-byte packet timestamp represented as [<day>, <ms>]
--                          days = 2 bytes of days since GPS epoch
--                          ms = 4 bytes of milliseconds in the current day
--                  GMT -   UTC time represented as a one of two date strings
--                          "<year>:<month>:<day of month>:<hour in day>:<minute in hour>:<second in minute>""
--                          "<year>:<day of year>:<hour in day>:<minute in hour>:<second in minute>"
--                  GPS -   seconds since GPS epoch "January 6, 1980"
--              3. The GMT output is always in Day of Year format
--

local json = require("json")
local parm = json.decode(arg[1])

local time_value = parm["time"]
local input_format = parm["input"]
local output_format = parm["output"]

local result_time = time_value
local result_format = "\""..output_format.."\"" 

if input_format == "NOW" or time_value == "NOW" then
    if output_format == "GPS" then
        result_time = time.gps()
    elseif output_format == "GMT" then
        local year, day, hour, minute, second, millisecond = time.gmt()
        result_time = string.format("\"%d:%d:%d:%d:%d\"", year, day, hour, minute, second)
    end

elseif input_format == "GPS" and output_format == "GMT" then
    local year, day, hour, minute, second, millisecond = time.gps2gmt(tonumber(time_value))
    result_time = string.format("\"%d:%d:%d:%d:%d\"", year, day, hour, minute, second)

elseif input_format == "GMT" and output_format == "GPS" then
    result_time = time.gmt2gps(time_value)

elseif input_format == "CDS" and output_format == "GMT" then
    local year, day, hour, minute, second, millisecond = time.cds2gmt(time_value[1], time_value[2])
    result_time = string.format("\"%d:%d:%d:%d:%d\"", year, day, hour, minute, second)

elseif input_format == "CDS" and output_format == "GMT" then
    local year, day, hour, minute, second, millisecond = time.cds2gmt(time_value[1], time_value[2])
    local gmt_time = string.format("\"%d:%d:%d:%d:%d\"", year, day, hour, minute, second)
    result_time = time.gmt2fps(gmt_time)

end

return string.format('{\"time\": %s, \"format\": %s}', result_time, result_format)
