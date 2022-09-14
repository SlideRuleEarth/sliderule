--
-- ENDPOINT:    /source/atl06p
--
-- PURPOSE:     fan out atl06 requests to multiple back-end servers and collect responses
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>",
--                  "resources":    ["<url of hdf5 file or object>", ...],
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
local timeout = rqst["timeout"] or icesat2.API_TIMEOUT

-- Proxy Request --
local proxy = icesat2.proxy("atl06", atl03_asset, resources, json.encode(parms), timeout, rspq)

-- Wait Until Proxy Completes --
local duration = 0
local interval = 10 -- seconds
while (userlog:numsubs() > 0) and not proxy:waiton(interval * 1000) do
    duration = duration + interval
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("proxy request <%s> timed-out after %d seconds", rspq, duration))
        return
    end
end

-- Done --
return
