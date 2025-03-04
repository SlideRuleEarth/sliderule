--
-- ENDPOINT:    /source/atl03x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

dataframe.proxy("atl03x", parms, rqst["parms"], rspq, channels, function(userlog)
    local dataframes = {}
    local runners = {}
    local resource = parms["resource"]
    -- atl06
    if parms:stage(icesat2.ATL06) then
        local fitter = icesat2.fit(parms)
        table.insert(runners, fitter)
    end
    -- atl08
    local atl08h5 = nil
    if parms:stage(icesat2.ATL08) then
        atl08h5 = h5.object(parms["asset"], resource:gsub("ATL03", "ATL08"))
    end
    -- atl03x
    local atl03h5 = h5.object(parms["asset"], resource)
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl03x(beam, parms, atl03h5, atl08h5, rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create dataframe for beam %s", rspq, resource, beam))
        end
    end
    -- return back to proxy
    return dataframes, runners
end)
