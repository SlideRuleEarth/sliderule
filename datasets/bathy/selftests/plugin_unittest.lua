local runner = require("test_executive")
local console = require("console")

-- Check If Present --

if not core.UNITTEST then return end

-- Setup --

local ut_ri_water_index = bathy.ut_riwater()

-- Unit Test --

print('\n------------------\nTest RI Water\n------------------')
runner.check(ut_ri_water_index:test(), "Failed test")

-- Clean Up --

-- Report Results --

runner.report()

