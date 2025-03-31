--
-- ENDPOINT:    /source/bathy
--
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

-------------------------------------------------------
-- proxy request
-------------------------------------------------------
dataframe.proxy("atl24x", parms, rqst["parms"], rspq, channels, function(userlog)

    local resource      = parms["resource"]
    local bathymask     = bathy.mask()
    local atl03h5       = h5.object(parms["asset"], resource)
    local atl09h5       = util.get_atl09(parms, t0, t1, userlog, resource)
--    local ndwi          = parms["generate_ndwi"] and util.get_ndwi(parms, resource) or nil
--    local granule       = (parms["output"]["format"] == "h5") and bathy.granule(parms, atl03h5, rspq) or nil
    local kd490         = util.get_viirs(parms, rgps)
    local seasurface    = bathy.seasurface(parms)
    local refraction    = bathy.refraction(parms)
    local uncertainty   = bathy.uncertainty(parms, kd490)

    local runners = {seasurface, refraction, uncertainty}
    local dataframes = {}
    for _, beam in ipairs(parms["beams"]) do
        dataframes[beam] = bathy.dataframe(beam, parms, bathymask, atl03h5, atl09h5, rspq)
        if not dataframes[beam] then
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create bathy dataframe for beam %s", rspq, resource, beam))
        end
    end

    return dataframes, runners

end)
