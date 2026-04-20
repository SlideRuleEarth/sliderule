-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local dataframe = require("dataframe")
    local json      = require("json")
    local rqst      = json.decode(arg[1])
    local parms     = gedi.parms(rqst["parms"], rqst["key_space"], "gedil1b", rqst["resource"])
    local channels  = 8 -- number of dataframes per resource (one per beam)

    dataframe.proxy("gedi01bx", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local dataframes = {}
        local resource = parms["resource"]

        local gedi01bh5 = h5coro.object(parms["asset"], resource)
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = gedi.gedi01bx(beam, parms, gedi01bh5, _rqst.rspq)
            if not dataframes[beam] then
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
            end
        end

        return dataframes, {}
    end)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "GEDI 1B Dataframe",
    description = "Spatially and temporally subsets waveforms from multiple GEDI 1B granules with additional filters (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}