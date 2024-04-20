local runner = require("test_executive")
local console = require("console")

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --
if not pgc.UNITTEST then return end

-- Setup --

ut = pgc.ut_subset()
runner.check(ut ~= nil, "Failed to create subset")

-- Unit Test --

local demType = "rema-mosaic"
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour"}))
runner.check(dem ~= nil)

print('\n------------------\nTest01\n------------------')
runner.check(ut:test(dem), "Failed subset test")


-- Clean Up --

-- Report Results --

runner.report()

