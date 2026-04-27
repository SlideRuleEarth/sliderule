-------------------------------------------------------
-- initialization
-------------------------------------------------------
local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = gedi.parms(rqst["parms"], rqst["key_space"], "gedil4a", rqst["resource"])
local resource  = parms["resource"]
local channels  = 8 -- number of dataframes per resource (one per beam)

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    dataframe.proxy("gedi04ax", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local dataframes = {}
        local gedi04ah5 = h5coro.object(parms["asset"], resource)
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = gedi.gedi04ax(beam, parms, gedi04ah5, _rqst.rspq)
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
    parms = parms,
    name = "GEDI 4A Dataframe",
    description = "Spatially and temporally subsets above ground biomass density from multiple GEDI 4A granules with additional filters (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}