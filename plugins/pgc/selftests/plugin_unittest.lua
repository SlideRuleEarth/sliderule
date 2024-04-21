local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --
if not pgc.UNITTEST then return end

-- Setup --

-- Unit Test --


print('\n------------------\nTest01 RasterSubset::rema-mosaic \n------------------')

local demType = "rema-mosaic"
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=50, zonal_stats=true}))
runner.check(dem ~= nil)
local ut = pgc.ut_subset(dem)
runner.check(ut ~= nil)
local status = ut:test()
runner.check(status, "Failed subset test")
ut = nil


print('\n------------------\nTest02 RasterSubset::rema-strips \n------------------')
demType = "rema-strips"
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=50, zonal_stats=true}))
runner.check(dem ~= nil)
ut = pgc.ut_subset(dem)
runner.check(ut ~= nil)
status = ut:test()
runner.check(status, "Failed subset test")
ut = nil


-- Clean Up --

-- Report Results --

runner.report()

