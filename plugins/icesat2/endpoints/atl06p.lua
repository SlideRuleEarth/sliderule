--
-- ENDPOINT:    /source/atl06p
--
-- PURPOSE:     fan out atl06 requests to multiple back-end servers and collect responses
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>",
--                  "resources":    "<url of hdf5 file or object>",
--                  "parms":        {<table of parameters>},
--                  "timeout":      <seconds to wait for response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      stream of responses
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"]
local resources = rqst["resources"]
local parms = rqst["parms"]
local timeout = rqst["timeout"] or 600000 -- 10 minutes

-- Proxy Request --
print("started...")
local atl06p = icesat2.atl06proxy(atl03_asset, resources, json.encode(parms), timeout, rspq)

-- Wait Until Proxy Completes --
local duration = 0
local interval = 10000 -- 10 seconds
while __alive() and not atl06p:waiton(interval) do
    duration = duration + interval
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("proxy request <%s> timed-out after %d seconds", rspq, duration / 1000))
        return
    end
end

-- Done --
print("finished...")
return
