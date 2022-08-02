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
--                  "timeout":      <milliseconds to wait for first response>
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
local resources = rqst["resources"]
local orchestrator = os.getenv("ORCHESTRATOR") or "http://127.0.0.1:8050"

-- Loop Through Resources --
for i,resource in ipairs(resources) do

    -- Post Initial Status Progress --
    userlog:sendlog(core.INFO, string.format("request <%s> proxied to atl06 [%d out of %d] for %s ...", rspq, i, #resources, resource))


end

-- While not done...
--  Get Available Nodes (as list)
--  Pass off processing resources to each of the nodes
--  As nodes complete, unlock them and request additional locks for the remaining resources

-- Need maximum number of worker threads


local response, status = netsvc.get(orchestrator.."/discovery/lock", false, false, '{"service":"sliderule", "nodesNeeded":1, "timeout":600}')
if status then
    print(response)
else
    sys.log(core.ERROR, "Failed to get services from <"..orchestrator..">: "..response)
end


return
