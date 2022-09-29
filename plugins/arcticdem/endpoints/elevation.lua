--
-- ENDPOINT:    /source/elevation
--
-- PURPOSE:     return an elevation from the ArcticDEM given a lat and lon
--
-- INPUT:       rqst
--              {
--                  "dem-asset":          "<name of asset to use>"
--                  "dem-type":           "<strip or mosaic>"
--                  "sampling-algorithm": "<NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, Gauss>"
--                  "sampling-radius":    <meters, 0 indicates resampling with 8 buffer pixels plus pixel of interest>
--                  "coordinates": [
--                      [<longitude>, <latitude>],
--                      [<longitude>, <latitude>]...
--                  ]
--              }
--
-- OUTPUT:      elevation
--

local json = require("json")

-- Request Parameters --
local rqst = json.decode(arg[1])
local dem_asset = rqst["dem-asset"] or "arcticdem-local"
local dem_type = rqst["dem-type"] or "mosaic"
local sampling_alg = rqst["sampling-algorithm"] or "NearestNeighbour"
local sampling_radius = rqst["sampling-radius"] or 0
local coord = rqst["coordinates"]

local sample, status, lat, lon
local samples = {}


-- Get Elevation --
local dem = arcticdem.raster(dem_type, sampling_alg, sampling_radius)

for _, position in ipairs(coord) do
    lon = position[1]
    lat = position[2]
    sample, status = dem:sample(lon, lat)
    table.insert(samples, sample)
end


-- Return Response
return json.encode({elevations=samples})