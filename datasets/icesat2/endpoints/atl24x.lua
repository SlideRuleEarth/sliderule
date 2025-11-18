--
-- ENDPOINT:    /source/atl24x
--

local dataframe = require("dataframe")
local json      = require("json")
local ams_atl24 = require("ams_atl24")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl24", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

-- attempt to populate resources on initial request
if (parms["key_space"] == core.INVALID_KEY) and
   (rqst["parms"] and not rqst["parms"]["resources"]) and
   (rqst["ams"]) then
    -- construct query parameters
    rqst["ams"]["t0"] = rqst["ams"]["t0"] or rqst["t0"]
    rqst["ams"]["t1"] = rqst["ams"]["t1"] or rqst["t1"]
    rqst["ams"]["poly"] = rqst["ams"]["poly"] or rqst["poly"]
    -- query AMS
    local response = ams_atl24.query(rqst["ams"])
    if response then
        local rc, data = pcall(json.decode, response)
        if rc then
            local granules = {}
            for granule in data["granules"] do
                granules[#granules + 1] = granule
            end
            rqst["parms"]["resources"] = granules
        else
            local userlog = msg.publish(_rqst.rspq)
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> failed to parse response from asset metadata service: %s", _rqst.id, response))
            return
        end
    end
end

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
