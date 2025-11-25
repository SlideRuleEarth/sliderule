--
-- ENDPOINT:    /source/atl24x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local channels  = 6 -- number of dataframes per resource

-- determine default asset
local default_asset = "atl24-s3"
local rqst_resource = rqst["resource"]
if rqst_resource and (string.sub(rqst_resource, 38, 40) == "001") then
    default_asset = "icesat2-atl24"
end

-- create parameters
local parms = icesat2.parms(rqst["parms"], rqst["key_space"], default_asset, rqst_resource)

-- proxy the request
dataframe.proxy("atl24x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
    local dataframes = {}
    local runners = {}
    local resource = parms["resource"]
    local atl24h5 = h5.object(parms["asset"], resource)
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl24x(beam, parms, atl24h5, _rqst.rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
        end
    end
    return dataframes, runners
end)
