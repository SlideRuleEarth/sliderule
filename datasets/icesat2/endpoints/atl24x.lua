--
-- ENDPOINT:    /source/atl24x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "atl24-s3", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

if sys.ispublic() then -- verify on a private cluster
    print(string.format("request <%s> forbidden on public cluster... exiting", rspq))
    return
end

local function get_atl24_granule(filename)
    if filename:find("ATL03") then
        return filename:gsub("ATL03", "ATL24"):gsub(".h5", "_001_01.h5")
    else
        return filename
    end
end

dataframe.proxy("atl24x", parms, rqst["parms"], rspq, channels, function(userlog)
    local dataframes = {}
    local runners = {}
    local resource = parms["resource"]
    local atl24h5 = h5.object(parms["asset"], get_atl24_granule(resource))
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl24x(beam, parms, atl24h5, rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create dataframe for beam %s", rspq, resource, beam))
        end
    end
    return dataframes, runners
end)
