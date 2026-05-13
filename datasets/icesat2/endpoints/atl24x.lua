-------------------------------------------------------
-- initialization
-------------------------------------------------------
local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local channels  = 6 -- number of dataframes per resource
local default_asset = "icesat2-atl24" -- determine default asset
local rqst_resource = rqst["resource"]
if rqst_resource and (string.sub(rqst_resource, 38, 40) == "001") then
    default_asset = "icesat2-atl24v1"
end
local parms = icesat2.parms(rqst["parms"], rqst["key_space"], default_asset, rqst_resource)

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

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
    parms = parms,
    name = "ATL24 Dataframe",
    description = "Spatially and temporally subsets ATL24 granule bathemtry data with additional filters",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        tags = "x-series, icesat2",
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/Icesat2Parameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "$ref": "../components/schemas/Atl24DataFrame.json"
            }
        } ]]
    }
}
