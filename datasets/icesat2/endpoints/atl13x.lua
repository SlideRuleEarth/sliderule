--
-- ENDPOINT:    /source/atl13x
--
local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl13", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

-- attempt to populate resources on initial request
if parms["key_space"] == core.INVALID_KEY then
    -- query for resources
    local atl13_parms = parms["atl13"]
    local response = nil
    if atl13_parms["refid"] > 0 then
        response = core.manager("GET", string.format("/manager/ams/atl13?refid=%d", atl13_parms["refid"]))
    elseif #atl13_parms["name"] < 0 then
        response = core.manager("GET", string.format("/manager/ams/atl13?name=%s", atl13_parms["name"]))
    elseif atl13_parms["coord"]["lat"] ~= 0.0 or atl13_parms["coord"]["lon"] ~= 0.0 then
        response = core.manager("GET", string.format("/manager/ams/atl13?lon=%f&lat=%f", atl13_parms["coord"]["lon"], atl13_parms["coord"]["lat"]))
    end
    -- get resources
    if response then
        local rc, data = pcall(json.decode, response)
        if rc then
            rqst["parms"]["resources"] = data["granules"]
        else
            local userlog = msg.publish(rspq)
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> failed to parse response from manager: %s", rspq, response))
            return
        end
    end
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
