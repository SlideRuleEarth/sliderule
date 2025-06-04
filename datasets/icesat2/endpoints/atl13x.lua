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
    local atl13_parms = parms["atl13"]
    -- query for resources
    local resources_set_by_ams = false
    local response = nil
    if atl13_parms["refid"] > 0 then
        response = core.manager("GET", string.format("/manager/ams/atl13?refid=%d", atl13_parms["refid"]))
    elseif #atl13_parms["name"] > 0 then
        response = core.manager("GET", string.format("/manager/ams/atl13?name=%s", string.gsub(atl13_parms["name"], " ", "%%20")))
    elseif atl13_parms["coord"]["lat"] ~= 0.0 or atl13_parms["coord"]["lon"] ~= 0.0 then
        response = core.manager("GET", string.format("/manager/ams/atl13?lon=%f&lat=%f", atl13_parms["coord"]["lon"], atl13_parms["coord"]["lat"]))
    end
    -- get resources
    if response then
        local rc, data = pcall(json.decode, response)
        if rc then
            rqst["parms"]["atl13"]["refid"] = data["refid"]
            rqst["parms"]["resources"] = data["granules"]
            resources_set_by_ams = true
        else
            local userlog = msg.publish(rspq)
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> failed to parse response from manager: %s", rspq, response))
            return
        end
    end
    -- apply polygon if supplied
    if resources_set_by_ams and parms:length("poly") > 0 then
        local userlog = msg.publish(rspq)
        rqst["parms"]["asset"] = parms["asset"] -- needed by earth data search function inside get_resources
        local status, cmr_response = dataframe.get_resources(rqst["parms"], rspq, userlog)
        if status == RC_SUCCESS and type(cmr_response) == 'table' then

            -- pull out all resources from cmr query
            local resources_to_process = {}
            for _,resource in ipairs(cmr_response) do
                resources_to_process[resource] = true
            end
            -- keep only resources also found in ams query
            local resources = {}
            for _,resource in ipairs(rqst["parms"]["resources"]) do
                if resources_to_process[resource] then
                    table.insert(resources, resource)
                end
            end
            -- update request parameters
            rqst["parms"]["resources"] = resources
        else
            -- report error back to user
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> earthdata queury failed: %d", rspq, status))
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
