local runner = require("test_executive")
local console = require("console")

-- Check If Present --
if not core.UNITTEST then return end

-- Setup --

local atl06_dispatch = icesat2.ut_atl06()
local atl03_reader = icesat2.ut_atl03()

-- Unit Test --

print('\n------------------\nTest01\n------------------')
runner.check(atl06_dispatch:lsftest(), "Failed lsftest")

print('\n------------------\nTest02\n------------------')
runner.check(atl06_dispatch:sorttest(), "Failed sorttest")

-- Clean Up --

-- Report Results --

runner.report()

