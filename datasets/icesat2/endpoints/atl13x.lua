--
-- ENDPOINT:    /source/atl13x
--

local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl13", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

-- get resources to process
local atl13_parms = parms["atl13"]
if #atl13_parms["reference_id"] > 0 then
    -- query manager with reference id
    rqst["resources"] = {}
elseif #atl13_parms["name"] < 0 then
    -- query manager with lake name
    rqst["resources"] = {}
elseif atl13_parms["coordinate"]["lat"] ~= 0.0 or atl13_parms["coordinate"]["lon"] ~= 0.0 then
    -- query manager with coordinate
    rqst["resources"] = {}
end

-- proxy request
dataframe.proxy("atl13x", parms, rqst["parms"], rspq, channels, function(userlog)
    local dataframes = {}
    local runners = {}
    local resource = parms["resource"]
    local h5obj = h5.object(parms["asset"], resource)
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl13x(beam, parms, h5obj, rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", rspq, resource, beam))
        end
    end
    return dataframes, runners
end)
