local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()
local base64 = require("base64")

-- Self Test --

runner.unittest("GeoUser Raster", function()
    -- open area of interest
    local f = io.open(dirpath.."../data/geouser_test_raster.tif", "rb")
    assert(f)
    local rasterfile = f:read("*a")
    f:close()

    -- open raster
    local encodedRaster = base64.encode( rasterfile )
    local len = string.len(encodedRaster)
    local params = {
        data = encodedRaster,
        length = len,
        date = 0,
        samples = {
            elevation_bands = {"1"}
        }
    }
    local robj = geo.userraster(params)
    runner.assert(robj ~= nil)

    -- sample
    local tbl, err = robj:sample(149.00001, 69.00001, 0)
    runner.assert(err == 0, "failed to sample raster", true)
    runner.assert(tbl ~= nil, "failed to sample raster", true)
    runner.assert(math.abs(10 - tbl[1]["value"]) < 0.01)

    -- dimensions
    local rows, cols  = robj:dim()
    runner.assert(rows == 12)
    runner.assert(cols == 12012)

    -- bounding box
    local lon_min, lat_min, lon_max, lat_max = robj:bbox()
    runner.assert(lon_min ~= 0)
    runner.assert(lat_min ~= 0)
    runner.assert(lon_max ~= 0)
    runner.assert(lat_max ~= 0)

    -- cellsize
    local cellsize = robj:cell()
    runner.assert(math.abs(0.000083 - cellsize) < 0.000001)
end)

-- Report Results --

runner.report()
