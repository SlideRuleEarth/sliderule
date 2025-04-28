--
-- ENDPOINT:    /source/event
--
-- INPUT:       arg[1] -
--              {
--                  "type":     <core.LOG | core.TRACE | core.TELEMETRY>
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

-- Get parameters
local type = global.eval(parm["type"]) or core.LOG
local duration = parm["duration"] or 600

-- Create dispatcher and publisher
local dispatcher = streaming.dispatcher(core.EVENTQ)
local publisher = streaming.publish(rspq)

-- Attach publisher to specified record types
if type & core.LOG      then dispatcher:attach(publisher, "eventrec")   end
if type & core.TRACE    then dispatcher:attach(publisher, "tracerec")   end
if type & core.TELEMETRY   then dispatcher:attach(publisher, "metricrec")  end

-- Run dispatcher
dispatcher:run()

-- Watch response queue
local watchq = msg.publish(rspq)

-- Pend for duration (in 1 second intervals to allow hooks to execute) --
local seconds = 0
if duration > 600 then duration = 600 end
while (watchq:numsubs() > 0) and (seconds < duration) do
    seconds = seconds + 1
    sys.wait(1)
end
