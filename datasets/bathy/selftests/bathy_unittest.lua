local runner = require("test_executive")
local asset = require("asset")

-- Requirements --

if (not core.UNITTEST) or (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local assets = asset.loaddir()
local ut_refraction = bathy.ut_refraction()

-- Self Test --

print('\n------------------\nTest RI Water\n------------------')
runner.assert(ut_refraction:riwater(), "Failed ri water test")

print('\n------------------\nTest Refraction\n------------------')
local parms = bathy.parms({refraction={use_water_ri_mask=false}})
local refraction = bathy.refraction(parms)
runner.assert(ut_refraction:refraction(parms, refraction), "Failed refraction test")

-- Clean Up --

-- Report Results --

runner.report()

