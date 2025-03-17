local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --

if not core.UNITTEST then
    print("Skipping pgc plugin self test")
    return
end

-- Setup --

-- Unit Test --

print('\n------------------\nTest01 RasterSubset::rema-mosaic \n------------------')

local demType = "rema-mosaic"
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=50, zonal_stats=true}))
runner.assert(dem ~= nil)
local ut = geo.ut_subset(dem)
runner.assert(ut ~= nil)
local status = ut:test()
runner.assert(status, "Failed subset test")
ut = nil


print('\n------------------\nTest02 RasterSubset::rema-strips \n------------------')
demType = "rema-strips"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=50, zonal_stats=true}))
runner.assert(dem ~= nil)
ut = geo.ut_subset(dem)
runner.assert(ut ~= nil)
status = ut:test()
runner.assert(status, "Failed subset test")
ut = nil


lon = -178.0
lat =   51.7
lon_incr = 0.0001
lat_incr = 0.0001
pointCount = 10

print('\n------------------\nTest03 RasterSampler::arcticdem-mosaic\n------------------')
demType = "arcticdem-mosaic"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=false}))
runner.assert(dem ~= nil)

ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.assert(status, "Failed sampling test")
ut = nil


pointCount = 2
lon_incr = 2.0
print('\n------------------\nTest04 RasterSampler::arcticdem-strips\n------------------')
demType = "arcticdem-strips"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=true, sort_by_index=true}))
runner.assert(dem ~= nil)

ut = geo.ut_sample(dem)
runner.assert(ut ~= nil)
status = ut:test(lon, lat, lon_incr, lat_incr, pointCount)
runner.assert(status, "Failed sampling test")
ut = nil


-- Clean Up --

-- Report Results --

runner.report()

