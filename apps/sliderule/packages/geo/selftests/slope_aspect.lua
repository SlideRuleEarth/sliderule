local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()
local base64 = require("base64")

-- Setup --

-- planar_ramp_1m.tif is a synthetic 101x101 Float32 plane on a 1 meter UTM zone 12N
-- grid (EPSG:32612): dz/dx = 0.25 m/m to the east, dz/dy = 0.125 m/m to the north.
-- Every value is an exact multiple of 0.125, so the raster carries the plane without
-- rounding.  The slope of a plane does not depend on the size of the window used to
-- estimate it, so the same slope must be returned at every slope_scale_length.
local expSlope = 15.616129405024543   -- atan(hypot(0.25, 0.125)) in degrees
local sigma = 1.0e-6

local lon = -108.2246653625
local lat =   39.0856593976

-- Self Test --

runner.unittest("Slope Scale Length", function()

    -- open raster
    local f = io.open(dirpath.."../data/planar_ramp_1m.tif", "rb")
    assert(f)
    local rasterfile = f:read("*a")
    f:close()
    local encodedRaster = base64.encode( rasterfile )
    local len = string.len(encodedRaster)

    -- slope_scale_length in meters and the expected number of pixels in the window
    local scales = {{0, 9}, {20, 441}, {40, 1681}}

    for _, scale in ipairs(scales) do
        local length, count = scale[1], scale[2]

        local params = {
            data = encodedRaster,
            length = len,
            date = 0,
            samples = {
                elevation_bands = {"1"},
                slope_aspect = true,
                slope_scale_length = length
            }
        }
        local robj = geo.userraster(params)
        runner.assert(robj ~= nil)

        -- sample
        local tbl, err = robj:sample(lon, lat, 0)
        runner.assert(err == 0, "failed to sample raster", true)
        runner.assert(tbl ~= nil, "failed to sample raster", true)

        local slope = tbl[1]["slope"]
        local slope_count = tbl[1]["slope_count"]
        print(string.format("slope_scale_length = %4d  slope: %.9f  slope_count: %d", length, slope, slope_count))

        runner.assert(slope_count == count, string.format("slope_count = %d, expected = %d", slope_count, count))
        runner.assert(math.abs(slope - expSlope) < sigma, string.format("slope = %f, expected = %f, error = %f", slope, expSlope, math.abs(slope - expSlope)))
    end
end)

-- Report Results --

runner.report()
