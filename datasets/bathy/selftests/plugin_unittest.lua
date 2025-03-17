local runner = require("test_executive")
local asset = require("asset")
local console = require("console")

-- Check If Present --

if not core.UNITTEST then
    print("Skipping bathy plugin self test")
    return
end

-- Setup --

local assets = asset.loaddir()
local ut_refraction = bathy.ut_refraction()

-- Unit Test --

print('\n------------------\nTest RI Water\n------------------')
runner.assert(ut_refraction:riwater(), "Failed ri water test")

print('\n------------------\nTest Refraction\n------------------')
local parms = bathy.parms({refraction={use_water_ri_mask=false}})
local refraction = bathy.refraction(parms)
runner.assert(ut_refraction:refraction(parms, refraction), "Failed refraction test")

-- Clean Up --

-- Report Results --

runner.report()

