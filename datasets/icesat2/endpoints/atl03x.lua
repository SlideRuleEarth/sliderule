--
-- ENDPOINT:    /source/atl03x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local _parms    = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

dataframe.proxy("atl03x", _parms, rqst["parms"], rspq, channels, function(parms, rspq, userlog)
    local dataframes = {}
    local resource = parms["resource"]
    local atl03h5 = h5.object(parms["asset"], resource)
    local atl08h5 = h5.object(parms["asset"], resource:gsub("ATL03", "ATL08"))
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl03x(beam, parms, atl03h5, atl08h5, rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create dataframe for beam %s", rspq, resource, beam))
        end
    end
    return dataframes, {}
end)
