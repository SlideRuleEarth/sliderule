--
-- ENDPOINT:    /source/samples
--
-- PURPOSE:     return a sample from a raster dataset given a lat and lon
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
    if #coord == 1 then
        -- Single coordinate: Use dem:sample
        local lon = coord[1][1]
        local lat = coord[1][2]
        local height = coord[1][3] or 0 -- Default to 0 if not provided
        local sample, error = dem:sample(lon, lat, height)
        table.insert(samples, sample)
        table.insert(errors, error or 0)
    else
        -- Multiple coordinates: Use dem:batchsample
        local lons = {}
        local lats = {}
        local heights = {}

        for _, position in ipairs(coord) do
            table.insert(lons, position[1])
            table.insert(lats, position[2])
            table.insert(heights, position[3] or 0) -- Default to 0 if not provided
        end

        local batchSamples, error = dem:batchsample(lons, lats, heights)

        for i, sample in ipairs(batchSamples) do
            table.insert(samples, sample)
            table.insert(errors, error) -- Apply the same error code to all samples
        end
    end
else
    -- Handle Missing DEM
    for _ in ipairs(coord) do
        table.insert(samples, nil)
        table.insert(errors, "DEM not available")
    end
end

-- Return Response
return json.encode({samples=samples, errors=errors})