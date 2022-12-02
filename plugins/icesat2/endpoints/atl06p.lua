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
local timeout = parms["rqst-timeout"] or parms["timeout"] or icesat2.RQST_TIMEOUT
local node_timeout = parms["node-timeout"] or parms["timeout"] or icesat2.NODE_TIMEOUT
local output_parms = parms["output"]

-- Initialize Timeouts --
local duration = 0
local interval = 10 -- seconds

-- Handle Output Options --
local output_dispatch = nil
if output_parms then
    local output_filename = output_parms["path"]
    local output_format = output_parms["format"]
    if output_format == "parquet" then
        -- Parquet Writer --
        local parquet_builder = arrow.parquet(output_filename, rspq, "atl06rec.elevation", rqstid)
        output_dispatch = core.dispatcher(rspq)
        output_dispatch:attach(parquet_builder, "atl06rec")
        output_dispatch:run()
    end
end

-- Proxy Request --
local proxy = icesat2.proxy("atl06", atl03_asset, resources, json.encode(parms), node_timeout, rspq)

-- Wait Until Proxy Completes --
while (userlog:numsubs() > 0) and not proxy:waiton(interval * 1000) do
    duration = duration + interval
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("proxy request <%s> timed-out after %d seconds", rspq, duration))
        return
    end
end

sys.wait(5)
-- Wait Until Dispatch Completion --
--if output_dispatch then
--    while (userlog:numsubs() > 0) and not output_dispatch:waiton(interval * 1000) do
--        duration = duration + interval
--        if timeout >= 0 and duration >= timeout then
--            userlog:sendlog(core.ERROR, string.format("proxy dispatch <%s> timed-out after %d seconds", rspq, duration))
--            return
--        end
--    end
--end

-- Done --
return
