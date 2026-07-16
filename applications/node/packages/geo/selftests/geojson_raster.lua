local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Self Test --

runner.unittest("GeoJSON Raster", function()
    -- open area of interest
    local f = io.open(dirpath.."../data/grandmesa.geojson", "r")
    assert(f)
    local vectorfile = f:read("*a")
    f:close()

    -- open raster
    local cellsize = 0.01
    local robj = geo.geojson(vectorfile, cellsize)
    runner.assert(robj ~= nil)

    -- sample
    local tbl, err = robj:sample(-108, 39, 0)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)
    runner.assert(tbl[1]["value"] == 1)

    -- dimensions
    local rows, cols  = robj:dim()
    runner.assert(rows == 37)
    runner.assert(cols == 61)

    -- bounding box
    local lon_min, lat_min, lon_max, lat_max = robj:bbox()
    runner.assert(lon_min ~= 0)
    runner.assert(lat_min ~= 0)
    runner.assert(lon_max ~= 0)
    runner.assert(lat_max ~= 0)

    -- celsize
    local _cellsize = robj:cell()
    runner.assert(_cellsize == cellsize)

    -- edge of bbox
    tbl, err = robj:sample(-108.34, 38.90, 0)
    runner.assert(err == 0)
    runner.assert(tbl ~= nil)
    runner.assert(tostring(tbl[1]["value"]) == "nan")

    -- outside of bbox
    tbl, err = robj:sample(-100, 40, 0)
    runner.assert(err ~= 0)
    runner.assert(#tbl == 0)
end)

-- Report Results --

runner.report()
