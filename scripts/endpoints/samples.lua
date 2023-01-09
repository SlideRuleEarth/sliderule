--
-- ENDPOINT:    /source/samples
--
-- PURPOSE:     return a sample from the ArcticDEM given a lat and lon
--
-- INPUT:       rqst
--              {
--                  "dem-asset":          "<mosaic or strip>"
--                  "sampling-algorithm": "<NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, Gauss>"
--                  "sampling-radius":    <meters, 0 indicates resampling with 8 buffer pixels plus pixel of interest>
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
local dem_asset = rqst["dem-asset"] or "arcticdem-mosaic"
local sampling_alg = rqst["sampling-algorithm"] or geo.NearestNeighbour
local sampling_radius = rqst["sampling-radius"] or 0
local coord = rqst["coordinates"]

-- Get Samples --
local dem = geo.vrt(dem_asset, sampling_alg, sampling_radius)

-- Build Table --
local samples = {}
for _, position in ipairs(coord) do
    local lon = position[1]
    local lat = position[2]
    local sample, status = dem:sample(lon, lat)
    if status then
        table.insert(samples, sample)
    end
end

-- Return Response
return json.encode({samples=samples})