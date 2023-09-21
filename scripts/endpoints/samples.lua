--
-- ENDPOINT:    /source/samples
--
-- PURPOSE:     return a sample from a rastar dataset given a lat and lon
--
-- INPUT:       rqst
--              {
--                  "samples": {<geoparms>}
--                  "coordinates": [
--                      [<longitude>, <latitude>],
--                      [<longitude>, <latitude>]...
--                  ]
--              }
--
-- OUTPUT:      samples
--

local json = require("json")

-- Request Parameters --
local rqst = json.decode(arg[1])
local coord = rqst["coordinates"]

-- Get Samples --
local dem = geo.raster(geo.parms(rqst[geo.PARMS]))

-- Build Table --
local samples = {}
local errors = {}
if dem then
    for _, position in ipairs(coord) do
        local lon = position[1]
        local lat = position[2]
        local height = position[3] -- optional
        local sample, error = dem:sample(lon, lat, height)
        table.insert(samples, sample)
        table.insert(errors, error)
    end
end

-- Return Response
return json.encode({samples=samples, errors=errors})