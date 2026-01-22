--
-- ENDPOINT:    /source/atl08x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl08", rqst["resource"])
local channels  = 6 -- number of dataframes per resource (one per beam)

dataframe.proxy("atl08x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
    local dataframes = {}
    local resource = parms["resource"]

    local atl08h5 = h5.object(parms["asset"], resource)
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl08x(beam, parms, atl08h5, _rqst.rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
        end
    end

    return dataframes, {}
end)
