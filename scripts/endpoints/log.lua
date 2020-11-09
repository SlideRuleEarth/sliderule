--
-- ENDPOINT:    /engine/log
--
-- INPUT:       arg[1] -
--              {
--                  "level":    "<log level string>" -OR- <log level number>
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




local leakylog = msg.publish(rspq)





-- Create User Log --
local userlog = core.logger(rspq, level, true)

-- Pend for Duration --
local seconds = 0
while duration == 0 or seconds < duration do
    seconds = seconds + 1
    sys.wait(1)
end





leakylog:sendlog(core.CRITICAL, string.format("processing of complete\n"))
return
