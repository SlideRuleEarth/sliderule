-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local dataframe = require("dataframe")
    local json      = require("json")
    local earthdata = require("earth_data_query")
    local rqst      = json.decode(arg[1])
    local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2-atl13", rqst["resource"])
    local channels  = 6 -- number of dataframes per resource

    -- attempt to populate resources on initial request
    if parms["key_space"] == core.INVALID_KEY then
        local rc_ams, response = earthdata.ams(rqst["parms"], nil, true, "ATL13")
        if rc_ams == earthdata.SUCCESS or rc_ams == earthdata.RC_RSPS_TRUNCATED then
            if not rqst["parms"]["atl13"] then rqst["parms"]["atl13"] = {} end
            rqst["parms"]["atl13"]["refid"] = response["refid"]
            if not rqst["parms"]["resources"] then
                rqst["parms"]["resources"] = response["granules"]
            end
        end
    end

    -- set locks if not supplied (since ATL13 requests are subsetted without a polygon)
    rqst["parms"]["locks"] = rqst["locks"] or 1

    -- proxy request
    dataframe.proxy("atl13x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
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

        -- atl13
        local h5obj = h5coro.object(parms["asset"], resource)
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = icesat2.atl13x(beam, parms, h5obj, _rqst.rspq)
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
    name = "ATL13 Dataframe",
    description = "Spatially and temporally subsets ATL13 granule lake metrics with additional filters (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}
