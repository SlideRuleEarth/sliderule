-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json          = require("json")
local dataframe     = require("dataframe")
local util          = require("bathy_utils")
local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local granule       = parms["granule"]
local rdate         = string.format("%04d-%02d-%02dT00:00:00Z", granule["year"], granule["month"], granule["day"])
local rgps          = time.gmt2gps(rdate)
local channels      = 6 -- number of dataframes per resource
local resource      = parms["resource"]

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local bathymask     = bathy.mask()
    local atl03h5       = h5coro.object(parms["asset"], resource)
    local kd490         = util.get_viirs(parms, rgps)
    local seasurface    = bathy.seasurface(parms)
    local refraction    = bathy.refraction(parms)
    local uncertainty   = bathy.uncertainty(parms, kd490)
    dataframe.proxy("bathy", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local dataframes = {}
        local runners = {seasurface, refraction, uncertainty}
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = bathy.dataframe(beam, parms, bathymask, atl03h5, _rqst.rspq)
            if not dataframes[beam] then
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create bathy dataframe for beam %s", _rqst.rspq, resource, beam))
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
    name = "Bathymetry Dataframe",
    description = "Generate custom bathymetry data using parameters supplied in the request (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        tags = "x-series, bathy",
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/BathyParameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "$ref": "../components/schemas/BathyDataFrame.json"
            }
        } ]]
    }
}