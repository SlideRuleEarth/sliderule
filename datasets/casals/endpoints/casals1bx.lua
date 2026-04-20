-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local dataframe = require("dataframe")
    local json      = require("json")
    local rqst      = json.decode(arg[1])
    local parms     = casals.parms(rqst["parms"], rqst["key_space"], "casals1b", rqst["resource"])
    local channels  = 1 -- number of dataframes per resource

    dataframe.proxy("casals1bx", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local resource = parms["resource"]
        local casals1bh5 = h5coro.object(parms["asset"], resource)
        local df = casals.casals1bx(parms, casals1bh5, _rqst.rspq)
        if not df then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe", _rqst.id, resource))
        end
        return {swath=df}, {}
    end)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "CASALS 1B Dataframe",
    description = "Spatially and temporally subsets CASALS 1B waveforms (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}
