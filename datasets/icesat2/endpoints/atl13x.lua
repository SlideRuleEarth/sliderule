--
-- ENDPOINT:    /source/atl13x
--
local dataframe = require("dataframe")
local json      = require("json")
local earthdata = require("earth_data_query")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl13", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

-- attempt to populate resources on initial request
if parms["key_space"] == core.INVALID_KEY then
    -- get resources
    local resources_set_by_ams = false
    local rc_ams, response = earthdata.ams(rqst["parms"], nil, true, "ATL13")
    if rc_ams == earthdata.SUCCESS or rc_ams == earthdata.RC_RSPS_TRUNCATED then
        rqst["parms"]["atl13"]["refid"] = response["refid"]
        if not rqst["parms"]["resources"] then
            rqst["parms"]["resources"] = response["granules"]
            resources_set_by_ams = true
        end
    end
    -- apply polygon if supplied
    if resources_set_by_ams and parms:length("poly") > 0 then
        local rc_cmr, cmr_response = earthdata.cmr(rqst["parms"], nil, false, "ATL13")
        if rc_cmr == RC_SUCCESS and type(cmr_response) == 'table' then
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
            sys.log(core.CRITICAL, string.format("request <%s> earthdata queury failed: %d", _rqst.id, rc_cmr))
        end
    end
end

-- proxy request
dataframe.proxy("atl13x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
    local dataframes = {}
    local runners = {}
    local resource = parms["resource"]
    local h5obj = h5.object(parms["asset"], resource)
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = icesat2.atl13x(beam, parms, h5obj, _rqst.rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
        end
    end
    return dataframes, runners
end)
