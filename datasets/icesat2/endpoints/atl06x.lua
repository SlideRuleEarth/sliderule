-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local dataframe = require("dataframe")
    local json      = require("json")
    local rqst      = json.decode(arg[1])
    local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl06", rqst["resource"])
    local channels  = 6 -- number of dataframes per resource (one per beam)
    -- proxy request
    dataframe.proxy("atl06x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
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

        -- atl06
        local atl06h5 = h5coro.object(parms["asset"], resource)
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = icesat2.atl06x(beam, parms, atl06h5, _rqst.rspq)
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
    name = "ATL06 Dataframe",
    description = "Spatially and temporally subsets ATL06 granules elevations with additional filters (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}