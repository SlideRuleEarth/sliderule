--
-- ENDPOINT:    /engine/log
--
-- INPUT:       arg[1] -
--              {
--                  "level":    "<event level string>" -OR- <event level number>
--                  "duration": <seconds to hold connection open | 0 for indefinite>
--              }
--
--              rspq - output queue to stream log messages
--
-- OUTPUT:      System log messages
--

local json = require("json")
local parm = json.decode(arg[1])

local level = parm["level"] or core.CRITICAL
local duration = parm["duration"] or 0

-- Attach monitor to response queue --
local userlog = core.monitor(rspq, level, true)

-- Pend for duration (in 1 second intervals to allow hooks to execute) --
local seconds = 0
while duration == 0 or seconds < duration do
    seconds = seconds + 1
    sys.wait(1)
end
