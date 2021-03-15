--
-- ENDPOINT:    /engine/event
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

local json = require("json")
local parm = json.decode(arg[1])

local type = parm["type"] or core.LOG
local level = parm["level"] or nil
local format = parm["format"] or core.FMT_TEXT
local duration = parm["duration"] or 0

-- Attach monitor to response queue --
local userevents = core.monitor(type, level, format, rspq)

-- Pend for duration (in 1 second intervals to allow hooks to execute) --
local seconds = 0
while duration == 0 or seconds < duration do
    seconds = seconds + 1
    sys.wait(1)
end
