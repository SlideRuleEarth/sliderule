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
local timeout = rqst["timeout"] or 600

local atl06p = icesat2.atl06proxy(atl03_asset, resources, json.encode(parms), timeout, rspq)


-- While not done...
--  Get Available Nodes (as list)
--  Pass off processing resources to each of the nodes
--  As nodes complete, unlock them and request additional locks for the remaining resources

-- Need maximum number of worker threads

--[[
local response, status = netsvc.get(orchestrator.."/discovery/lock", false, false, '{"service":"sliderule", "nodesNeeded":1, "timeout":600}')
if status then
    print(response)
else
    sys.log(core.ERROR, "Failed to get services from <"..orchestrator..">: "..response)
end
]]

return
