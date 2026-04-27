-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- parse request
    local time_value = parms["time"]
    local input_format = parms["input"]
    local output_format = parms["output"]
    local result_time = time_value
    local result_format = "\""..output_format.."\""

    -- get time input
    local input_time = time_value
    if input_format == "NOW" then
        input_time = time.gps()
    elseif input_format == "GMT" then
        input_time = time.gmt2gps(time_value)
    elseif input_format == "DATE" then
        input_time = time.gmt2gps(time_value)
    elseif input_format == "CDS" then
        local year, doy, hour, minute, second, _ = time.cds2gmt(time_value[1], time_value[2])
        local gmt_time = string.format("\"%d:%d:%d:%d:%d\"", year, doy, hour, minute, second)
        input_time = time.gmt2gps(gmt_time)
    end

    -- set output time
    if output_format == "GPS" then
        result_time = input_time
    elseif output_format == "GMT" then
        local year, doy, hour, minute, second, _ = time.gps2gmt(input_time)
        result_time = string.format("\"%d:%d:%d:%d:%d\"", year, doy, hour, minute, second)
    elseif output_format == "DATE" then
        local year, month, day, hour, minute, second, _ = time.gps2date(input_time)
        result_time = string.format("\"%04d-%02d-%02dT%02d:%02d:%02dZ\"", year, month, day, hour, minute, second)
    end

    -- return response
    return string.format('{\"time\": %s, \"format\": %s}', result_time, result_format)

end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Time",
    description = "Returns current and converted times in different formats support and used by SlideRule",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}

-- INPUT
--  {
--      "time":     <time value>
--      "input":    "<format of time value>"
--      "output":   "<format of output>"
--  }
--
-- OUTPUT
--  {
--      "time":     <time value>
--      "format":   "<format of time value>"
--  }
--
-- NOTES:
--  1. Both the input and the output are json objects
--  2. The format options are as follows
--      NOW -   if supplied for either input or time then grab the current time
--      CDS -   CCSDS 6-byte packet timestamp represented as [<day>, <ms>]
--              days = 2 bytes of days since GPS epoch
--              ms = 4 bytes of milliseconds in the current day
--      GMT -   UTC time represented as
--              "<year>:<day of year>:<hour in day>:<minute in hour>:<second in minute>"
--      GPS -   milliseconds since GPS epoch "January 6, 1980"
--      DATE-   UTC time represented as
--              "<year>-<month>-<day of month>T<hour in day>:<minute in hour>:<second in minute>Z""
--  3. The GMT output is always in Day of Year format
