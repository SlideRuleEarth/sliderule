-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local dataframe = require("dataframe")
    local json      = require("json")
    local rqst      = json.decode(arg[1])
    local channels  = 6 -- number of dataframes per resource

    -- determine default asset
    local default_asset = "icesat2-atl24"
    local rqst_resource = rqst["resource"]
    if rqst_resource and (string.sub(rqst_resource, 38, 40) == "001") then
        default_asset = "icesat2-atl24v1"
    end

    -- create parameters
    local parms = icesat2.parms(rqst["parms"], rqst["key_space"], default_asset, rqst_resource)

    -- proxy the request
    dataframe.proxy("atl24x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local dataframes = {}
        local runners = {}
        local resource = parms["resource"]

        -- atl09
        if parms:stage(icesat2.ATL09) then
            local utils = require("icesat2_utils")
            if not utils.add_atmo_runner(runners, parms, userlog) then
                return {}, {} -- unable to process this resource
            end
        end

        -- atl24
        local atl24h5 = h5coro.object(parms["asset"], resource)
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = icesat2.atl24x(beam, parms, atl24h5, _rqst.rspq)
            if not dataframes[beam] then
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
            end
        end

        return dataframes, runners
    end)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "ATL24 Dataframe",
    description = "Spatially and temporally subsets ATL24 granule bathemtry data with additional filters (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}
