--
-- ENDPOINT:    /source/atl03x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

dataframe.proxy("atl03x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
    local dataframes = {}
    local runners = {}
    local resource = parms["resource"]
    -- custom algorithms
    if parms:stage(icesat2.ATL06) then -- surface fitter
        local fitter = icesat2.fit(parms)
        table.insert(runners, fitter)
    elseif parms:stage(icesat2.PHOREAL) then -- phoreal
        local phoreal = icesat2.phoreal(parms)
        table.insert(runners, phoreal)
    end
    -- atl08
    local atl08h5 = nil
    if parms:stage(icesat2.ATL08) then
        atl08h5 = h5.object(parms["asset"], resource:gsub("ATL03", "ATL08"))
    end
    -- atl24
    local atl24h5 = nil
    if parms:stage(icesat2.ATL24) then
        local atl24_filename = resource:gsub("ATL03", "ATL24"):gsub(".h5", "_001_01.h5")
        atl24h5 = h5.object("atl24-s3", atl24_filename) -- asset will change once NSIDC releases ATL24
    end
    -- atl03x
    local atl03h5 = h5.object(parms["asset"], resource)
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl03x(beam, parms, atl03h5, atl08h5, atl24h5, _rqst.rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
        end
    end
    -- return back to proxy
    return dataframes, runners
end)
