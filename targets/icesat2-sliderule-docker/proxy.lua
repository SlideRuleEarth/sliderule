--
-- ENDPOINT:    /source/proxy
--
-- PURPOSE:     fan out request to multiple back-end servers and collect responses
--
-- INPUT:       rqst
--              {
--                  "api":          "<endpoint to proxy to>",
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
local api = rqst["api"]
local resources = rqst["resources"]
local orchestrator = os.getenv("ORCHESTRATOR") or "http://10.0.1.5:8051"

-- Loop Through Resources --
for i,resource in ipairs(resources) do

    -- Post Initial Status Progress --
    userlog:sendlog(core.INFO, string.format("request <%s> proxied to %s [%d out of %d] for %s ...", rspq, api, i, #resources, resource))


end

local url = orchestrator.."/discovery/"


---- "action": "availableNode" | default is "allNodes"

local response, status = netsvc.get(url, false, false, '{"service":"sliderule"}')
if status then
    print(response)
else
    sys.log(core.ERROR, "Failed to get services from <"..url..">: "..response)
end


return
