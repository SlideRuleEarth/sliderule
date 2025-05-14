local runner = require("test_executive")

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

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

