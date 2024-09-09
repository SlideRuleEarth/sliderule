--
-- ENDPOINT:    /source/event
--
-- INPUT:       arg[1] -
--              {
--                  "type":     <core.LOG | core.TRACE | core.METRIC>
--                  "level":    "<event level string>" -OR- <event level number>
--                  "format":   <core.FMT_TEXT | core.FMT_JSON>
--                  "duration": <seconds to hold connection open | 0 for indefinite>
--              }
--
--              rspq - output queue to stream log messages
--
-- OUTPUT:      application event messages
--

local global = require("global")
local json = require("json")
local parm = json.decode(arg[1])

local type = global.eval(parm["type"]) or core.LOG
local level = global.eval(parm["level"]) or core.INFO
local format = global.eval(parm["format"]) or nil
local duration = parm["duration"] or 0

-- Attach monitor to post event to response queue --
local userevents = core.pmonitor(type, level, format, core.EVENTQ, rspq)

-- Bounds check duration
if duration > 600 then
    duration = 600
end

-- Watch response queue
local watchq = msg.publish(rspq)

-- Pend for duration (in 1 second intervals to allow hooks to execute) --
local seconds = 0
while (watchq:numsubs() > 0) and (seconds < duration) do
    seconds = seconds + 1
    sys.wait(1)
end
