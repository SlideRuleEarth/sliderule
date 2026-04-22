-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json          = require("json")
    local dataframe     = require("dataframe")
    local util          = require("bathy_utils")
    local rqst          = json.decode(arg[1])
    local parms         = bathy.parms(rqst["parms"], 0)
    local rdate         = string.format("%04d-%02d-%02dT00:00:00Z", parms["year"], parms["month"], parms["day"])
    local rgps          = time.gmt2gps(rdate)
    local rdelta        = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
    local t0            = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
    local t1            = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))
    local channels      = 6 -- number of dataframes per resource
    -- proxy request
    dataframe.proxy("atl24x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local resource      = parms["resource"]
        local bathymask     = bathy.mask()
        local atl03h5       = h5coro.object(parms["asset"], resource)
    --    local ndwi          = parms["generate_ndwi"] and util.get_ndwi(parms, resource) or nil
    --    local granule       = (parms["output"]["format"] == "h5") and bathy.granule(parms, atl03h5, _rqst.rspq) or nil
        local kd490         = util.get_viirs(parms, rgps)
        local seasurface    = bathy.seasurface(parms)
        local refraction    = bathy.refraction(parms)
        local uncertainty   = bathy.uncertainty(parms, kd490)
        local runners       = {seasurface, refraction, uncertainty}
        local dataframes    = {}
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
    name = "Bathymetry Dataframe",
    description = "Generate custom bathymetry data using parameters supplied in the request (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}